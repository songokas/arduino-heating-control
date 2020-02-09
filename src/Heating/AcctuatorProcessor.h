#ifndef ACCTUATOR_PROCESSOR
#define ACCTUATOR_PROCESSOR


namespace RadioEncrypted { class IEncryptedMesh; }

using RadioEncrypted::IEncryptedMesh;

typedef ZoneInfo States[Config::MAX_ZONES];

namespace Heating
{
    class AcctuatorProcessor
    {
        public:
            AcctuatorProcessor(const Config & config);
            
            void handleStates();

            void handlePacket(const Packet & packet);

            bool applyState(ZoneInfo & state, IEncryptedMesh & radio, const HeaterInfo & heaterInfo);

            States & getStates();

            bool isAnyEnabled() const;
            bool isAnyWarmEnough() const;

            void printConfig(EthernetClient & client) const;
            void printInfo(EthernetClient & client, const HeaterInfo & heaterInfo, uint8_t networkFailures) const;

            const ZoneConfig * getZoneConfigById(byte id) const;
            ZoneInfo & getAvailableZoneInfoById(byte id);

            void setExpectedTemperature(ZoneInfo & zoneInfo, float expectedTemperature);

            bool hasReachedBefore(byte id);
            void saveReached(byte id, bool reached);
        private:
            const Config & config;
            States zones {};
            Error errors[Config::MAX_GENERAL_ERRORS] {};
    };
}

#endif
