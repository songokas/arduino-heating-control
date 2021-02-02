#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Time.h>
#include <Streaming.h>
#include "Common.h"
#include "Config.h"
#include "Storage.h"

using Heating::Storage;
using Heating::Config;

extern int freeRam () {
    extern int __heap_start, *__brkval; 
    int v; 
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

const char DEFAULT_JSON[] PROGMEM = "{\"acctuatorWarmupTime\":180,\"heaterPumpStopTime\":600,\"constantTemperature\":18,\"constantTemperatureEnabled\":false,\"minPwmState\":30,\"minTemperatureDiffForPwm\":0.5,\"temperatureDropWait\":0.7,\"zones\":[{\"n\":\"miegamasis\",\"id\":205,\"t\":[{\"eT\":20.5,\"f\":0,\"to\":24}]},{\"n\":\"vaiku\",\"id\":204,\"t\":[{\"eT\":21,\"f\":0,\"to\":7},{\"eT\":20.5,\"f\":7,\"to\":20},{\"eT\":21,\"f\":20,\"to\":24}]},{\"n\":\"darbo\",\"id\":203,\"t\":[{\"eT\":20.5,\"f\":4,\"to\":18}]},{\"n\":\"vonia\",\"id\":202,\"t\":[{\"eT\":22,\"f\":4,\"to\":7},{\"eT\":21,\"f\":7,\"to\":23}]},{\"n\":\"rubine\",\"id\":201,\"t\":[{\"eT\":20,\"f\":4,\"to\":7}]},{\"n\":\"katiline\",\"id\":34,\"t\":[{\"eT\":20,\"f\":4,\"to\":7}]},{\"n\":\"tamburas\",\"id\":35,\"t\":[{\"eT\":20,\"f\":4,\"to\":7}]},{\"n\":\"toletas\",\"id\":6,\"t\":[{\"eT\":21,\"f\":4,\"to\":21}]},{\"n\":\"sveciu\",\"id\":5,\"t\":[{\"eT\":20.5,\"f\":4,\"to\":21}]},{\"n\":\"salionas\",\"id\":4,\"t\":[{\"eT\":20.5,\"f\":4,\"to\":21}]},{\"n\":\"virtuve\",\"id\":3,\"t\":[{\"eT\":20.5,\"f\":4,\"to\":21}]}]}";

bool Storage::loadConfiguration(Config & config) const
{
    DynamicJsonDocument root(Config::MAX_CONFIG_SIZE + 300);
    char * json = new char[Config::MAX_CONFIG_SIZE + 1] {0};
    if (EEPROM[0] == '{') {
        Serial.println(F("Reading from memory"));
        unsigned int i = 0;
        while (i < Config::MAX_CONFIG_SIZE) {
            json[i] = EEPROM.read(i);
            if (json[i] == '\0') {
                break;
            }
            i++;
        }
        json[i] = '\0';
    } else {
        Serial.println(F("Using default configuration"));
        int i = 0;
        int defaultJsonSize = strlen_P(DEFAULT_JSON);
        while (i < defaultJsonSize && i < Config::MAX_CONFIG_SIZE) {
            json[i] = pgm_read_byte_near(DEFAULT_JSON + i);
            i++;
        }
        json[i] = '\0';
    }

    Serial.print(F("Deserialize config memory left: "));
    Serial.println(freeRam());
    auto error = deserializeJson(root, json, Config::MAX_CONFIG_SIZE + 1);
    if (error) {
        delete json;
        EEPROM.update(0, '0');
        Serial << F("failed to parse config: ") << error.c_str() << endl;
        return false;
    }
    Serial.print(F("Deserialized finished config memory left: "));
    Serial.println(freeRam());

    config.acctuatorWarmupTime = root["acctuatorWarmupTime"].as<unsigned int>();
    if (!(config.acctuatorWarmupTime < 600)) {
        config.acctuatorWarmupTime = 180;
    }
    config.heaterPumpStopTime = root["heaterPumpStopTime"].as<unsigned int>();
    if (!(config.heaterPumpStopTime < 3600)) {
        config.heaterPumpStopTime = 600;
    }
    config.constantTemperature = root["constantTemperature"].as<float>();
    config.constantTemperatureEnabled = root["constantTemperatureEnabled"].as<bool>();
    config.minPwmState = root["minPwmState"].as<byte>();
    if (!(config.minPwmState > 10 && config.minPwmState < 100)) {
        config.minPwmState = 30;
    }
    config.minTemperatureDiffForPwm = root["minTemperatureDiffForPwm"].as<float>();
    if (!(config.minTemperatureDiffForPwm > 0 && config.minTemperatureDiffForPwm < 2)) {
        config.minTemperatureDiffForPwm = 0.5;
    }

    config.temperatureDropWait = root["temperatureDropWait"].as<float>();
    if (!(config.temperatureDropWait > 0 && config.temperatureDropWait < 2)) {
        config.temperatureDropWait = 0.7;
    }
    int i = 0;
    JsonArray zones = root["zones"].as<JsonArray>();
    for (auto zone: zones) {
        ZoneConfig & zoneConfig = config.getZone(i);
        zoneConfig.updateName(zone["n"] | "unknown");
        zoneConfig.id = zone["id"].as<byte>();

        int j = 0;
        JsonArray times = zone["t"].as<JsonArray>();
        for (auto time: times) {
            Time & zoneTime = zoneConfig.getTime(j);
            zoneTime.from = time["f"].as<byte>();
            zoneTime.to = time["to"].as<byte>();
            zoneTime.expectedTemperature = time["eT"].as<float>();
            j++;
            if (j >= zoneConfig.getTimeArrLength()) {
                break;
            }
        }
        while (j < zoneConfig.getTimeArrLength()) {
            Time & zoneTime = zoneConfig.getTime(j);
            zoneTime = {};
            j++;
        }
        i++;
        if (i >= config.getZoneArrLength()) {
            break;
        }
    }
    while (i < config.getZoneArrLength()) {
        ZoneConfig & zoneConfig = config.getZone(i);
        zoneConfig = {};
        i++;
    }
    delete json;
    return true;
}

bool Storage::saveConfiguration(const char * json) const
{
    int lastIndex = strlen(json);
    bool valid = strcmp(json, "0") == 0 || (json[0] == '{' && json[lastIndex - 1] == '}');
    if (!valid) {
        return false;
    }
    Serial.print(F("Save config memory left: "));
    Serial.println(freeRam());
    int i = 0;
    while(i < Config::MAX_CONFIG_SIZE && json[i] != '\0') {
        EEPROM.update(i, (byte)json[i]);
        i++;
    }
    EEPROM.update(i, '\0');
    return i > 0;
}
