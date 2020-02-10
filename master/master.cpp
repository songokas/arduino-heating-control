#include <Arduino.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Crypto.h>
#include <CryptoLW.h>
#include <Acorn128.h>
#include <Streaming.h>
#include <ArduinoJson.h>
#include <Ethernet.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <Time.h>
#include <PubSubClient.h>
#include <NTPClient.h>

// required by arduino-mk to include it when compiling storage
#include <Wire.h>
#include <AuthenticatedCipher.h>
#include <Cipher.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Timezone.h>
#include "Heating/Common.h"
#include "Heating/Config.h"
// arduino-mk

#include "Heating/Storage.h"
#include "Heating/TemperatureSensor.h"
#include "Heating/Domain/HeaterInfo.h"
#include "Heating/Domain/ZoneInfo.h"

#include "CommonModule/MacroHelper.h"
#include "RadioEncrypted/RadioEncryptedConfig.h"
#include "RadioEncrypted/Encryption.h"
#include "RadioEncrypted/EncryptedRadio.h"
#include "RadioEncrypted/Entropy/AnalogSignalEntropy.h"
#include "RadioEncrypted/Helpers.h"
#include "MqttModule/MqttConfig.h"

using Heating::Packet;
using Heating::printTime;
using Heating::printPacket;
using Heating::isPinAvailable;
using Heating::Config;
using Heating::StaticConfig;
using Heating::Storage;
using Heating::TemperatureSensor;
using Heating::Domain::HeaterInfo;
using Heating::Domain::StaticHeaterInfo;
using Heating::Domain::ZoneInfo;

#include "Heating/AcctuatorProcessor.h"

using Heating::AcctuatorProcessor;
using Heating::StaticAcctuatorProcessor;

using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedRadio;
using RadioEncrypted::IEncryptedMesh;
using RadioEncrypted::Entropy::AnalogSignalEntropy;
using RadioEncrypted::reconnect;

const char HEATING_TOPIC [] PROGMEM {"heating/nodes/%s/temperature"};
// mqtt is using c style callback and we require these
StaticConfig<Config::MAX_ZONES, Config::MAX_ZONE_NAME_LENGTH, Config::MAX_TIMES_PER_ZONE> config {};
StaticAcctuatorProcessor<Config::MAX_ZONES, Config::MAX_ZONE_TEMPS, Config::MAX_ZONE_STATE_HISTORY, Config::MAX_ZONE_ERRORS> processor(config);

#include "html.h"
#include "helpers.h"

extern int freeRam ();

void(* resetFunc) (void) = 0;

