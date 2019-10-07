#ifndef RADIO
#define RADIO

namespace Heating
{

    using Network::PacketHandler;
    using Network::Session;
    using Network::Encryptor;

    enum RadioType { WRITE, READ };

    class Radio
    {
        private:
            bool started {false};
            bool payloadPrepared {false};
            byte startedAddress[6] = {0};
            RF24 & rf;
            PacketHandler & packetHandler;
            Encryptor & encryptor;
        public:
            Radio(RF24 & rf, PacketHandler & packetHandler, Encryptor & encryptor);
            void setUpTo(const RadioType & type, const byte * address);
            bool send(const byte * address, void * packet, size_t size, byte retry = 1);
            bool receive(const byte * address, void * packet, size_t size);
            bool read(void * data, size_t size);
            bool write(void * data, size_t size);
            bool preparePayload(void * data, size_t size);
            bool isAvailable(const byte * address);
            void powerUp();
            void powerDown();
    };
}

#endif
