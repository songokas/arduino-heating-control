#ifndef HEATING_HEATER_INFO_H
#define HEATING_HEATER_INFO_H

namespace Heating
{
    namespace Domain
    {
        class HeaterInfo
        {
            public:
                HeaterInfo() = default;
                HeaterInfo(time_t initTime);
                bool isOn() const;
                time_t getInitTime() const;
                bool isEmptyHistory() const;
                void markOn();
                void markOff();
                bool isShutingDown(unsigned int pumpStopTime) const;
                
                virtual OnOffTime & getHistory(uint8_t i) = 0;
                virtual OnOffTime getHistory(uint8_t i) const = 0;
                virtual uint8_t getHistoryArrLength() const = 0;
            private:
                bool on {false};
                time_t initTime {0};
        };

        template<uint8_t maxHistory>
        class StaticHeaterInfo: public HeaterInfo
        {
            public:
                StaticHeaterInfo(): HeaterInfo() {}
                StaticHeaterInfo(time_t initTime): HeaterInfo(initTime) {}

                OnOffTime & getHistory(uint8_t i) { return i >= maxHistory  ? history[maxHistory - 1] : history[i]; }
                OnOffTime getHistory(uint8_t i) const { return i >= maxHistory  ? history[maxHistory - 1] : history[i]; }

                uint8_t getHistoryArrLength() const { return maxHistory; }
            private:
                OnOffTime history[maxHistory] {}; 
        };
    }
}


#endif
