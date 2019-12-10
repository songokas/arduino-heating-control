#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Time.h>
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

const char DEFAULT_JSON[] PROGMEM = "{\"acctuatorWarmupTime\":180,\"heaterPumpStopTime\":600,\"constantTemperature\":18,\"constantTemperatureEnabled\":false,\"minPwmState\":30,\"minTemperatureDiffForPwm\":0.5,\"temperatureDropWait\":0.7,\"zones\":[{\"n\":\"miegamasis\",\"id\":110,\"t\":[{\"eT\":20.5,\"f\":0,\"to\":24}]},{\"n\":\"vaiku\",\"id\":109,\"t\":[{\"eT\":21,\"f\":0,\"to\":7},{\"eT\":20.5,\"f\":7,\"to\":20},{\"eT\":21,\"f\":20,\"to\":24}]},{\"n\":\"darbo\",\"id\":106,\"t\":[{\"eT\":20.5,\"f\":4,\"to\":18}]},{\"n\":\"vonia\",\"id\":105,\"t\":[{\"eT\":22,\"f\":4,\"to\":7},{\"eT\":21,\"f\":7,\"to\":23}]},{\"n\":\"rubine\",\"id\":103,\"t\":[{\"eT\":20,\"f\":4,\"to\":7}]},{\"n\":\"katiline\",\"id\":34,\"t\":[{\"eT\":20,\"f\":4,\"to\":7}]},{\"n\":\"tamburas\",\"id\":35,\"t\":[{\"eT\":20,\"f\":4,\"to\":7}]},{\"n\":\"toletas\",\"id\":6,\"t\":[{\"eT\":21,\"f\":4,\"to\":21}]},{\"n\":\"sveciu\",\"id\":5,\"t\":[{\"eT\":20.5,\"f\":4,\"to\":21}]},{\"n\":\"salionas\",\"id\":4,\"t\":[{\"eT\":20.5,\"f\":4,\"to\":21}]},{\"n\":\"virtuve\",\"id\":3,\"t\":[{\"eT\":20.5,\"f\":4,\"to\":21}]}]}";

bool Storage::loadConfiguration(Config & config) const
{
    DynamicJsonBuffer jsonBuffer(Config::MAX_CONFIG_SIZE + 300);
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
    Serial.print(F("Load config memory left: "));
    Serial.println(freeRam());
    Serial.println(json);
    JsonObject& root = jsonBuffer.parseObject(json);
    if (!root.success()) {
        EEPROM.update(0, '0');
        Serial.println(F("Failed to parse config"));
        delete json;
        return false;
    }
    Serial.print(F("Load config memory left: "));
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
    JsonArray & zones = root["zones"];
    for (auto zone: zones) {
        strlcpy(config.zones[i].name, zone["n"] | "unknown", sizeof(config.zones[i].name));
        config.zones[i].id = zone["id"].as<byte>();
        int j = 0;
        JsonArray & times = zone["t"];
        for (auto time: times) {
            config.zones[i].times[j].from = time["f"].as<byte>();
            config.zones[i].times[j].to = time["to"].as<byte>();
            config.zones[i].times[j].expectedTemperature = time["eT"].as<float>();
            j++;
            if (j >= Config::MAX_ZONE_TEMPS) {
                break;
            }
        }
        while (j < Config::MAX_ZONE_TEMPS) {
            config.zones[i].times[j] = {};
            j++;
        }
        i++;
        if (i >= Config::MAX_ZONES) {
            break;
        }
    }
    while (i < Config::MAX_ZONES) {
        config.zones[i] = {};
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
