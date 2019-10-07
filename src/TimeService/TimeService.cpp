#include <Arduino.h>
#include <Ethernet.h>
#include <Time.h>
#include <Timezone.h>
#include <avr/wdt.h>
#include "Config.h"
#include "TimeService.h"

namespace TimeService
{
    using Heating::Config;

    EthernetUDP udpConnection;

    //TimeChangeRule * tcr;

    time_t getTime()
    {
        wdt_disable();
        unsigned long utc = ntpUnixTime(udpConnection, 15);
        wdt_enable(WDTO_8S);
        if (utc == 0) {
            return 0;
        }
        TimeChangeRule eestSummer = {"EESTS", Last, Sun, Mar, 3, 180};  //UTC + 3 hours
        TimeChangeRule eestWinter = {"EESTW", Last, Sun, Oct, 4, 120};   //UTC + 2 hours
        Timezone eestZone(eestSummer, eestWinter);
        return eestZone.toLocal(utc);
    }

    /*
    * © Francesco Potortì 2013 - GPLv3 - Revision: 1.13
    *
    * Send an NTP packet and wait for the response, return the Unix time
    *
    * To lower the memory footprint, no buffers are allocated for sending
    * and receiving the NTP packets.  Four bytes of memory are allocated
    * for transmision, the rest is random garbage collected from the data
    * memory segment, and the received packet is read one byte at a time.
    * The Unix time is returned, that is, seconds from 1970-01-01T00:00.
    */
    unsigned long ntpUnixTime (UDP &udp, byte timeoutRetries)
    {
        static int udpInited = udp.begin(123); // open socket on arbitrary port
        if (!udpInited) {
            Serial.print("failed to initialize");
        }

        wdt_reset();

        const char timeServer[] = "pool.ntp.org";  // NTP server

        // Only the first four bytes of an outgoing NTP packet need to be set
        // appropriately, the rest can be whatever.
        const long ntpFirstFourBytes = 0xEC0600E3; // NTP request header

        // Fail if WiFiUdp.begin() could not init a socket
        if (! udpInited)
            return 0;

        // Clear received data from possible stray received packets
        udp.flush();

        // Send an NTP request
        if (!udp.beginPacket(timeServer, 123)) {
            Serial.print("failed to begin");
                return 0;  
        }
        if (udp.write((byte *)&ntpFirstFourBytes, 48) != 48) {
                Serial.print("failed to write");
                return 0;  
        }
        if (!udp.endPacket()) {
                Serial.print("failed to end");
            return 0;  
            // sending request failed
        }

        // Wait for response; check every pollIntv ms up to maxPoll times
        const int pollIntv = 150;   // poll every this many ms
        const byte maxPoll = timeoutRetries;    // poll up to this many times
        int pktLen;       // received packet length
        for (byte i=0; i<maxPoll; i++) {
            wdt_reset();
            pktLen = udp.parsePacket();
            if (pktLen == 48) {
                break;
            }
            delay(pollIntv);
        }
        if (pktLen != 48) {
            Serial.println("no correct packet received");
            Serial.println(pktLen);
            return 0;       // no correct packet received
        }

        // Read and discard the first useless bytes
        // Set useless to 32 for speed; set to 40 for accuracy.
        const byte useless = 40;
        for (byte i = 0; i < useless; ++i)
            udp.read();

        // Read the integer part of sending time
        unsigned long time = udp.read();  // NTP time
        for (byte i = 1; i < 4; i++)
            time = time << 8 | udp.read();

        // Round to the nearest second if we want accuracy
        // The fractionary part is the next byte divided by 256: if it is
        // greater than 500ms we round to the next second; we also account
        // for an assumed network delay of 50ms, and (0.5-0.05)*256=115;
        // additionally, we account for how much we delayed reading the packet
        // since its arrival, which we assume on average to be pollIntv/2.
        time += (udp.read() > 115 - pollIntv/8);

        // Discard the rest of the packet
        udp.flush();
        return time - 2208988800ul;   // convert NTP time to Unix time
    }
}

