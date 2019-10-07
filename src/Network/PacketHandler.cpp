#include <Arduino.h>

#include "Config.h"
#include "Encryptor.h"
#include "PacketHandler.h"

using Network::PacketHandler;
using Network::SessionHistory;
using Network::Session;

PacketHandler::PacketHandler()
{}

void PacketHandler::prepareSession(Session & session, const byte * address, const void * data, size_t dataSize)
{
    /*SessionHistory * existingSession = getSessionByAddress(address);
    if (existingSession) {
        session.id = existingSession->sessionId;
    }*/
    session.id = random(2000UL, 200000000UL);
    memcpy(session.data, data, dataSize);
}

void PacketHandler::saveSession(const byte * address, unsigned long sessionId)
{

    SessionHistory * session = getSessionByAddress(address);
    if (!session) {
        Serial.println(F("Using new session"));
        session = getFree();
        memcpy(session->address, address, sizeof(session->address));
    }
    Serial.println(F("Current session id: "));
    Serial.println(session->sessionId);

    session->sessionId = sessionId;
}


bool PacketHandler::sessionMatches(unsigned long receivedSessionId, unsigned long actualSessionId) const
{
    return true;
    //unsigned long period = 60000UL;
    //unsigned long sessionCmp = actualSessionId > period ? actualSessionId - period : 0;
    //return receivedSessionId > sessionCmp && receivedSessionId <= actualSessionId;
}

SessionHistory * PacketHandler::getSessionByAddress(const byte * address)
{
    for (auto & session: sessions) {
        if (memcmp(session.address, address, sizeof(session.address)) == 0) {
            return &session;
        }
    }
    return nullptr;
}


unsigned long PacketHandler::getSessionIdByAddress(const byte * address)
{
    SessionHistory * session = getSessionByAddress(address);
    return session != nullptr ? session->sessionId : 0;
}

void PacketHandler::printAllSessions() const
{
    for (const auto & session: sessions) {
        Serial.print("Session: ");
        Serial.print(session.sessionId);
        Serial.print(" Address: ");
        Serial.println((const char *)session.address);
    }
}

SessionHistory * PacketHandler::getFree()
{
    byte index = 0;
    byte i = 0;
    unsigned long max = 4294967295UL;
    for (auto & session: sessions) {
        if (session.sessionId == 0) {
            return &session;
        }
        if (max > session.sessionId) {
            max = session.sessionId;
            index = i;
        }

        i++;
    }
    return &sessions[index];
}
