#include <Arduino.h>
#include <Time.h>
#include "../Common.h"
#include "../Config.h"
#include "ZoneInfo.h"

using Heating::Domain::ZoneInfo;
using Heating::Config;
using Heating::Error;

float ZoneInfo::getCurrentTemperature() const
{
    float product = 0;
    byte count = 0;
    for (uint8_t i = 0; i < getTempArrLength(); i++) {
        float cT = getTemp(i);
        if (cT != 0 && cT > -40 && cT < 60) {
            product += cT;
            count++;
        }
    }
    return product > 0 ? product / count : 0;
}

void ZoneInfo::addTemperature(float currentTemperature)
{
    for (uint8_t i = getTempArrLength() - 1; i > 0; i--) {
        float & current = getTemp(i);
        float previous = getTemp(i - 1);
        current = previous;
    }
    float & current = getTemp(0);
    current = currentTemperature;
}

bool ZoneInfo::isOn() const
{
    return (now() - dtReceived < (Config::MAX_DELAY / 1000)) && pin.state > 0;
}

bool ZoneInfo::isExpectedOn() const
{
    return (now() - dtExpectedReceived < (Config::MAX_DELAY / 1000)) && senderExpectedTemperature != 0;
}

bool ZoneInfo::hasConfirmation() const
{
    return dtConfirmationReceived > 0 && (now() - dtConfirmationReceived) < 600;
}

bool ZoneInfo::isWarm(unsigned int acctuatorWarmupTime) const
{
    OnOffTime time = getStateTime(0);
    return isOn() && time.dtOn > 0 && time.dtOff == 0 && (time.dtOn + acctuatorWarmupTime) <= now();
}

byte ZoneInfo::getState() const
{
    return isOn() ? pin.state : 0;
}

byte ZoneInfo::getPwmState() const
{
    return map(getState(), 0, 100, 0, 255);
}

uint16_t ZoneInfo::getPwmValue() const 
{
    return map(getState(), 0, 100, 0, 1023);
}

void ZoneInfo::recordState(byte state)
{
    OnOffTime & time = getStateTime(0);
    if (state > 0 && time.dtOn > 0 && time.dtOff == 0) {
        return;
    } else if (state == 0) {
        if (time.dtOff == 0) {
            time.dtOff = now();
        }
        return;
    }
    for (int i = getStateTimeArrLength() - 1; i > 0; i--) {
        OnOffTime & current = getStateTime(i);
        OnOffTime previous = getStateTime(i - 1);
        current = previous;

    }
    OnOffTime & current = getStateTime(0);
    current = {now(), 0};
}

void ZoneInfo::addError(Error error)
{
    for (uint8_t i = 0; i < getErrorArrLength(); i++) {
        Error & e = getError(i);
        if (e == Error::NONE || e == error) {
            e = error;
            break;
        }
    }
}

void ZoneInfo::removeError(Error error)
{
    for (uint8_t i = 0; i < getErrorArrLength(); i++) {
        Error & e = getError(i);
        if (e == error) {
            e = Error::NONE;
            break;
        }
    }
}

void ZoneInfo::print() const
{
    Serial.print(F("Id: "));
    Serial.print(pin.id);
    Serial.print(F(" State: "));
    Serial.print(getState());
    Serial.print(F(" Temperature reached: "));
    Serial.print(reachedDesired);
    Serial.print(F(" Current temperature: "));
    Serial.print(getCurrentTemperature());
    Serial.print(F(" Expected temperature: "));
    Serial.print(expectedTemperature);
    Serial.print(F(" Last received on: "));
    printTime(dtReceived);
    Serial.print(F(" Is On?: "));
    Serial.print(isOn() ? "yes" : "no");
    Serial.print(F(" Sender expected: "));
    Serial.print(senderExpectedTemperature);
    Serial.print(F(" Expected on: "));
    Serial.print(isExpectedOn() ? "yes" : "no");
    Serial.println();
}

