#ifndef HEATING_ACCTUATOR_PROCESSOR_H
#define HEATING_ACCTUATOR_PROCESSOR_H

#include "Domain/ZoneInfo.h"

namespace RadioEncrypted { class IEncryptedMesh; }

using RadioEncrypted::IEncryptedMesh;
using Heating::Domain::ZoneInfo;
using Heating::Domain::StaticZoneInfo;

namespace Heating
{
    class AcctuatorProcessor
    {
        public:

            AcctuatorProcessor(const Config & config);
            
            void handleStates();

            void handlePacket(const Packet & packet);

            bool applyState(ZoneInfo & state, IEncryptedMesh & radio, const HeaterInfo & heaterInfo);

            bool isAnyEnabled() const;
            bool isAnyWarmEnough() const;

            void printConfig(EthernetClient & client) const;
            void printInfo(EthernetClient & client, const HeaterInfo & heaterInfo, uint8_t networkFailures) const;

            const ZoneConfig * getZoneConfigById(byte id) const;
            ZoneInfo & getAvailableZoneInfoById(byte id);

            void setExpectedTemperature(ZoneInfo & zoneInfo, float expectedTemperature);

            bool hasReachedBefore(byte id);
            void saveReached(byte id, bool reached);

            virtual ZoneInfo & getState(uint8_t i) = 0;
            virtual const ZoneInfo & getState(uint8_t i) const = 0;
            virtual uint8_t getStateArrLength() const = 0;

        private:
            const Config & config;
    };
    
    template<uint8_t maxZones, uint8_t maxTemps, uint8_t maxHistory, uint8_t maxZoneErrors>
    class StaticAcctuatorProcessor: public AcctuatorProcessor
    {
        public:
           StaticAcctuatorProcessor(const Config & config): AcctuatorProcessor(config) {}
           ZoneInfo & getState(uint8_t i) { return i >= maxZones ? zones[maxZones - 1] : zones[i]; }
           const ZoneInfo & getState(uint8_t i) const { return i >= maxZones ? zones[maxZones - 1] : zones[i]; }
           uint8_t getStateArrLength() const { return maxZones; };
        private:
            StaticZoneInfo<maxTemps, maxHistory, maxZoneErrors> zones[maxZones];
    };
}

#endif
