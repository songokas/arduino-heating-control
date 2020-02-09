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

States & AcctuatorProcessor::getStates()
{
    return zones;
}

bool AcctuatorProcessor::applyState(ZoneInfo & zoneInfo, IEncryptedMesh & radio, const HeaterInfo & heaterInfo)
{
    if (zoneInfo.pin.id > 0) {
        zoneInfo.print();
    }
    if (zoneInfo.pin.id > 100) {
        wdt_reset();
        ControllPacket controllPacket {zoneInfo.pin.id - 100, zoneInfo.getPwmState()};
        if (!radio.send(&controllPacket, sizeof(controllPacket), 0, Config::ADDRESS_SLAVE, 2)) {

            Serial.print(F("Failed to send Id: "));
            Serial.print(controllPacket.id);
            Serial.print(F(" State: "));
            Serial.println(controllPacket.state);

            zoneInfo.addError(Error::CONTROL_PACKET_FAILED);
            return false;
        } else {
            zoneInfo.removeError(Error::CONTROL_PACKET_FAILED);
        }
    } else if (zoneInfo.pin.id > 0) {
        pinMode(zoneInfo.pin.id, OUTPUT);
        analogWrite(zoneInfo.pin.id, zoneInfo.getPwmState());
    }
    zoneInfo.recordState(zoneInfo.getState());
    //saveReached(zoneInfo.pin.id, zoneInfo.reachedDesired);
    return true;
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

    const ZoneConfig * zoneConfig = getZoneConfigById(packet.id);
    if (!zoneConfig) {
        Serial.print(F("Error id "));
        Serial.print(packet.id);
        Serial.println(F(" is not in settings. Ignoring"));
        return;
    }
    ZoneInfo & zoneInfo = getAvailableZoneInfoById(packet.id);
    if (!(zoneInfo.pin.id > 0)) {
        zoneInfo.pin.id = packet.id;
        //if (hasReachedBefore(packet.id)) {
        //    zoneInfo.reachedDesired = true;
        //}
    }
    zoneInfo.addTemperature(currentTemperature);
    zoneInfo.dtReceived = now();
    float expectedTemperature = packet.expectedTemperature != 0 ? (float)packet.expectedTemperature / 100 : 0;
    zoneInfo.senderExpectedTemperature = expectedTemperature;
}

void AcctuatorProcessor::handleStates()
{
    for (auto & zoneInfo: zones) {
        if (!(zoneInfo.pin.id > 0)) {
            continue;
        }
        if (config.constantTemperatureEnabled && config.constantTemperature > 0) {
            setExpectedTemperature(zoneInfo, config.constantTemperature);
            continue;
        }
        const ZoneConfig * zoneConfig = getZoneConfigById(zoneInfo.pin.id);

        if (zoneInfo.senderExpectedTemperature > 0) {
            setExpectedTemperature(zoneInfo, zoneInfo.senderExpectedTemperature);
            continue;
        }

        if (!zoneConfig) {
            setExpectedTemperature(zoneInfo, Config::TEMPERATURE_DEFAULT);
            continue;
        }

        unsigned long currentHours = hour();
        bool stateSet = false;
        for (const auto & time: zoneConfig->times) {
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
    for (const auto & zoneInfo: zones) {
        if (zoneInfo.isOn() && zoneInfo.isWarm(config.acctuatorWarmupTime)) {
            return true;
        }
    }
    return false;
}

bool AcctuatorProcessor::isAnyEnabled() const
{
    for (const auto & zoneInfo: zones) {
        if (zoneInfo.isOn()) {
            return true;
        }
    }
    return false;
}

const ZoneConfig * AcctuatorProcessor::getZoneConfigById(byte id) const
{
    for (const auto & zone: config.zones) {
        if (zone.id == id) {
            return &zone;
        }
    }
    return nullptr;
}

ZoneInfo & AcctuatorProcessor::getAvailableZoneInfoById(byte id)
{
    for (auto & zone: zones) {
        if (zone.pin.id == id) {
            return zone;
        }
    }
    for (auto & zone: zones) {
        if (!(zone.pin.id > 0)) {
            return zone;
        }
    }

    return zones[Config::MAX_ZONES - 1];
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
    for (const auto & zone: config.zones) {
        if (zone.name[0] != '\0') {
            JsonObject zoneJson = jsonZones.createNestedObject();
            zoneJson["n"] = zone.name;
            zoneJson["id"] = zone.id;
            JsonArray times = zoneJson.createNestedArray("t");
            for (const auto & time: zone.times) {
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
    for (const auto & time: heaterInfo.history) {
        if (time.dtOn > 0) {
            JsonObject heaterTime = heaterTimes.createNestedObject();
            heaterTime["dtOn"] = time.dtOn;
            heaterTime["dtOff"] = time.dtOff;
        }
    }

    JsonArray jsonZones = root.createNestedArray("zones");
    for (const auto & zone: zones) {
        if (zone.pin.id > 0) {
            JsonObject zoneJson = jsonZones.createNestedObject();
            const ZoneConfig * zoneConfig = getZoneConfigById(zone.pin.id);
            zoneJson["n"] = zoneConfig ? zoneConfig->name : "anonymous";
            zoneJson["pin"] = zone.pin.id;
            zoneJson["st"] = zone.getState();
            zoneJson["cT"] = zone.getCurrentTemperature();
            zoneJson["eT"] = zone.expectedTemperature;
            zoneJson["dtR"] = zone.dtReceived;
            JsonArray errors = zoneJson.createNestedArray("er");
            for (const auto & error: zone.errors) {
                if (error != Error::NONE) {
                    errors.add((char)error);
                }
            }
            JsonArray recordedStates = zoneJson.createNestedArray("sT");
            for (const auto & stateTime: zone.stateTimes) {
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