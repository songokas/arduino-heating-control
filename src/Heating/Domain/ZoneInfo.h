#ifndef HEATING_DOMAIN_ZONE_INFO_H
#define HEATING_DOMAIN_ZONE_INFO_H

namespace Heating
{
    namespace Domain
    {

        class ZoneInfo
        {
            public:

                Pin pin {};
                bool reachedDesired {true};
                float senderExpectedTemperature {0};
                float expectedTemperature {0};
                time_t dtReceived {0};

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

                virtual float & getTemp(uint8_t i) = 0;
                virtual float getTemp(uint8_t i) const = 0;
                virtual uint8_t getTempArrLength() const = 0;
                virtual OnOffTime & getStateTime(uint8_t i) = 0;
                virtual OnOffTime getStateTime(uint8_t i) const = 0;
                virtual uint8_t getStateTimeArrLength() const = 0;
                virtual Error & getError(uint8_t i) = 0;
                virtual Error getError(uint8_t i) const = 0;
                virtual uint8_t getErrorArrLength() const = 0;
        };

        template<uint8_t maxTemps, uint8_t maxHistory, uint8_t maxErrors>
        class StaticZoneInfo: public ZoneInfo
        {
            public:
                float & getTemp(uint8_t i) { return i >= maxTemps ? temps[maxTemps - 1] : temps[i]; }
                float getTemp(uint8_t i) const { return i >= maxTemps ? temps[maxTemps - 1] : temps[i]; }
                uint8_t getTempArrLength() const { return maxTemps; }

                OnOffTime & getStateTime(uint8_t i) { return i >= maxHistory ? stateTimes[maxHistory - 1] : stateTimes[i]; }
                OnOffTime getStateTime(uint8_t i) const { return i >= maxHistory ? stateTimes[maxHistory - 1] : stateTimes[i]; }
                uint8_t getStateTimeArrLength() const { return maxHistory; };
                
                Error & getError(uint8_t i) { return i >= maxErrors ? errors[maxErrors - 1] : errors[i]; } 
                Error getError(uint8_t i) const { return i >= maxErrors ? errors[maxErrors - 1] : errors[i]; } 
                uint8_t getErrorArrLength() const { return maxErrors; };

            private:
                float temps[maxTemps] {0};
                OnOffTime stateTimes[maxHistory] {};
                Error errors[maxErrors] {};
        };
    }
}

#endif
