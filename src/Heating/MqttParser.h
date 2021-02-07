#ifndef HEATING_MQTT_PARSER_H
#define HEATING_MQTT_PARSER_H

// #include <Streaming.h>

#include "CommonModule/MacroHelper.h"
#include "MqttModule/MqttConfig.h"
#include "Heating/Config.h"

namespace Heating
{

    bool inline retrieveJson(JsonDocument & doc, const char * content)
    {
        DeserializationError error = deserializeJson(doc, content);
        if (error) {
            #if ARDUINO
                Serial << F("deserializeJson() failed: ") << error.c_str() << endl;
            #else
                std::cout << F("deserializeJson() failed: ") << error.c_str() << std::endl;
            #endif
            return false;
        }
        return true;
    }

    // parse pwm confirmation from tasmota
    bool inline parsePwmConfirmation(const char * topic, const char * message, const char * expectedChannel, uint8_t * ids, uint8_t idSize)
    {
        if (strncmp(expectedChannel, topic, strlen(expectedChannel)) != 0) {
            return false;
        }
        StaticJsonDocument<MAX_LEN_JSON_MESSAGE> doc;

        if (!retrieveJson(doc, message)) {
            return false;
        }

        auto json = doc["PWM"].as<JsonObject>();
        uint8_t i {0};
        for (const auto & pwm: json) {
            if (i >= idSize) {
                break;
            }
            if (!pwm.value().is<int16_t>()) {
                continue;
            }

            int16_t val = pwm.value().as<int16_t>();
            if (!(val > 0)) {
                continue;
            }
            auto title = pwm.key().c_str();
            int pwmIndex {0};
            sscanf(title, "PWM%d", &pwmIndex);
            if (!(pwmIndex > 0)) {
                continue;
            }

            uint8_t id = pwmIndex + Config::TASMOTA_SLAVE_PIN_START;
      
            ids[i] = id;
            i++;
            
        }
        return i > 0;
    }
}

#endif