int main()
{
    init();

    wdt_disable();

    uint8_t wdtTime = WDTO_8S;

    // heater off
    pinMode(Config::PIN_HEATING, OUTPUT);
    digitalWrite(Config::PIN_HEATING, HIGH);

    Serial.begin(Config::SERIAL_RATE); 

    RF24 radio(Config::PIN_RADIO_CE, Config::PIN_RADIO_CSN);

    Acorn128 cipher;
    AnalogSignalEntropy entropyAdapter(A0, Config::ADDRESS_MASTER);
    Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
    EncryptedRadio encMesh (Config::ADDRESS_MASTER, radio, encryption);

    wdt_enable(wdtTime);

    connectToRadio(radio);

    wdt_reset();

    if (Ethernet.begin(Config::MASTER_MAC) == 0) {
        Serial << F("Failed to obtain dhcp address") << endl;
        Ethernet.begin(Config::MASTER_MAC, Config::MASTER_IP);
    }
    delay(1000);

    Serial.print(F("server is at "));
    Serial.println(Ethernet.localIP());

    EthernetServer server(80);
    server.begin();

    wdt_reset();

    EthernetUDP udpConnection;
    NTPClient timeClient(udpConnection);

    timeClient.begin();
    timeClient.update();
    syncTime(timeClient);

    time_t initTime = now();
    Serial.print(F("current time: "));
    printTime(initTime);
    Serial.println();

    Storage storage {};
    StaticHeaterInfo<Config::MAX_HEATER_HISTORY> heaterInfo {initTime};
    storage.loadConfiguration(config);

#ifdef OWN_TEMPERATURE_SENSOR

    OneWire oneWire(Config::PIN_TEMPERATURE); 
    DallasTemperature sensors(&oneWire);
    TemperatureSensor sensor(sensors);

#endif

    EthernetClient net;

    PubSubClient mqttClient(net);

    Serial << F("Wait for mqtt server on ") << MQTT_SERVER_ADDRESS << endl;

    uint16_t mqttAttempts = 0;
    mqttClient.setServer(MQTT_SERVER_ADDRESS, 1883);
    while (!mqttClient.connect(NODE_NAME) && mqttAttempts < 10) {
        mqttAttempts++;
        Serial.print(".");
        delay(500 * mqttAttempts);
        wdt_reset();
    }
    if (mqttAttempts >= 10) {
        Serial << F("failed to connect.") << endl;
    } else {
        Serial << F("connected.") << endl;
    }
    mqttClient.setCallback(mqttCallback);

    unsigned long handleTime = 0;
    unsigned long timeUpdate = 0;
    unsigned long lastRadioReceived = 0;
    uint8_t networkFailures = 0;
    uint8_t failureToApplyStates = 0;
    uint8_t publishFailed = 0;
    uint8_t reconnectMqttFailed = 0;

    auto retrieveCallback = [&mqttClient, &encMesh, &server, &heaterInfo, &networkFailures, &storage, &lastRadioReceived]() {
        mqttClient.loop();

        wdt_reset();
        if (handleRadio(encMesh, processor)) {
            lastRadioReceived = millis();
        }
        EthernetClient client = server.available();
        bool configUpdated = handleRequest(client, storage, processor, heaterInfo, networkFailures, config);
        wdt_reset();
        if (configUpdated) {
            storage.loadConfiguration(config);
        }
    };

    while(true) {

        retrieveCallback();

        unsigned long timeToSend = random(1000, 10000) + 60000UL;

        if (millis() - handleTime >= timeToSend) {

            Serial << F("Time to send") << endl;

            wdt_reset();

#ifdef OWN_TEMPERATURE_SENSOR
            Packet packet { OWN_TEMPERATURE_SENSOR, 100 * sensor.read(), 0};
            wdt_reset();
            processor.handlePacket(packet);
#endif

            wdt_reset();

            processor.handleStates();

            handleHeater(processor, heaterInfo);

            wdt_reset();

            if (!heaterInfo.isShutingDown(config.heaterPumpStopTime)) {
                for (uint8_t i = 0; i < processor.getStateArrLength(); i++) {
                    auto & state = processor.getState(i);
                    if (!(state.pin.id > 0)) {
                        continue;
                    }

                    wdt_reset();
                    if (!processor.applyState(state, encMesh, heaterInfo)) {
                        failureToApplyStates++;
                    } else {
                        failureToApplyStates = 0;
                    }
                    wdt_reset();

                    retrieveCallback();
                }
            }

            wdt_disable();
            timeClient.update();
            syncTime(timeClient);
            timeUpdate = millis();
            wdt_enable(wdtTime);

            char liveMsg[30] {0};
            sprintf(liveMsg, "%lu", millis());
		    if (!mqttClient.publish(CHANNEL_KEEP_ALIVE, liveMsg)) {
                Serial << F("Failed to send keep alive") << endl;
                if (!connectToMqtt(NODE_NAME, mqttClient)) {
                    reconnectMqttFailed++;
                } else {
                    reconnectMqttFailed = 0;
                }
                publishFailed++;
		    } else {
                publishFailed = 0;
            }

            Serial.print(F("Memory left:"));
            Serial.println(freeRam());
            handleTime = millis();

            if (!maintainDhcp()) {
                networkFailures++;
            } else {
                networkFailures = 0;
            }

            if (radio.failureDetected) {
                radio.failureDetected = 0;
                connectToRadio(radio);
            }

            bool receiveFailure = millis() - lastRadioReceived > 1800000UL;

            if (receiveFailure) {
                Serial << F("Resetting") << endl;
                Serial.flush();
                radio.powerDown();
                delay(200);
                resetFunc();
                return 1;
            }
        }

        wdt_reset();
    }
    return 0;
} 
