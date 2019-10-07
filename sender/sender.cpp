#include <Arduino.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <LowPower.h>
#include <Time.h>
#include "Common.h"
#include "Config.h"
// arduino-mk
#include <xxtea-iot-crypt.h>
#include "Encryptor.h"
#include "PacketHandler.h"
#include "Radio.h"
#include "TemperatureSensor.h"

using Heating::Packet;
using Heating::Config;
using Heating::TemperatureSensor;
using Heating::Radio;
using Heating::printPacket;
using Heating::timer;
using Network::Encryptor;
using Network::PacketHandler;

// these should be defined when building
//#define ID 105
//#define KEY key

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

    RF24 rf(Config::PIN_RADIO_CE, Config::PIN_RADIO_CSN);
    OneWire oneWire(Config::PIN_TEMPERATURE); 
    DallasTemperature sensors(&oneWire);
    Encryptor encryptor(KEY);
    PacketHandler packageHandler;
    Radio radio(rf, packageHandler, encryptor);
    TemperatureSensor sensor(sensors);
    Packet packet { ID, 0, 0};

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
        if (!radio.send(Config::ADDRESS_MASTER, &packet, sizeof(packet))) {
            Serial.println(F("Failed to send packet to master"));
        }

        printPacket(packet);
        Serial.flush();
        radio.powerDown();
        LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
        timer(8700);
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


