#ifndef HEATING_COMMON_H
#define HEATING_COMMON_H

namespace Heating
{
    enum class Error : char {
        NONE,
        CONTROL_PACKET_FAILED,
        RECEIVE_FAILED,
        RECEIVE_WRONG_ID_COUNT
    };
#pragma pack (1)
    struct ControllPacket
    {
        uint8_t id {0};
        uint8_t state {0};

        ControllPacket()
        {}

        ControllPacket(uint8_t id, uint8_t state): id(id), state(state)
        {}
    };
#pragma pack (0)

#pragma pack (1)
    struct Packet {
        uint8_t id {0};
        int16_t currentTemperature {0};
        int16_t expectedTemperature {0};

        Packet()
        {}

        Packet(uint8_t id, int16_t currentTemperature): id(id), currentTemperature(currentTemperature)
        {}

        Packet(uint8_t id, int16_t currentTemperature, int16_t expectedTemperature): id(id), currentTemperature(currentTemperature), expectedTemperature(expectedTemperature)
        {}
    };
#pragma pack (0)

    struct Pin
    {
        uint8_t id {0};
        uint8_t state {0};

        Pin()
        {}

        Pin(uint8_t id, uint8_t state): id(id), state(state)
        {}
    };

    struct OnOffTime
    {
        time_t dtOn {0};
        time_t dtOff {0};

        OnOffTime()
        {}

        OnOffTime(time_t dtOn, time_t dtOff): dtOn(dtOn), dtOff(dtOff)
        {}
    };

    void printTime(time_t t);

    void printPacket(const Packet & packet);

    bool isPinAvailable(byte pin, const byte availablePins[], byte arrayLength);

    /**
     * adjust in milliseconds
     */
    unsigned long timer(unsigned int ajust = 0, bool reset = false);
}
#endif
