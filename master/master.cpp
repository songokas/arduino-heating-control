#include <Arduino.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <RF24Network.h>
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

#ifdef NTP_TIME
#include <NTPClient.h>
#else
#include <ThreeWire.h>  
#include <RtcDS1302.h>
//#include <RtcDS3231.h>
#endif

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

#include "Heating/Common.h"
#include "Heating/Storage.h"
#include "Heating/TemperatureSensor.h"
#include "Heating/Domain/HeaterInfo.h"
#include "Heating/Domain/ZoneInfo.h"

#include "CommonModule/MacroHelper.h"
#include "MqttModule/MqttConfig.h"
#include "RadioEncrypted/RadioEncryptedConfig.h"
#include "RadioEncrypted/Encryption.h"
#include "RadioEncrypted/EncryptedRadio.h"
#include "RadioEncrypted/Entropy/AnalogSignalEntropy.h"
#include "RadioEncrypted/Helpers.h"

using Heating::Packet;
using Heating::ControllPacket;
using Heating::Error;
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
using RadioEncrypted::resetWatchDog;

const char HEATING_TOPIC [] PROGMEM {"heating/nodes/%s/temperature"};
const char HEATING_ZIGBEE_TOPIC [] PROGMEM {"zigbee2mqtt/heating/nodes/%s"};
const char * SUBSCRIBE_TOPIC = "heating/nodes/#";
const char * SUBSCRIBE_ZIGBEE_TOPIC = "zigbee2mqtt/heating/nodes/#";
const char CHANNEL_SLAVE_CONTROL[] PROGMEM {"cmnd/heating-slave/pwm%d"};
const char CHANNEL_SLAVE_CONFIRM[] PROGMEM {"stat/heating-slave/result"};
const char INFO_NODE_TOPIC [] PROGMEM {"stat/heating/nodes/%s"};
const char INFO_NODE_MESSAGE [] PROGMEM {"{\"temperature\":%0.2f,\"power\":%d}"};
const char INFO_MASTER_TOPIC [] PROGMEM {"stat/heating/master/info"};
const char INFO_MASTER_MESSAGE [] PROGMEM {"{\"power\":%d}"};
const char NTP_MASTER_TOPIC [] PROGMEM {"stat/heating/master/ntp"};
const char NTP_MASTER_MESSAGE [] PROGMEM {"{\"updated\":%s,\"elapsed\":%lu}"};
const char * SUBSCRIBE_TOPICS[] {SUBSCRIBE_TOPIC, SUBSCRIBE_ZIGBEE_TOPIC, CHANNEL_SLAVE_CONFIRM};


// mqtt is using c style callback and we require these
StaticConfig<Config::MAX_ZONES, Config::MAX_ZONE_NAME_LENGTH, Config::MAX_TIMES_PER_ZONE> config {};
StaticAcctuatorProcessor<Config::MAX_ZONES, Config::MAX_ZONE_TEMPS, Config::MAX_ZONE_STATE_HISTORY, Config::MAX_ZONE_ERRORS> processor(config);

extern int freeRam ();

void(* resetFunc) (void) = 0;

#include "html.h"
#include "helpers.h"

