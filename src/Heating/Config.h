#ifndef HEATING_CONFIG_H
#define HEATING_CONFIG_H

namespace Heating
{

    struct Time {
        float expectedTemperature {0};
        uint8_t from {0};
        uint8_t to {0};
    };

    struct ZoneConfig
    {
        uint8_t id;

        virtual const char * getName() const = 0;
        virtual Time & getTime(uint8_t i) = 0;
        virtual Time getTime(uint8_t i) const = 0;
        virtual uint8_t getTimeArrLength() const = 0;
        virtual void updateName(const char * newname) = 0;
    };

    template<uint8_t maxName, uint8_t maxTimes>
    struct StaticZoneConfig: public ZoneConfig
    {
        const char * getName() const { return name; }
        Time & getTime(uint8_t i) { return i >= maxTimes  ? times[maxTimes - 1] : times[i]; }
        Time getTime(uint8_t i) const { return i >= maxTimes  ? times[maxTimes - 1] : times[i]; }
        uint8_t getTimeArrLength() const { return maxTimes; }
        void updateName(const char * newname) { strlcpy(name, newname, sizeof(name)); }

        char name[maxName] {0};
        Time times[maxTimes] {}; 
    };

    class Config
    {
        public:

            static const uint8_t ADDRESS_MASTER = 88;
            static const uint8_t ADDRESS_SLAVE = 89;
            static const uint8_t ADDRESS_FORWARD = 255;

            static constexpr uint8_t AVAILABLE_PINS_NANO[] = {3, 5, 6, 9, 10};
            // 2 to 13 and 44 to 46.
            static constexpr uint8_t AVAILABLE_PINS_MEGA[] = {3, 4, 5, 6, 9, 10, 34, 35, 44, 45, 46, 103, 105, 106, 109, 110};
            

            static const uint8_t PIN_RADIO_CE = 7;
            static const uint8_t PIN_RADIO_CSN = 8;
            static const uint8_t PIN_TEMPERATURE = 2;
            static const uint8_t PIN_LED =  4;
            static const uint8_t PIN_BUTTON = 3;

            static const uint8_t MAX_ZONES = 11;
            static const uint8_t MAX_ZONE_NAME_LENGTH = 20;
            static const uint8_t MAX_ZONE_TEMPS = 5;
            static const uint8_t MAX_ZONE_STATE_HISTORY = 3;
            static const uint8_t MAX_ZONE_ERRORS = 1;
            static const uint8_t MAX_TIMES_PER_ZONE = 3;

            static const uint8_t MAX_SLAVE_ACCTUATORS = 12;
            //no packet received in miliseconds
            static const long MAX_DELAY = 7200000UL;

            // default temperature if no configuration matches sender
            static constexpr float TEMPERATURE_DEFAULT = 21;
            // when a button is clicked raise temperature to
            static const int16_t RAISED_TEMPERATURE = 2300;

            // wait for temperature to drop more than diff
            static constexpr float TEMPERATURE_DROP_WAIT = 0.7;

            static const unsigned int MAX_REQUEST_SIZE = 1800;
            static const unsigned int MAX_REQUEST_PRINT_BUFFER = 512;

            static const unsigned int MAX_CONFIG_SIZE = 1300;
            static const unsigned int MAX_INFO_JSON_SIZE = 2000;
            // addtional info stored in eeprom
            static const unsigned int MIN_STORAGE_INDEX = 2500;

            static const uint8_t MAX_HEATER_HISTORY = 3;

            static const uint8_t TIME_ZONE = 2;     // Vilnius Time
            static const unsigned int SERIAL_RATE = 9600;

            static constexpr uint8_t MASTER_MAC[] { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

            // ip address if dhcp fails
            static constexpr uint8_t MASTER_IP[] { 192, 168, 0, 120 };

            //pin that turns heater on/off
            static const uint8_t PIN_HEATING = 30;

            static const uint8_t MAX_SESSION_DATA_SIZE = 22;
            static const uint8_t MAX_SESSIONS_AVAILABLE = 10;

            unsigned int acctuatorWarmupTime {180};
            unsigned int heaterPumpStopTime {600};
            bool constantTemperatureEnabled {false};
            float constantTemperature {0};
            float minTemperatureDiffForPwm {0.5};
            float temperatureDropWait {0.7};
            uint8_t minPwmState {30}; //percent

            virtual ZoneConfig & getZone(uint8_t i) = 0;
            virtual const ZoneConfig & getZone(uint8_t i) const = 0;
            virtual uint8_t getZoneArrLength() const = 0;
    };

    template<uint8_t maxZones, uint8_t maxName, uint8_t maxTimes>
    class StaticConfig: public Config
    {
        public:
            ZoneConfig & getZone(uint8_t i) { return i >= maxZones ? zones[maxZones - 1] : zones[i]; }
            const ZoneConfig & getZone(uint8_t i) const { return i >= maxZones ? zones[maxZones - 1] : zones[i]; }
            uint8_t getZoneArrLength() const { return maxZones; }
        private:
            StaticZoneConfig<maxName, maxTimes> zones[maxZones] {};
    };
}

#endif
