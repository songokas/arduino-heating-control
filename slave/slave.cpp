#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include <RF24Mesh.h>
#include <Crypto.h>
#include <CryptoLW.h>
#include <Acorn128.h>
#include <Streaming.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <Time.h>
#include <avr/wdt.h>
#include "Heating/Common.h"
#include "Heating/Config.h"
#include "Heating/TemperatureSensor.h"

#include "CommonModule/MacroHelper.h"
#include "RadioEncrypted/RadioEncryptedConfig.h"
#include "RadioEncrypted/Encryption.h"
#include "RadioEncrypted/EncryptedMesh.h"
#include "RadioEncrypted/Entropy/AnalogSignalEntropy.h"
#include "RadioEncrypted/Helpers.h"

using Heating::Config;
using Heating::ControllPacket;
using Heating::Packet;
using Heating::printPacket;
using Heating::isPinAvailable;
using Heating::TemperatureSensor;

using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedMesh;
using RadioEncrypted::IEncryptedMesh;
using RadioEncrypted::Entropy::AnalogSignalEntropy;
using RadioEncrypted::reconnect;

#include "helpers.h"

void(* resetFunc) (void) = 0;

int main()
{
    init();

    Serial.begin(Config::SERIAL_RATE); 
    RF24 radio(Config::PIN_RADIO_CE, Config::PIN_RADIO_CSN);
    RF24Network network(radio);
    RF24Mesh mesh(radio, network);

    Acorn128 cipher;
    AnalogSignalEntropy entropyAdapter(A0, Config::ADDRESS_SLAVE);
    Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
    EncryptedMesh encMesh (mesh, network, encryption);
    mesh.setNodeID(Config::ADDRESS_SLAVE);

    Handler handler {};

    OneWire oneWire(Config::PIN_TEMPERATURE); 
    DallasTemperature sensors(&oneWire);
    TemperatureSensor sensor(sensors);

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

    // 1 minute
    unsigned long TIME_PERIOD = 60000UL;
    unsigned long startTime = millis();
    uint8_t networkFailures = 10;
    // for some reason d10 on by default
    digitalWrite(10, LOW);
    while(true) {

        mesh.update();

        if (encMesh.isAvailable()) {
            wdt_reset();
            ControllPacket received;
            RF24NetworkHeader header;
            if (!encMesh.receive(&received, sizeof(received), 0, header, Config::ADDRESS_MASTER)) {
                Serial.println(F("Failed to receive for slave"));
                Serial.println(received.id);
                continue;
            }
            
            Serial.print(F("Packet received for: "));
            Serial.println(received.id);
            
            wdt_reset();

            if (isPinAvailable(received.id, Config::AVAILABLE_PINS_NANO, sizeof(Config::AVAILABLE_PINS_NANO)/sizeof(Config::AVAILABLE_PINS_NANO[0]))) {
                pinMode(received.id, OUTPUT);
                analogWrite(received.id, received.state);
                handler.addTimeout(received.id);
            } else {
                Serial.println(F("Ignoring since pin is not available"));
            }
        }
        unsigned long currentTime = millis();
        if (currentTime - startTime > TIME_PERIOD) {
            wdt_reset();
            //handleInnerTemperature(sensor, radio);
            handler.handleTimeouts();
            startTime = currentTime;
            if (!reconnect(mesh)) {
                networkFailures++;
            } else {
                networkFailures = 0;
            }
            Serial.println(F("Ping"));
        }

        if (networkFailures > 10) {
           resetFunc(); 
        }

        wdt_reset();
    }
    return 0;
} 
