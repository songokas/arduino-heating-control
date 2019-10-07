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

    struct ControllPacket {
        byte id;
        byte state;
        int info;
    };

    struct Packet {
        byte id;
        float currentTemperature;
        float expectedTemperature;
    };
    
    struct Pin {
        byte id;
        byte state;
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
