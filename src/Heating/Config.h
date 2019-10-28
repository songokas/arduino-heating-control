#ifndef CONFIG_H
#define CONFIG_H

namespace Heating
{
    static const byte MAX_ZONE_NAME_LENGTH = 20;
    static const byte MAX_TIMES_PER_ZONE = 4;

    struct Time {
        float expectedTemperature;
        byte from;
        byte to;
    };

    struct ZoneConfig {
        char name[MAX_ZONE_NAME_LENGTH];
        byte id;
        Time times[MAX_TIMES_PER_ZONE];
    };

    class Config
    {
        public:

            static const byte ADDRESS_MASTER = 88;
            static const byte ADDRESS_SLAVE = 89;

            static constexpr byte AVAILABLE_PINS_NANO[] = {3, 5, 6, 9, 10};
            // 2 to 13 and 44 to 46.
            static constexpr byte AVAILABLE_PINS_MEGA[] = {3, 4, 5, 6, 9, 10, 34, 35, 44, 45, 46, 103, 105, 106, 109, 110};
            

            static const byte PIN_RADIO_CE = 7;
            static const byte PIN_RADIO_CSN = 8;
            static const byte PIN_TEMPERATURE = 2;
            static const byte PIN_LED =  4;
            static const byte PIN_BUTTON = 3;

            static const byte MAX_ZONES = 12;
            static const byte MAX_ZONE_NAME_LENGTH = Heating::MAX_ZONE_NAME_LENGTH;
            static const byte MAX_ZONE_TEMPS = 5;
            static const byte MAX_ZONE_STATE_HISTORY = 3;
            static const byte MAX_TIMES_PER_ZONE = Heating::MAX_TIMES_PER_ZONE;

            static const byte MAX_SLAVE_ACCTUATORS = 12;
            //no packet received in miliseconds
            static const long MAX_DELAY = 7200000UL;

            // default temperature if no configuration matches sender
            static constexpr float TEMPERATURE_DEFAULT = 21;
            // when a button is clicked raise temperature to
            static const byte RAISED_TEMPERATURE = 23;

            // wait for temperature to drop more than diff
            static constexpr float TEMPERATURE_DROP_WAIT = 0.7;

            static const unsigned int MAX_REQUEST_SIZE = 2000;
            static const unsigned int MAX_REQUEST_PRINT_BUFFER = 512;

            static const unsigned int MAX_CONFIG_SIZE = 1300;
            static const unsigned int MAX_INFO_JSON_SIZE = 2200;

            static const byte MAX_HEATER_HISTORY = 3;
            static const byte MAX_ZONE_ERRORS = 3;

            static const byte TIME_ZONE = 2;     // Vilnius Time
            static const unsigned int SERIAL_RATE = 9600;

            static constexpr byte MASTER_MAC[] { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

            // ip address if dhcp fails
            static constexpr byte MASTER_IP[] { 192, 168, 0, 174 };

            //pin that turns heater on/off
            static const byte PIN_HEATING = 30;

            static const byte MAX_SESSION_DATA_SIZE = 22;
            static const byte MAX_SESSIONS_AVAILABLE = 10;

            static const byte MAX_GENERAL_ERRORS = 20;


            unsigned int acctuatorWarmupTime {180};
            unsigned int heaterPumpStopTime {600};
            bool constantTemperatureEnabled {false};
            float constantTemperature {0};
            float minTemperatureDiffForPwm {0.5};
            float temperatureDropWait {0.7};
            byte minPwmState {30}; //percent
            ZoneConfig zones[MAX_ZONES] {};
    };
}

#endif
