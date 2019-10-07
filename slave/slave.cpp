#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <Time.h>
#include <avr/wdt.h>
#include "Common.h"
#include "Config.h"
// arduino-mk
#include <xxtea-iot-crypt.h>
#include "Encryptor.h"
#include "PacketHandler.h"
#include "Radio.h"
#include "TemperatureSensor.h"

//#define ID 106
//#define KEY key

using Heating::Config;
using Heating::ControllPacket;
using Heating::Packet;
using Heating::printPacket;
using Heating::isPinAvailable;
using Heating::TemperatureSensor;
using Heating::Radio;
using Network::Encryptor;
using Network::PacketHandler;

struct Acctuator {
    byte id;
    unsigned long dtReceived;
};

struct Handler {
    Acctuator acctuators[Config::MAX_SLAVE_ACCTUATORS] {};

    void handleTimeouts()
    {
        unsigned long currentTime = millis();
        for (auto & acctuator: acctuators) {
            if (acctuator.id > 0) {
                Serial.print("Id: ");
                Serial.print(acctuator.id);
                Serial.print(" Received: ");
                Serial.println(acctuator.dtReceived);
                if (currentTime - acctuator.dtReceived > Config::MAX_DELAY) {
                    Serial.print("Timeout: ");
                    Serial.println(acctuator.id);
                    analogWrite(acctuator.id, 0);
                    acctuator = {};
                }
            }
        }
    }

    void addTimeout(byte id)
    {
        for (auto & acctuator: acctuators) {
            if (acctuator.id == id) {
                acctuator.dtReceived = millis();
                return;
            }
        }
        for (auto & acctuator: acctuators) {
            if (acctuator.id == 0) {
                acctuator = {id, millis()};
                return;
            }
        }

    }
};

void handleInnerTemperature(TemperatureSensor & sensor, Radio & radio)
{
    Packet packet { ID, sensor.read(), 0};
    printPacket(packet);
    if (!radio.send(Config::ADDRESS_MASTER, &packet, sizeof(packet), ID)) {
        Serial.println(F("Failed to send packet"));
    }
}

int main()
{
    init();

    Serial.begin(Config::SERIAL_RATE); 
    RF24 rf(Config::PIN_RADIO_CE, Config::PIN_RADIO_CSN);
    Encryptor encryptor(KEY);
    PacketHandler packageHandler;
    Radio radio(rf, packageHandler, encryptor);
    Handler handler {};

    OneWire oneWire(Config::PIN_TEMPERATURE); 
    DallasTemperature sensors(&oneWire);
    TemperatureSensor sensor(sensors);

    wdt_enable(WDTO_8S);

    // 1 minute
    unsigned long TIME_PERIOD = 60000UL;
    unsigned long startTime = millis();
    while(true) {

        if (radio.isAvailable(Config::ADDRESS_SLAVE)) {
            wdt_reset();
            ControllPacket received;
            if (!radio.receive(Config::ADDRESS_SLAVE, &received, sizeof(received))) {
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
