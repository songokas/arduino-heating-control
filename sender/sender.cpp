#include <Arduino.h>
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
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <LowPower.h>
#include <Time.h>

#include "Heating/Common.h"
#include "Heating/Config.h"
#include "Heating/TemperatureSensor.h"

#include "CommonModule/MacroHelper.h"
#include "RadioEncrypted/RadioEncryptedConfig.h"
#include "RadioEncrypted/Encryption.h"
#include "RadioEncrypted/EncryptedMesh.h"
#include "RadioEncrypted/Entropy/AvrEntropyAdapter.h"

using Heating::Packet;
using Heating::Config;
using Heating::TemperatureSensor;
using Heating::printPacket;
using Heating::timer;

using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedMesh;
using RadioEncrypted::IEncryptedMesh;
using RadioEncrypted::Entropy::AvrEntropyAdapter;

volatile byte notify = 0;

void blinkLed(byte times)
{
    Serial.print(F("Notify:"));
    Serial.println(times);
    while (times > 0) {
        digitalWrite(Config::PIN_LED, HIGH);
        delay(1000);
        digitalWrite(Config::PIN_LED, LOW);
        delay(1000);
        times--;
    }
}


void setExpected() {
    notify++;
}

int main()
{
    init();

    Serial.begin(Config::SERIAL_RATE);
    pinMode(Config::PIN_LED, OUTPUT);
    pinMode(Config::PIN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Config::PIN_BUTTON), setExpected, FALLING);

    RF24 radio(Config::PIN_RADIO_CE, Config::PIN_RADIO_CSN);
    RF24Network network(radio);
    RF24Mesh mesh(radio, network);

    Acorn128 cipher;
    EntropyClass entropy;
    entropy.initialize();
    AvrEntropyAdapter entropyAdapter(entropy);
    Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
    EncryptedMesh encMesh (mesh, network, encryption);
    mesh.setNodeID(NODE_ID);
    // Connect to the mesh
    Serial << F("Connecting to the mesh...") << endl;
    if (!mesh.begin(RADIO_CHANNEL, RF24_250KBPS, MESH_TIMEOUT)) {
        Serial << F("Failed to connect to mesh") << endl;
    } else {
        Serial << F("Connected.") << endl;
    }
    radio.setPALevel(RF24_PA_LOW);

    OneWire oneWire(Config::PIN_TEMPERATURE); 
    DallasTemperature sensors(&oneWire);
    TemperatureSensor sensor(sensors);

    Packet packet { NODE_ID, 0, 0};

    // in case interupt runs
    notify = 0;
    unsigned long timeExpectedRaised = 0;
    byte waitLoop = 0;
    byte buttonPressed = 0;
    float expectedTemperature = 0;

    while(true) {

        if (buttonPressed > 3) {
            if (expectedTemperature) {
                Serial.println(F("Notify reset raised temperature"));
                expectedTemperature = 0;
            } else {
                Serial.println(F("Notify raise temperature"));
                expectedTemperature = Config::RAISED_TEMPERATURE;
                timeExpectedRaised = timer();
            }
            buttonPressed = 0;
            waitLoop = 0;
        } else if (expectedTemperature > 0) {
            if (timer() - timeExpectedRaised > Config::MAX_DELAY) {
                Serial.println(F("Expected temperature stop"));
                expectedTemperature = 0;
                // since timer is not very precise reset
                timer(0, true);
            }
        }
        packet.currentTemperature = sensor.read();
        packet.expectedTemperature = expectedTemperature;
        if (!encMesh.send(&packet, sizeof(packet), 0, Config::ADDRESS_MASTER)) {
            Serial.println(F("Failed to send packet to master"));
        }

        printPacket(packet);
        Serial.flush();
        radio.powerDown();

        uint8_t timeToSleep = 7;
        uint32_t increaseTimer = timeToSleep * 8700;
        while (timeToSleep-- > 0) {
            LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
        }

        timer(increaseTimer);

        radio.powerUp();

        if (notify > 0) {
            buttonPressed++;
            notify = 0;
        } else if (buttonPressed > 0) {
            if (waitLoop > 3) {
                buttonPressed = 0;
                waitLoop = 0;
            } else {
                waitLoop++;
            }
        }

    }
    return 0;
}


