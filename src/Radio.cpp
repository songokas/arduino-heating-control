#include <nRF24L01.h>
#include <RF24.h>
#include <printf.h>
#include <Time.h>
#include "Common.h"
#include "Config.h"
#include "Encryptor.h"
#include "PacketHandler.h"

#include "Radio.h"

using Heating::Radio;
using Network::Session;
using Network::Encryptor;

Radio::Radio(RF24 & rf24, PacketHandler & packetHandler, Encryptor & encryptor): rf(rf24), packetHandler(packetHandler), encryptor(encryptor)
{
}

void Radio::setUpTo(const RadioType & type, const byte * address)
{
    if (!started) {
        rf.begin();
        rf.setChannel(108);
        rf.setDataRate(RF24_250KBPS);
        rf.setPALevel(RF24_PA_HIGH);
        //rf.setAutoAck(true);
        //rf.enableAckPayload();
        //rf.enableDynamicPayloads();
        //rf.setRetries(6,3);

        started = true;
        Serial.println(F("Radio started"));
    }
    if (type == WRITE) {
        if (memcmp(startedAddress, address, sizeof(startedAddress)) != 0) {
            Serial.print(F("Radio writing: "));
            Serial.println((const char *)address);

            //rf.flush_rx();
            //rf.flush_tx();

            rf.stopListening();
            rf.openWritingPipe(address);
            memcpy(startedAddress, address, sizeof(startedAddress));
        }
    } else {
        if (memcmp(startedAddress, address, sizeof(startedAddress)) != 0) {
            Serial.print(F("Radio listening: "));
            Serial.println((const char *)address);

            rf.openReadingPipe(1, address);
            memcpy(startedAddress, address, sizeof(startedAddress));
            //if (!payloadPrepared) {
                //unsigned long useSessionId = millis();
                //rf.writeAckPayload(1, &useSessionId, sizeof(useSessionId));
                //payloadPrepared = true;
            //}
            //rf.flush_rx();
            //rf.flush_tx();
            rf.startListening();


        }
    }
}

bool Radio::send(const byte * address, void * data, size_t size, byte retry)
{
    if (size > Config::MAX_SESSION_DATA_SIZE) {
        Serial.println(F("Can not encrypt/decrypt so many bytes"));
        return false;
    }

    setUpTo(WRITE, address);

    Session session;
    packetHandler.prepareSession(session, address, data, size);

    if (!write(&session, sizeof(session))) {
        Serial.println(F("Failed to send packet"));
        return false;
    }

    /*if (!rf.isAckPayloadAvailable()) {
        Serial.println(F("Ack payload not available"));
        return false;
    }

    byte payloadSize = rf.getDynamicPayloadSize();
    
    unsigned long sessionIdToUse = 0;
    if (payloadSize != sizeof(sessionIdToUse)) {
        Serial.println(F("Payload size does not match"));
        Serial.println(payloadSize);
        return false;
    }

    rf.read(&sessionIdToUse, sizeof(sessionIdToUse));
    Serial.print(F("Ack session id received:"));
    Serial.println(sessionIdToUse);


    packetHandler.saveSession(address, sessionIdToUse);

    if (!packetHandler.sessionMatches(session.id, sessionIdToUse)) {
        if (!(retry > 0)) {
            Serial.println(F("Received and sent session id: "));
            Serial.println(sessionIdToUse);
            Serial.println(session.id);
            Serial.println(F("Retry failed"));
            return false;
        }
        Serial.println(F("Received and sent session id: "));
        Serial.println(sessionIdToUse);
        Serial.println(session.id);
        Serial.println(F("Retry send"));
        retry--;
        return send(address, data, size, retry);
    }*/
    return true;
}

bool Radio::isAvailable(const byte * address)
{
    setUpTo(READ, address);
    return rf.available();
}

bool Radio::receive(const byte * address, void * data, size_t size)
{
    if (size > Config::MAX_SESSION_DATA_SIZE) {
        Serial.println(F("Can not encrypt/decrypt so many bytes"));
        return false;
    }

    setUpTo(READ, address);

    Session session {};
    bool result = read(&session, sizeof(session));
    if (result ) {
        memcpy(data, session.data, size);
        return true;
    }
    // prepare next
    /*unsigned long useSessionId = millis();
    rf.writeAckPayload(1, &useSessionId, sizeof(useSessionId));

        Serial.print(F("Ack session id sent:"));
        Serial.println(useSessionId);
        Serial.print(F("Received session id:"));
        Serial.println(session.id);

    if (!result) {
        Serial.println(F("Failed to read packet"));
        return false;
    }

    if (packetHandler.sessionMatches(session.id, useSessionId)) {
        memcpy(data, session.data, size);
        return true;
    } else {
        // @TODO remove. testing purposes
        memcpy(data, session.data, size);
        Serial.println(F("Sent and received session id: "));
        Serial.println(useSessionId);
        Serial.println(session.id);
    }*/
    return false;
}

bool Radio::read(void * data, size_t size)
{
    size_t diff = sizeof(Session) % 4;
    size_t add = diff > 0 ? 4 - diff : 0;
    byte mainPacket[sizeof(Session) + add] {};
    rf.read(&mainPacket, sizeof(mainPacket));

    if (!encryptor.decrypt(&mainPacket, sizeof(mainPacket), data, size)) {
        Serial.println(F("Packet received decryption failed"));
        return false;
    }
    return true;
}

bool Radio::write(void * data, size_t size)
{   
    size_t diff = sizeof(Session) % 4;
    size_t add = diff > 0 ? 4 - diff : 0;
    char packetToSend[sizeof(Session) + add] {};
    size_t maxLength = sizeof(packetToSend);
    if (!encryptor.encrypt(data, size, &packetToSend, maxLength)) {
        Serial.print(F("Failed to encrypt packet. Size: "));
        Serial.print(size);
        Serial.print(F(" Length: "));
        Serial.println(maxLength);
        Serial.print(F(" Structure size: "));
        Serial.println(sizeof(packetToSend));
        return false;
    }
    return rf.write(&packetToSend, sizeof(packetToSend));
}

void Radio::powerUp()
{
    rf.powerUp();
}

void Radio::powerDown()
{
    rf.powerDown();
}
