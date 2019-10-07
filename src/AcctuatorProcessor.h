#ifndef ACCTUATOR_PROCESSOR
#define ACCTUATOR_PROCESSOR

namespace Heating
{
    class AcctuatorProcessor
    {
        private:
            const Config & config;
            ZoneInfo zones[Config::MAX_ZONES] {};
            Error errors[Config::MAX_GENERAL_ERRORS] {};

        public:
            AcctuatorProcessor(const Config & config);
            
            void handleStates();

            void handlePacket(const Packet & packet);

            void applyStates(Radio & radio, const HeaterInfo & heaterInfo);

            bool isAnyEnabled() const;
            bool isAnyWarmEnough() const;

            void printConfig(EthernetClient & client) const;
            void printInfo(EthernetClient & client, const HeaterInfo & heaterInfo) const;

            const ZoneConfig * getZoneConfigById(byte id) const;
            ZoneInfo & getAvailableZoneInfoById(byte id);

            void setExpectedTemperature(ZoneInfo & zoneInfo, float expectedTemperature);
    };
}

#endif
