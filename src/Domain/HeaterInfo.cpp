#include <Arduino.h>
#include <Time.h>
#include "Common.h"
#include "Config.h"

using Heating::Config;

#include "HeaterInfo.h"

using Heating::Domain::HeaterInfo;

HeaterInfo::HeaterInfo(time_t systemStart): initTime(systemStart)
{}

bool HeaterInfo::isOn() const
{
    return on;
}

time_t HeaterInfo::getInitTime() const
{
    return initTime;
}
void HeaterInfo::markOn()
{
    on = true;
    for (int i = Config::MAX_HEATER_HISTORY - 1; i > 0; i--) {
        history[i] = history[i - 1];
    }
    history[0] = {now(), 0};
}

void HeaterInfo::markOff()
{
    on = false;
    if (history[0].dtOn > 0) {
        history[0].dtOff = now();
    }
}

bool HeaterInfo::isShutingDown(unsigned int pumpStopTime) const
{
    return on == false && history[0].dtOff > 0 && (history[0].dtOff + pumpStopTime) > now();
}

bool HeaterInfo::isEmptyHistory() const
{
    return history[0].dtOn == 0;
}
