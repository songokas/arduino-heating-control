#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include <RF24Mesh.h>
#include <Crypto.h>
#include <CryptoLW.h>
#include <Acorn128.h>
#include <Entropy.h>
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
#include "RadioEncrypted/Entropy/AvrEntropyAdapter.h"

using Heating::Config;
using Heating::ControllPacket;
using Heating::Packet;
using Heating::printPacket;
using Heating::isPinAvailable;
using Heating::TemperatureSensor;

using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedMesh;
using RadioEncrypted::IEncryptedMesh;
using RadioEncrypted::Entropy::AvrEntropyAdapter;

#include "helpers.h"

int main()
{
    init();

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
    radio.setPALevel(RF24_PA_LOW);

    wdt_reset();

    // 1 minute
    unsigned long TIME_PERIOD = 60000UL;
    unsigned long startTime = millis();
    while(true) {

        if (encMesh.isAvailable()) {
            wdt_reset();
            ControllPacket received;
            RF24NetworkHeader header;
            if (!encMesh.receive(&received, sizeof(received), 0, header)) {
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
                Serial.println("Ignoring since pin is not available");
            }
        }
        unsigned long currentTime = millis();
        if (currentTime - startTime > TIME_PERIOD) {
            wdt_reset();
            //handleInnerTemperature(sensor, radio);
            handler.handleTimeouts();
            startTime = currentTime;
        }

        wdt_reset();
    }
    return 0;
} 
