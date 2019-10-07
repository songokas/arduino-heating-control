#include <Arduino.h>
#include <Time.h>
#include "Common.h"
#include "Config.h"
#include "ZoneInfo.h"

using Heating::Domain::ZoneInfo;
using Heating::Config;
using Heating::Error;

float ZoneInfo::getCurrentTemperature() const
{
    float product = 0;
    byte count = 0;
    for (auto cT: temps) {
        if (cT != 0 && cT > -40 && cT < 60) {
            product += cT;
            count++;
        }
    }
    return product > 0 ? product / count : 0;
}

void ZoneInfo::addTemperature(float currentTemperature)
{
    for (int i = Config::MAX_ZONE_TEMPS - 1; i > 0; i--) {
        temps[i] = temps[i - 1];

    }
    temps[0] = currentTemperature;
}

bool ZoneInfo::isOn() const
{
    return now() - dtReceived < (Config::MAX_DELAY / 1000) && pin.state > 0;
}

bool ZoneInfo::isWarm(unsigned int acctuatorWarmupTime) const
{
    return isOn() && stateTimes[0].dtOn > 0 && stateTimes[0].dtOff == 0 && (stateTimes[0].dtOn + acctuatorWarmupTime) <= now();
}

byte ZoneInfo::getState() const
{
    return isOn() ? pin.state : 0;
}

byte ZoneInfo::getPwmState() const
{
    return map(getState(), 0, 100, 0, 255);
}

void ZoneInfo::recordState(byte state)
{
    if (state > 0 && stateTimes[0].dtOn > 0 && stateTimes[0].dtOff == 0) {
        return;
    } else if (state == 0) {
        if (stateTimes[0].dtOff == 0) {
            stateTimes[0].dtOff = now();
        }
        return;
    }
    for (int i = Config::MAX_ZONE_STATE_HISTORY - 1; i > 0; i--) {
        stateTimes[i] = stateTimes[i - 1];

    }
    stateTimes[0] = {now(), 0};
}

void ZoneInfo::addError(Error error)
{
    for (auto & e: errors) {
        if (e == Error::NONE || e == error) {
            e = error;
            break;
        }
    }
}

void ZoneInfo::removeError(Error error)
{
    for (auto & e: errors) {
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
    Serial.println();
}

