#include "catch.hpp"
#include <Arduino.h>

#include "Config.h"

using Heating::Config;

#include "PacketHandler.h"

using Network::PacketHandler;
using Network::Session;
using Network::SessionHistory;
using Heating::Config;

using Catch::Matchers::Contains;


TEST_CASE( "packet encryption", "[packet]" ) {

    PacketHandler packetHandler;

    SECTION ( "save session" ) {

        REQUIRE(packetHandler.getSessionByAddress(Config::ADDRESS_SLAVE) == nullptr);

        packetHandler.saveSession(Config::ADDRESS_SLAVE, 111);

        REQUIRE(packetHandler.getSessionIdByAddress(Config::ADDRESS_SLAVE) == 111);

        packetHandler.saveSession(Config::ADDRESS_MASTER, 222);

        REQUIRE(packetHandler.getSessionIdByAddress(Config::ADDRESS_MASTER) == 222);

        REQUIRE(packetHandler.getSessionIdByAddress(Config::ADDRESS_SLAVE) == 111);


        packetHandler.saveSession(Config::ADDRESS_MASTER, 333);

        REQUIRE(packetHandler.getSessionIdByAddress(Config::ADDRESS_MASTER) == 333);
    }

    SECTION( "prepare session" ) {

        Session session {};
        char data [] = "test";

        packetHandler.prepareSession(session, Config::ADDRESS_SLAVE, data, sizeof(data));

        REQUIRE(session.id > 0);

        packetHandler.saveSession(Config::ADDRESS_SLAVE, 111);
        packetHandler.prepareSession(session, Config::ADDRESS_SLAVE, data, sizeof(data));

        REQUIRE(session.id != 111);

    }


    SECTION ( "no available sessions" ) {

        bool result = false;
        byte index = 1;
        char data [] = "test";
        Session session {};
        while (index <= Config::MAX_SESSIONS_AVAILABLE) {
            session.id = index;
            const byte * address = &index;
            packetHandler.saveSession(address, session.id);
            index++;
        }

        byte previousIndex = index - 1;
        const byte * address = &index;
        const byte * previousAddress = &previousIndex;
        REQUIRE(nullptr == packetHandler.getSessionByAddress(previousAddress));
        REQUIRE(nullptr == packetHandler.getSessionByAddress(address));
        packetHandler.saveSession(address, index);
        REQUIRE(nullptr != packetHandler.getSessionByAddress(address));

        byte one = 1;
        const byte * firstAddress = &one;
        REQUIRE(nullptr == packetHandler.getSessionByAddress(firstAddress));
    }

}
