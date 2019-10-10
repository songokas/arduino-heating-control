#ifndef HEATER_INFO_H
#define HEATER_INFO_H

namespace Heating
{
    namespace Domain
    {
        class HeaterInfo {
            private:
                bool on {false};
                time_t initTime {0};
            public:
                HeaterInfo() = default;
                HeaterInfo(time_t initTime);
                OnOffTime history[Config::MAX_HEATER_HISTORY] {};
                bool isOn() const;
                time_t getInitTime() const;
                bool isEmptyHistory() const;
                void markOn();
                void markOff();
                bool isShutingDown(unsigned int pumpStopTime) const;
        };
    }
}


#endif
