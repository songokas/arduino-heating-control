#ifndef PACKET_HANDLER_H
#define PACKET_HANDLER_H


namespace Network
{

    using Heating::Config;

    struct Session
    {
        unsigned long id;
        byte data[Config::MAX_SESSION_DATA_SIZE];
    };

    struct SessionHistory
    {
        byte address[6];
        unsigned long sessionId;
    };

    class PacketHandler
    {
        public:
            PacketHandler();

            void prepareSession(Session & session, const byte * address, const void * data, size_t dataSize);

            void saveSession(const byte * address, unsigned long sessionId);

            bool sessionMatches(unsigned long receivedSessionId, unsigned long actualSessionId) const;

            void printAllSessions() const;

            SessionHistory * getSessionByAddress(const byte * address);

            unsigned long getSessionIdByAddress(const byte * address);

        private:

            SessionHistory * getFree();

            SessionHistory sessions[Config::MAX_SESSIONS_AVAILABLE] {};

    };
}

#endif
