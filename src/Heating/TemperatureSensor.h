#ifndef TEMPERATURE_SENSOR
#define TEMPERATURE_SENSOR

namespace Heating
{
    class TemperatureSensor
    {
        private:
            bool started {false};
            DallasTemperature & sensors;
        public:
            TemperatureSensor(DallasTemperature & sensors);
            void begin();
            const float read();
    };
}

#endif
