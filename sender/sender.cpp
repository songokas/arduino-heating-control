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
#include "RadioEncrypted/Entropy/AnalogSignalEntropy.h"
#include "RadioEncrypted/Helpers.h"

using Heating::Packet;
using Heating::Config;
using Heating::TemperatureSensor;
using Heating::printPacket;
using Heating::timer;

using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedMesh;
using RadioEncrypted::IEncryptedMesh;
using RadioEncrypted::Entropy::AnalogSignalEntropy;
using RadioEncrypted::reconnect;

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

void(* resetFunc) (void) = 0;

#ifdef TEST_RECEIVE

void testReceive(IEncryptedMesh & radio)
{
    if (radio.isAvailable()) {
        Packet received;
        RF24NetworkHeader header;
        if (!radio.receive(&received, sizeof(received), 0, header)) {
            Serial.println(F("Failed to receive packet on master"));
            printPacket(received);
            return;
        } else {
            Serial.print(F("Received: "));
            printPacket(received);
        }
    }
}

#endif


void setExpected() {
    notify++;
}

int main()
{
    init();

//    pinMode(LED_BUILTIN, OUTPUT);
//    digitalWrite(LED_BUILTIN, LOW);

    Serial.begin(Config::SERIAL_RATE);
    pinMode(Config::PIN_LED, OUTPUT);
    pinMode(Config::PIN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Config::PIN_BUTTON), setExpected, FALLING);

    RF24 radio(Config::PIN_RADIO_CE, Config::PIN_RADIO_CSN);
    RF24Network network(radio);
    RF24Mesh mesh(radio, network);

    Acorn128 cipher;
    AnalogSignalEntropy entropyAdapter(A0, NODE_ID);
    Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
    EncryptedMesh encMesh (mesh, network, encryption);
    mesh.setNodeID(NODE_ID);

    wdt_enable(WDTO_8S);

    // Connect to the mesh
    Serial << F("Connecting to the mesh...") << endl;
    if (!mesh.begin(RADIO_CHANNEL, RF24_250KBPS, MESH_TIMEOUT)) {
        Serial << F("Failed to connect to mesh") << endl;
    } else {
        Serial << F("Connected.") << endl;
    }

    radio.setPALevel(RF24_PA_HIGH);

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

    unsigned long timeLastSent = 0;
    uint8_t failures = 0;

    while(true) {

        mesh.update();
        wdt_reset();

#ifdef TEST_RECEIVE

        testReceive(encMesh);

        if (millis() - timeLastSent > 3000) {
            reconnect(mesh);
            timeLastSent = millis();
            packet.currentTemperature = sensor.read();
            packet.expectedTemperature = expectedTemperature;

            Serial.print(F("Sent: "));
            printPacket(packet);
            if (!encMesh.send(&packet, sizeof(packet), 0, TEST_RECEIVE)) {
                Serial.println(F("Failed to send packet to master"));
                wdt_reset();
            }

        }
#else

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

        
#ifdef KEEP_ALIVE
        if (millis() - timeLastSent > 30000UL) {
            reconnect(mesh);
            timeLastSent = millis();
            packet.currentTemperature = sensor.read();
            packet.expectedTemperature = expectedTemperature;

            Serial.print(F("Sent: "));
            printPacket(packet);

            wdt_reset();
            if (!encMesh.send(&packet, sizeof(packet), 0, Config::ADDRESS_MASTER)) {
                Serial.println(F("Failed to send packet to master"));
                failures++;
            } else {
                failures = 0;
            }
        }
#else
        packet.currentTemperature = sensor.read();
        packet.expectedTemperature = expectedTemperature;

        printPacket(packet);
        if (!encMesh.send(&packet, sizeof(packet), 0, Config::ADDRESS_MASTER)) {
            Serial.println(F("Failed to send packet to master"));
            failures++;
        } else {
            failures = 0;
        }
        wdt_reset();

        Serial.flush();
        radio.powerDown();

        uint8_t timeToSleep = 8;
        uint32_t increaseTimer = timeToSleep * 8700;

        while (timeToSleep > 0) {
            LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
            timeToSleep--;
        }

        timer(increaseTimer);

        radio.powerUp();

        reconnect(mesh);
#endif
        // @TODO bug with failing to connect  
        if (failures > 10) {
            resetFunc();
        }
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
#endif

    }
    return 0;
}


