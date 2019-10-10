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
#include <Entropy.h>
#include <Streaming.h>
#include <ArduinoJson.h>
#include <Ethernet.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <Time.h>

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
#include "TimeService/TimeService.h"
#include "Heating/TemperatureSensor.h"
#include "Heating/Domain/HeaterInfo.h"
#include "Heating/Domain/ZoneInfo.h"

#include "CommonModule/MacroHelper.h"
#include "RadioEncrypted/RadioEncryptedConfig.h"
#include "RadioEncrypted/Encryption.h"
#include "RadioEncrypted/EncryptedMesh.h"
#include "RadioEncrypted/Entropy/AvrEntropyAdapter.h"

using Heating::Packet;
using Heating::printTime;
using Heating::printPacket;
using Heating::isPinAvailable;
using Heating::Config;
using Heating::Storage;
using TimeService::getTime;
using Heating::TemperatureSensor;
using Heating::Domain::HeaterInfo;
using Heating::Domain::ZoneInfo;

#include "Heating/AcctuatorProcessor.h"

using Heating::AcctuatorProcessor;

using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedMesh;
using RadioEncrypted::IEncryptedMesh;
using RadioEncrypted::Entropy::AvrEntropyAdapter;

// defines pin id for master
//#define ID 34
//#define CURRENT_TIME 1520772798
#define TIME_SYNC_INTERVAL 3600

extern int freeRam ();

#include "html.h"
#include "helpers.h"

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
    EntropyClass entropy;
    entropy.initialize();
    AvrEntropyAdapter entropyAdapter(entropy);
    Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
    EncryptedMesh encMesh (mesh, network, encryption);
    mesh.setNodeID(Config::ADDRESS_MASTER);
    // Connect to the mesh
    Serial << F("Connecting to the mesh...") << endl;
    if (!mesh.begin(RADIO_CHANNEL, RF24_250KBPS, MESH_TIMEOUT)) {
        Serial << F("Failed to connect to mesh") << endl;
    } else {
        Serial << F("Connected.") << endl;
    }
    radio.setPALevel(RF24_PA_LOW);

    if (Ethernet.begin(Config::MASTER_MAC) == 0) {
        Serial.println(F("Failed to obtain address"));
        Ethernet.begin(Config::MASTER_MAC, Config::MASTER_IP);
    }
    delay(1000);

    Serial.print(F("server is at "));
    Serial.println(Ethernet.localIP());

    EthernetServer server(80);
    server.begin();

    wdt_enable(WDTO_8S);

    setSyncProvider(getTime);
    setSyncInterval(TIME_SYNC_INTERVAL);

    if (timeStatus() == timeNotSet) {
        Serial.print(F("Faile to set time using:"));
        Serial.println(CURRENT_TIME);
        setTime(CURRENT_TIME);
    }

    time_t initTime = now();
    Serial.print(F("current time: "));
    printTime(initTime);
    Serial.println();

    Storage storage;
    Config config {};
    HeaterInfo heaterInfo {initTime};
    storage.loadConfiguration(config);
    AcctuatorProcessor processor(config);

    OneWire oneWire(Config::PIN_TEMPERATURE); 
    DallasTemperature sensors(&oneWire);
    TemperatureSensor sensor(sensors);

    // 1 minute
    unsigned long TIME_PERIOD = 60000UL;
    unsigned long startTime = millis();

    while(true) {


        handleRadio(encMesh, processor);
        EthernetClient client = server.available();
        handleRequest(client, storage, processor, config, heaterInfo);

        unsigned long currentTime = millis();

        if (currentTime - startTime >= TIME_PERIOD) {

            wdt_reset();

            //Serial.println(F("Read sensor "));
            //Serial.println(millis());

            Packet packet { Config::ADDRESS_MASTER, sensor.read(), 0};

            wdt_reset();

            //Serial.print(F("Handle packet "));
            //Serial.println(millis());


            processor.handlePacket(packet);

            //Serial.print(F("Handle states "));
            //Serial.println(millis());

            wdt_reset();

            processor.handleStates();

            //Serial.print(F("Handle heater "));
            //Serial.println(millis());

            handleHeater(processor, heaterInfo);

            wdt_reset();

            //Serial.print(F("Apply states heater "));
            //Serial.println(millis());

            processor.applyStates(encMesh, heaterInfo);

            //Serial.print(F("All sessions "));
            //Serial.println(millis());

            startTime = currentTime;

        }

        wdt_reset();
        maintainDhcp();
    }
    return 0;
} 
