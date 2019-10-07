#ifndef TIME_SERVICE
#define TIME_SERVICE

namespace TimeService
{
    unsigned long ntpUnixTime (UDP &udp, byte timeoutRetries);

    time_t getTime();
}

#endif
