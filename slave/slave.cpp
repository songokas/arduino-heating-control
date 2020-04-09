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
#include "RadioEncrypted/EncryptedRadio.h"
#include "RadioEncrypted/Entropy/AnalogSignalEntropy.h"
#include "RadioEncrypted/Helpers.h"

using Heating::Config;
using Heating::ControllPacket;
using Heating::Packet;
using Heating::printPacket;
using Heating::isPinAvailable;
using Heating::TemperatureSensor;

using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedRadio;
using RadioEncrypted::IEncryptedMesh;
using RadioEncrypted::Entropy::AnalogSignalEntropy;
using RadioEncrypted::reconnect;

#include "helpers.h"

void(* resetFunc) (void) = 0;

int main()
{
    init();

    // for some reason d10 on by default
    digitalWrite(10, LOW);

    Serial.begin(Config::SERIAL_RATE);

    Handler handler {};

    OneWire oneWire(Config::PIN_TEMPERATURE); 
    DallasTemperature sensors(&oneWire);
    TemperatureSensor sensor(sensors);

    RF24 radio(Config::PIN_RADIO_CE, Config::PIN_RADIO_CSN);

    Acorn128 cipher;
    AnalogSignalEntropy entropyAdapter(A0, Config::ADDRESS_SLAVE);
    Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
    EncryptedRadio encMesh (Config::ADDRESS_SLAVE, radio, encryption);

    wdt_enable(WDTO_8S);

    uint8_t radioFailures = connectToRadio(radio) ? 0 : 1;

    wdt_reset();

    unsigned long lastReceived = 0;
    unsigned long lastLoop = millis();
;
    while(true) {

        if (encMesh.isAvailable()) {
            wdt_reset();
            ControllPacket received;
            RF24NetworkHeader header;
            if (!encMesh.receive(&received, sizeof(received), 0, header, Config::ADDRESS_MASTER)) {
                Serial.println(F("Failed to receive for slave"));
                Serial.println(received.id);
                continue;
            } else {
                lastReceived = millis();
            }

            if (header.to_node != encMesh.getNodeId()) {
                Serial << F("Ignoring forward to node: ") << header.to_node << endl;
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
        if (millis() - lastLoop > 60000UL) {
            wdt_reset();
            handler.handleTimeouts();
            lastLoop = millis();
            if (radio.failureDetected) {
                radio.failureDetected = 0;
                radioFailures = connectToRadio(radio) ? 0 : radioFailures + 1;
            }
            if (millis() - lastReceived > 3600000UL) {
                radioFailures = connectToRadio(radio) ? 0 : radioFailures + 1;
            }
            if (radioFailures > 10) {
                resetFunc();
                return 1;
            }
            Serial.println(F("Ping"));

        }


        wdt_reset();
    }
    return 0;
} 
