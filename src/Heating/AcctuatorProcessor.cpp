#include <Arduino.h>
#include <ArduinoJson.h>
#include <Time.h>
#include <Ethernet.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <avr/wdt.h>
#include <EEPROM.h>

#include "Common.h"
#include "Config.h"
#include "Domain/HeaterInfo.h"
#include "Domain/ZoneInfo.h"

#include "RadioEncrypted/IEncryptedMesh.h"

using Heating::ControllPacket;
using Heating::Error;
using Heating::Config;
using Heating::Domain::HeaterInfo;
using Heating::Domain::ZoneInfo;
using Heating::ZoneConfig;
using RadioEncrypted::IEncryptedMesh;

#include "AcctuatorProcessor.h"

using Heating::AcctuatorProcessor;

extern int freeRam ();

AcctuatorProcessor::AcctuatorProcessor(const Config & c): config(c)
{
}

void AcctuatorProcessor::handlePacket(const Packet & packet)
{
    float currentTemperature = packet.currentTemperature != 0 ? (float)packet.currentTemperature / 100 : 0;
    if (currentTemperature > 50 || currentTemperature < -40) {
        Serial.print(F("Error temperature for "));
        Serial.print(packet.id);
        Serial.print(F(" temperature "));
        Serial.print(currentTemperature);
        Serial.println(F(" is not valid. Ignoring"));
        return;
    }

    byte id = packet.id;

    const ZoneConfig * zoneConfig = getZoneConfigById(id);
    if (!zoneConfig) {
        Serial.print(F("Error id "));
        Serial.print(id);
        Serial.println(F(" is not in settings. Ignoring"));
        return;
    }
    ZoneInfo & zoneInfo = getAvailableZoneInfoById(id);
    if (!(zoneInfo.pin.id > 0)) {
        zoneInfo.pin.id = id;
    }
    if (packet.currentTemperature != 0) {
        zoneInfo.addTemperature(currentTemperature);
        zoneInfo.dtReceived = now();
    }
    if (packet.expectedTemperature != 0) {
        zoneInfo.senderExpectedTemperature = (float)packet.expectedTemperature / 100;
        zoneInfo.dtExpectedReceived = now();
    }
}

void AcctuatorProcessor::handleStates()
{
    for (uint8_t i = 0; i < getStateArrLength(); i++) {
        ZoneInfo & zoneInfo = getState(i);
        if (!(zoneInfo.pin.id > 0)) {
            continue;
        }
        if (config.constantTemperatureEnabled && config.constantTemperature > 0) {
            setExpectedTemperature(zoneInfo, config.constantTemperature);
            continue;
        }
        const ZoneConfig * zoneConfig = getZoneConfigById(zoneInfo.pin.id);

        if (zoneInfo.isExpectedOn()) {
            setExpectedTemperature(zoneInfo, zoneInfo.senderExpectedTemperature);
            continue;
        }

        if (!zoneConfig) {
            setExpectedTemperature(zoneInfo, Config::TEMPERATURE_DEFAULT);
            continue;
        }

        unsigned long currentHours = hour();
        bool stateSet = false;
        for (uint8_t j = 0; j < zoneConfig->getTimeArrLength(); j++) {
            const Time & time = zoneConfig->getTime(j);
            if (currentHours >= time.from && currentHours < time.to) {
                if (zoneInfo.isWarm(9800) && !zoneInfo.reachedDesired) {
                    zoneInfo.reachedDesired = true;
                }
                setExpectedTemperature(zoneInfo, time.expectedTemperature);
                stateSet = true;
                break;
            }
        }
        if (!stateSet) {
            zoneInfo.pin.state = 0;
            zoneInfo.expectedTemperature = 0;
        }
    }
}

void AcctuatorProcessor::setExpectedTemperature(ZoneInfo & zoneInfo, float expectedTemperature)
{
    float currentTemperature = zoneInfo.getCurrentTemperature();
    byte statePercent = 0;
    if (zoneInfo.reachedDesired == true) {
        expectedTemperature -= config.temperatureDropWait;
    }
    if (currentTemperature < expectedTemperature) {
        float diff = expectedTemperature - currentTemperature;
        statePercent = diff < config.minTemperatureDiffForPwm ? 100 / config.minTemperatureDiffForPwm * diff : 100;
        if (statePercent < config.minPwmState) {
            zoneInfo.reachedDesired = true;
            statePercent = 0;
        } else {
            zoneInfo.reachedDesired = false;
        }
    } else {
        zoneInfo.reachedDesired = true;
        statePercent = 0;
    }
    zoneInfo.pin.state = statePercent;
    zoneInfo.expectedTemperature = expectedTemperature;
}

bool AcctuatorProcessor::isAnyWarmEnough() const
{
    for (uint8_t i = 0; i < getStateArrLength(); i++) {
        const ZoneInfo & zoneInfo = getState(i);
        if (zoneInfo.isOn() && zoneInfo.isWarm(config.acctuatorWarmupTime)) {
            return true;
        }
    }
    return false;
}

bool AcctuatorProcessor::isAnyEnabled() const
{
    for (uint8_t i = 0; i < getStateArrLength(); i++) {
        const ZoneInfo & zoneInfo = getState(i);
        if (zoneInfo.isOn()) {
            return true;
        }
    }
    return false;
}

