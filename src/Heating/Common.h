#ifndef COMMON_H
#define COMMON_H

namespace Heating
{
    enum class Error : char {
        NONE,
        CONTROL_PACKET_FAILED,
        RECEIVE_FAILED,
        RECEIVE_WRONG_ID_COUNT
    };
#pragma pack (1)
    struct ControllPacket {
        uint8_t id;
        uint8_t state;
    };
#pragma pack (0)

#pragma pack (1)
    struct Packet {
        uint8_t id;
        int16_t currentTemperature;
        int16_t expectedTemperature;
    };
#pragma pack (0)

    struct Pin {
        uint8_t id;
        uint8_t state;
    };

    struct OnOffTime {
        time_t dtOn;
        time_t dtOff;
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
