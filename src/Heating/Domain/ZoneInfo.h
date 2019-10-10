#ifndef ZONE_INFO_H
#define ZONE_INFO_H

namespace Heating
{
    namespace Domain
    {

        class ZoneInfo {
            public:

                Pin pin {};
                bool reachedDesired {};
                float senderExpectedTemperature {};
                float expectedTemperature {};
                float temps[Config::MAX_ZONE_TEMPS] {};
                time_t dtReceived {};
                OnOffTime stateTimes[Config::MAX_ZONE_STATE_HISTORY] {};
                Error errors[Config::MAX_ZONE_ERRORS] {};

                float getCurrentTemperature() const;
                void addTemperature(float currentTemperature);
                bool isOn() const;
                byte getState() const;
                byte getPwmState() const;
                void addError(Error error);
                void removeError(Error error);
                bool isWarm(unsigned int acctuatorWarmupTime) const;
                void print() const;
                void recordState(byte state);
        };
    }
}

#endif
