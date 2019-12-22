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

using Heating::Packet;
using Heating::printTime;
using Heating::printPacket;
using Heating::isPinAvailable;
using Heating::Config;
using Heating::Storage;
using Heating::TemperatureSensor;
using Heating::Domain::HeaterInfo;
using Heating::Domain::ZoneInfo;

#include "Heating/AcctuatorProcessor.h"

using Heating::AcctuatorProcessor;

using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedRadio;
using RadioEncrypted::IEncryptedMesh;
using RadioEncrypted::Entropy::AnalogSignalEntropy;
using RadioEncrypted::reconnect;

#include "html.h"
#include "helpers.h"

extern int freeRam ();

void(* resetFunc) (void) = 0;

const char CHANNEL_KEEP_ALIVE[] {"heating/master/keep-alive"};

int main()
{
    init();

    wdt_disable();

    // heater off
    pinMode(Config::PIN_HEATING, OUTPUT);
    digitalWrite(Config::PIN_HEATING, HIGH);

    Serial.begin(Config::SERIAL_RATE); 

    RF24 radio(Config::PIN_RADIO_CE, Config::PIN_RADIO_CSN);

    Acorn128 cipher;
    AnalogSignalEntropy entropyAdapter(A0, Config::ADDRESS_MASTER);
    Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
    EncryptedRadio encMesh (Config::ADDRESS_MASTER, radio, encryption);

    wdt_enable(WDTO_8S);

    Serial << F("Starting radio...") << endl;
    uint8_t retryRadio = 10;
    while (retryRadio-- > 0) {
        if (!radio.begin()) {
            Serial << F("Failed to initialize radio") << endl;
        } else if (radio.isChipConnected()) {
            Serial << F("Connected.") << endl;
            break;
        }
    }
    if (retryRadio == 0) {
        resetFunc();
    }

    const uint8_t address[] = {Config::ADDRESS_MASTER, 0, 0, 0, 0, 0};
    radio.openReadingPipe(1,address);
    radio.setChannel(RADIO_CHANNEL);
    radio.setDataRate(RF24_250KBPS);
    radio.setPALevel(RF24_PA_MAX);
    radio.startListening();

    wdt_reset();

    if (Ethernet.begin(Config::MASTER_MAC) == 0) {
        Serial.println(F("Failed to obtain address"));
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
    Config config {};
    HeaterInfo heaterInfo {initTime};
    storage.loadConfiguration(config);
    AcctuatorProcessor processor(config);

#ifdef OWN_TEMPERATURE_SENSOR

    OneWire oneWire(Config::PIN_TEMPERATURE); 
    DallasTemperature sensors(&oneWire);
    TemperatureSensor sensor(sensors);

#endif

    EthernetClient net;

    PubSubClient mqttClient(net);
    mqttClient.setServer(MQTT_SERVER_ADDRESS, 1883);

    reconnectToMqtt(mqttClient);

    unsigned long handleTime = 0;
    unsigned long timeUpdate = 0;
    unsigned long lastRadioReceived = 0;
    uint8_t networkFailures = 0;
    uint8_t failureToApplyStates = 0;
    uint8_t publishFailed = 0;
    uint8_t reconnectMqttFailed = 0;
    uint8_t dhcpFailures = 0;

    auto retrieveCallback = [&mqttClient, &encMesh, &processor, &server, &heaterInfo, &networkFailures, &storage, &lastRadioReceived, &config]() {
        mqttClient.loop();

        if (handleRadio(encMesh, processor)) {
            lastRadioReceived = millis();
        }
        EthernetClient client = server.available();
        bool configUpdated = handleRequest(client, storage, processor, heaterInfo, networkFailures);
        if (configUpdated) {
            storage.loadConfiguration(config);
        }
    };

    while(true) {

        retrieveCallback();

        if (millis() - handleTime >= 60000UL) {

            wdt_reset();

#ifdef OWN_TEMPERATURE_SENSOR
            Packet packet { OWN_TEMPERATURE_SENSOR, sensor.read(), 0};
            wdt_reset();
            processor.handlePacket(packet);
#endif

            wdt_reset();

            processor.handleStates();

            handleHeater(processor, heaterInfo);

            wdt_reset();

            if (!heaterInfo.isShutingDown(config.heaterPumpStopTime)) {
                for (auto &state: processor.getStates()) {
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

            if (reconnectToMqtt(mqttClient) != ConnectionStatus::Connected) {
                reconnectMqttFailed++;
            } else {
                reconnectMqttFailed = 0;
            }

            char liveMsg[16] {0};
            sprintf(liveMsg, "%lu", millis());
		    if (!mqttClient.publish(CHANNEL_KEEP_ALIVE, liveMsg)) {
                Serial << F("Failed to send keep alive") << endl;
                publishFailed++;
		    } else {
                publishFailed = 0;
            }

            Serial.print(F("Memory left:"));
            Serial.println(freeRam());
            handleTime = millis();

            if (!maintainDhcp()) {
                dhcpFailures++;
            } else {
                dhcpFailures = 0;
            }

            bool radioFailure = millis() - lastRadioReceived > 600000UL;

            if (networkFailures > 20 || dhcpFailures > 20 || radioFailure) {
                Serial << F("Resetting") << endl;
                Serial.flush();
                resetFunc();
            }
        }

        if (millis() - timeUpdate >= 600000UL) {
            timeClient.update();
            syncTime(timeClient);
            timeUpdate = millis();

        }

        wdt_reset();
    }
    return 0;
} 
