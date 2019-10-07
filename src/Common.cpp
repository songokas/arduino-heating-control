#include <Arduino.h>
#include <Time.h>
#include "Common.h"

namespace Heating
{

    void printTime(time_t t)
    {
        Serial.print(year(t));
        Serial.print('-');
        Serial.print(month(t));
        Serial.print('-');
        Serial.print(day(t));
        Serial.print(' ');
        Serial.print(hour(t));
        Serial.print('-');
        Serial.print(minute(t));
        Serial.print('-');
        Serial.print(second(t));
    }

    void printPacket(const Packet & packet)
    {
        Serial.print(F("Id: "));
        Serial.print(packet.id);
        Serial.print(F(" Current temperature: "));
        Serial.print(packet.currentTemperature);
        Serial.print(F(" Expected temperature: "));
        Serial.println(packet.expectedTemperature);
    }

    bool isPinAvailable(byte pin, const byte availablePins [], byte arrayLength)
    {
        for (byte i =  0; i < arrayLength; i++) {
            if (availablePins[i] == pin) {
                return true;
            }
        }
        return false;
    }

    unsigned long timer(unsigned int adjust, bool reset)
    {
        static unsigned long adjustedMillis = 0;
        if (reset) {
            adjustedMillis = 0;
        }
        adjustedMillis += adjust;
        return millis() + adjustedMillis;
    }
}
