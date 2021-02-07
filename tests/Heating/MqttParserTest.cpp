#include "catch.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Heating/MqttParser.h"

using Heating::parsePwmConfirmation;

SCENARIO( "test confirmation parsing", "[mqtt]" ) {

    GIVEN( "correct topic arives" ) {

        const char * topic = "stat/heating-slave/RESULT";
        const char * message = "{\"PWM\":{\"PWM1\":200,\"PWM2\":0,\"PWM3\":1023,\"PWM4\":3,\"pwm5\":1}}";

        WHEN("message is parsed") {
            uint8_t ids[10] {0};
            bool result = parsePwmConfirmation(topic, message, topic, ids, COUNT_OF(ids));
            REQUIRE(result);

            THEN("should retrieve ids") {
                int expected[] = {201, 203, 204};
                REQUIRE((int)ids[0] == expected[0]);
                REQUIRE((int)ids[1] == expected[1]);
                REQUIRE((int)ids[2] == expected[2]);
            }
        }
    }

    GIVEN( "incorrect topic arives" ) {

        const char * topic = "stat/heating-slave/RESULT";
        const char * message = "{\"PWM\":{\"PWM1\":200,\"PWM2\":0,\"PWM3\":1023,\"PWM4\":0,\"pwm5\":1}}";

        WHEN("message is parsed") {
            uint8_t ids[10] {0};
            bool result = parsePwmConfirmation(topic, message, "incorrectTopic", ids, COUNT_OF(ids));
            THEN("should ignore parsing") {
                REQUIRE_FALSE(result);
            }
        }
    }

    GIVEN( "incorrect message arives" ) {

        const char * topic = "stat/heating-slave/RESULT";
        auto message = GENERATE(as<std::string>{}, "someTestMessage", "{\"pwm\":{}}", "{\"pwm\":{\"PWM3\":1023}}", "{\"PW1\":1}");

        WHEN("message is parsed") {
            uint8_t ids[10] {0};
            bool result = parsePwmConfirmation(topic, message.c_str(), topic, ids, COUNT_OF(ids));
            THEN("should ignore parsing") {
                REQUIRE_FALSE(result);
            }
        }
    }
}