const ZoneConfig * AcctuatorProcessor::getZoneConfigById(byte id) const
{
    for (uint8_t i = 0; i < config.getZoneArrLength(); i++) {
        const ZoneConfig & zone = config.getZone(i);
        if (zone.id == id) {
            return &zone;
        }
    }
    return nullptr;
}

ZoneInfo & AcctuatorProcessor::getAvailableZoneInfoById(byte id)
{
    for (uint8_t i = 0; i < getStateArrLength(); i++) {
        ZoneInfo & zoneInfo = getState(i);
        if (zoneInfo.pin.id == id) {
            return zoneInfo;
        }
    }
    for (uint8_t i = 0; i < getStateArrLength(); i++) {
        ZoneInfo & zoneInfo = getState(i);
        if (!(zoneInfo.pin.id > 0)) {
            return zoneInfo;
        }
    }

    return getState(getStateArrLength() - 1);
}

// this class should not print 
void AcctuatorProcessor::printConfig(EthernetClient & client) const
{
    StaticJsonDocument<Config::MAX_CONFIG_SIZE> root;
    root["acctuatorWarmupTime"] = config.acctuatorWarmupTime;
    root["heaterPumpStopTime"] = config.heaterPumpStopTime;
    root["constantTemperature"] = config.constantTemperature;
    root["constantTemperatureEnabled"] = config.constantTemperatureEnabled;
    root["minPwmState"] = config.minPwmState;
    root["minTemperatureDiffForPwm"] = config.minTemperatureDiffForPwm;
    root["temperatureDropWait"] = config.temperatureDropWait;
    JsonArray jsonZones = root.createNestedArray("zones");
    for (uint8_t i = 0; i < config.getZoneArrLength(); i++) {
        const ZoneConfig & zone = config.getZone(i);
        if (zone.id > 0) {
            JsonObject zoneJson = jsonZones.createNestedObject();
            zoneJson["n"] = zone.getName();
            zoneJson["id"] = zone.id;
            JsonArray times = zoneJson.createNestedArray("t");
            for (uint8_t j = 0; j < zone.getTimeArrLength(); j++) {
                const Time & time = zone.getTime(j);
                if (time.expectedTemperature > 0) {
                    JsonObject timejson = times.createNestedObject();
                    timejson["eT"] = time.expectedTemperature;
                    timejson["f"] = time.from;
                    timejson["to"] = time.to;
                }
            }
        }
    }
    serializeJson(root, client);
}

void AcctuatorProcessor::printInfo(EthernetClient & client, const HeaterInfo & heaterInfo, uint8_t networkFailures) const
{
    StaticJsonDocument<Config::MAX_INFO_JSON_SIZE> root;
    JsonObject system = root.createNestedObject("system");
    system["m"] = freeRam();
    system["f"] = networkFailures;


    JsonObject heater = root.createNestedObject("heater");
    heater["on"] = heaterInfo.isOn();
    heater["iT"] = heaterInfo.getInitTime();
    JsonArray heaterTimes = heater.createNestedArray("t");
    for (uint8_t i = 0; i < heaterInfo.getHistoryArrLength(); i++) {
        const auto & time = heaterInfo.getHistory(i);
        if (time.dtOn > 0) {
            JsonObject heaterTime = heaterTimes.createNestedObject();
            heaterTime["dtOn"] = time.dtOn;
            heaterTime["dtOff"] = time.dtOff;
        }
    }

    JsonArray jsonZones = root.createNestedArray("zones");
    for (uint8_t i = 0; i < getStateArrLength(); i++) {
        const ZoneInfo & zone = getState(i);
        if (zone.pin.id > 0) {
            JsonObject zoneJson = jsonZones.createNestedObject();
            const ZoneConfig * zoneConfig = getZoneConfigById(zone.pin.id);
            zoneJson["n"] = zoneConfig ? zoneConfig->getName() : "anonymous";
            zoneJson["pin"] = zone.pin.id;
            zoneJson["st"] = zone.getState();
            zoneJson["cT"] = zone.getCurrentTemperature();
            zoneJson["eT"] = zone.expectedTemperature;
            zoneJson["dtR"] = zone.dtReceived;
            JsonArray errors = zoneJson.createNestedArray("er");
            for (uint8_t j = 0; j < zone.getErrorArrLength(); j++) {
                const auto & error = zone.getError(j);
                if (error != Error::NONE) {
                    errors.add((char)error);
                }
            }
            JsonArray recordedStates = zoneJson.createNestedArray("sT");
            for (uint8_t j = 0; j < zone.getStateTimeArrLength(); j++) {
                const auto & stateTime = zone.getStateTime(j);
                if (stateTime.dtOn > 0) {
                    JsonObject jsonTime = recordedStates.createNestedObject();
                    jsonTime["dtOn"] = stateTime.dtOn;
                    jsonTime["dtOff"] = stateTime.dtOff;
                }
            }
        }
    }
    serializeJson(root, client);

}

bool AcctuatorProcessor::hasReachedBefore(byte id)
{
    return EEPROM.read(Config::MIN_STORAGE_INDEX + id) == 1;
}

void AcctuatorProcessor::saveReached(byte id, bool reached)
{
    EEPROM.update(Config::MIN_STORAGE_INDEX + id, reached ? 1 : 0);
}