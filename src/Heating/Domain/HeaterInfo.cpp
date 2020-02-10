#include <Arduino.h>
#include <Time.h>
#include "../Common.h"
#include "../Config.h"

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

    for (int i = getHistoryArrLength() - 1; i > 0; i--) {
        OnOffTime & current = getHistory(i);
        OnOffTime previous = getHistory(i - 1);
        current = previous;
    }
    OnOffTime & current = getHistory(0);
    current = {now(), 0};
}

void HeaterInfo::markOff()
{
    on = false;
    if (getHistory(0).dtOn > 0) {
        getHistory(0).dtOff = now();
    }
}

bool HeaterInfo::isShutingDown(unsigned int pumpStopTime) const
{
    OnOffTime history = getHistory(0);
    return on == false && history.dtOff > 0 && (history.dtOff + pumpStopTime) > now();
}

bool HeaterInfo::isEmptyHistory() const
{
    return getHistory(0).dtOn == 0;
}
