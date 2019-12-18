#include <Arduino.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <RF24Network.h>
#include <RF24Mesh.h>
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
#include "RadioEncrypted/EncryptedMesh.h"
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
using RadioEncrypted::EncryptedMesh;
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
    RF24Network network(radio);
    RF24Mesh mesh(radio, network);

    Acorn128 cipher;
    AnalogSignalEntropy entropyAdapter(A0, Config::ADDRESS_MASTER);
    Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
    EncryptedMesh encMesh (mesh, network, encryption);
    mesh.setNodeID(Config::ADDRESS_MASTER);

    wdt_enable(WDTO_8S);

    // Connect to the mesh
    Serial << F("Connecting to the mesh...") << endl;
    if (!mesh.begin(RADIO_CHANNEL, RF24_250KBPS, MESH_TIMEOUT)) {
        Serial << F("Failed to connect to mesh") << endl;
    } else {
        Serial << F("Connected.") << endl;
    }
    radio.setPALevel(RF24_PA_MAX);

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
    uint8_t networkFailures = 0;
    uint8_t failureToApplyStates = 0;
    uint8_t publishFailed = 0;
    uint8_t reconnectMqttFailed = 0;
    uint8_t dhcpFailures = 0;

    while(true) {

        mqttClient.loop();
        mesh.update();
        if (Config::ADDRESS_MASTER == 0) {
            mesh.DHCP();
        }

        handleRadio(encMesh, processor);
        EthernetClient client = server.available();
        bool configUpdated = handleRequest(client, storage, processor, heaterInfo, networkFailures);
        if (configUpdated) {
            storage.loadConfiguration(config);
        }

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

                    wdt_reset();
                    if (!processor.applyState(state, encMesh, heaterInfo)) {
                        failureToApplyStates++;
                    } else {
                        failureToApplyStates = 0;
                    }
                    wdt_reset();

                    mqttClient.loop();
                    mesh.update();
                    if (Config::ADDRESS_MASTER == 0) {
                        mesh.DHCP();
                    }

                    handleRadio(encMesh, processor);
                    EthernetClient client = server.available();
                    bool configUpdated = handleRequest(client, storage, processor, heaterInfo, networkFailures);
                    if (configUpdated) {
                        storage.loadConfiguration(config);
                    }
                }
            }

            if (Config::ADDRESS_MASTER != 0) {
                if (!reconnect(mesh)) {
                    networkFailures++;
                } else {
                    networkFailures = 0;
                }
            }

            if (reconnectToMqtt(mqttClient) != ConnectionStatus::Connected) {
                reconnectMqttFailed++;
            } else {
                reconnectMqttFailed = 0;
            }

            char liveMsg[16] {0};
            sprintf(liveMsg, "%lud", millis());
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


            Serial << F("Network ") << networkFailures << F(" Failure to send ") << failureToApplyStates << F(" Dhcp ") << dhcpFailures << F(" Radio ") << radio.failureDetected << endl;
            if (networkFailures > 20 || failureToApplyStates > 100 || dhcpFailures > 20/* || radio.failureDetected*/) {
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
        maintainDhcp();
    }
    return 0;
} 
