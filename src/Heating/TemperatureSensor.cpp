#include <OneWire.h> 
#include <DallasTemperature.h>
#include "TemperatureSensor.h"

using Heating::TemperatureSensor;

TemperatureSensor::TemperatureSensor(DallasTemperature & sensors): sensors(sensors)
{
}

void TemperatureSensor::begin()
{
    if (!started) {
        sensors.begin();
    }
}

const float TemperatureSensor::read()
{
    begin();
    sensors.requestTemperatures();
    return sensors.getTempCByIndex(0);
}