int main()
{
    init();

    wdt_disable();

    uint8_t wdtTime = WDTO_8S;

    // spi
    //pinMode(4,OUTPUT);
    //digitalWrite(4,HIGH);

    // heater off
    pinMode(Config::PIN_HEATING, OUTPUT);
    digitalWrite(Config::PIN_HEATING, HIGH);

    Serial.begin(Config::SERIAL_RATE);

    Serial << F("Begin memory left:") << freeRam() << endl;

    RF24 radio(Config::PIN_RADIO_CE, Config::PIN_RADIO_CSN);

    Acorn128 cipher;
    AnalogSignalEntropy entropyAdapter(A0, Config::ADDRESS_MASTER);
    Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
    EncryptedRadio encMesh (Config::ADDRESS_MASTER, radio, encryption);

    wdt_enable(wdtTime);

    connectToRadio(radio);

    resetWatchDog();

    if (Ethernet.begin(Config::MASTER_MAC) == 0) {
        Serial << F("Failed to obtain dhcp address") << endl;
        Ethernet.begin(Config::MASTER_MAC, Config::MASTER_IP);
    }
    delay(1000);

    Serial.print(F("server is at "));
    Serial.println(Ethernet.localIP());

    EthernetServer server(80);
    server.begin();

    updateTime();
    resetWatchDog();

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
    mqttClient.setServer(MQTT_SERVER_ADDRESS, 1883);

    unsigned long handleTime = 0;
    unsigned long timeUpdate = 0;
    unsigned long lastRadioReceived = 0;
    uint8_t networkFailures = 0;
    uint8_t publishFailed = 0;
    uint8_t reconnectMqttFailed = 0;

    Serial << F("Wait for mqtt server on ") << MQTT_SERVER_ADDRESS << endl;

    reconnectMqttFailed = connectToMqtt(mqttClient, MQTT_CLIENT_NAME, SUBSCRIBE_TOPICS, COUNT_OF(SUBSCRIBE_TOPICS)) ? 0 : 1;

    mqttClient.setCallback(mqttCallback);

    auto retrieveCallback = [&mqttClient, &encMesh, &server, &heaterInfo, &networkFailures, &storage, &lastRadioReceived]() {
        mqttClient.loop();

        resetWatchDog();
        if (handleRadio(encMesh, processor)) {
            lastRadioReceived = millis();
        }
        EthernetClient client = server.available();
        bool configUpdated = handleRequest(client, storage, processor, heaterInfo, networkFailures, config);
        resetWatchDog();
        if (configUpdated) {
            storage.loadConfiguration(config);
        }
    };

    while(true) {

        retrieveCallback();

        unsigned long timeToSend = random(1000, 10000) + 60000UL;

        if (millis() - handleTime >= timeToSend) {

            Serial << F("Time to send") << endl;

            resetWatchDog();

#ifdef OWN_TEMPERATURE_SENSOR
            Packet packet { OWN_TEMPERATURE_SENSOR, (int16_t)(100 * sensor.read()), 0};
            resetWatchDog();
            processor.handlePacket(packet);
#endif

            resetWatchDog();

            processor.handleStates();

            handleHeater(processor, heaterInfo, mqttClient);

            resetWatchDog();

            if (!heaterInfo.isShutingDown(config.heaterPumpStopTime)) {

                for (uint8_t i = 0; i < processor.getStateArrLength(); i++) {
                    auto & zoneInfo = processor.getState(i);
                    if (!(zoneInfo.pin.id > 0)) {
                        continue;
                    }
                    retrieveCallback();
                    handleAcctuator(mqttClient, encMesh, zoneInfo);
                    resetWatchDog();
                }
            }

            resetWatchDog();

            if (!sendKeepAlive(mqttClient, CHANNEL_KEEP_ALIVE, millis())) {
                publishFailed++;
                reconnectMqttFailed = connectToMqtt(mqttClient, MQTT_CLIENT_NAME, SUBSCRIBE_TOPICS, COUNT_OF(SUBSCRIBE_TOPICS))
                    ? 0 : reconnectMqttFailed + 1;
            } else {
                publishFailed = 0;
            }

            resetWatchDog();

            if (!maintainDhcp()) {
                networkFailures++;
            } else {
                networkFailures = 0;
            }

            resetWatchDog();

            if (radio.failureDetected) {
                radio.failureDetected = 0;
                connectToRadio(radio);
            }

            checkSockStatus(server);

            resetWatchDog();

            Serial << F("Memory left:") << freeRam() << endl;

            handleTime = millis();
        }

        if (millis() - timeUpdate >= 3600000UL) {
            TimeOperation timeUpdated = updateTime();
            resetWatchDog();
            if (!sendNtp(mqttClient, timeUpdated)) {
                Serial << F("Failed to send ntp message") << endl;
            }
            timeUpdate = millis();
        }

        resetWatchDog();
    }
    return 0;
} 
