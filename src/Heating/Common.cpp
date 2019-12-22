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
        Serial.print((float)packet.currentTemperature / 100);
        Serial.print(F(" Expected temperature: "));
        Serial.println((float)packet.expectedTemperature / 100);
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


#if DEBUG
#define PRINTS(s) Serial.print(F(s))
#define PRINT(s,v) do { Serial.print(F(s)); Serial.print(v); } while(0);
#define PRINTX(s,v) do { Serial.print(F(s)); Serial.print(F(“0x”)); Serial.print(v, HEX); } while(0);
#else
#define PRINTS(s)
#define PRINT(s,v)
#define PRINTX(s,v)
#endif


}
