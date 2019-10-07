#include "catch.hpp"
#include <string.h>

#include "Encryptor.h"

using Network::Encryptor;
using Catch::Matchers::Contains;

struct TestStruct
{
    unsigned long id;
    char data[9];
    bool verify;
};

TEST_CASE( "encrypt data", "[encryptor]" ) {

    Encryptor encryptor("test key");

    SECTION( "encrypt a string" ) {
        const char data[] = "content to be encrypted as";
        char encryptedBuffer[30] {};
        size_t maxLength = 30;
        bool result = encryptor.encrypt(data, sizeof(data), encryptedBuffer, maxLength);
        REQUIRE(result == true);
        //4 bytes spacing
        REQUIRE(maxLength == 28);
        REQUIRE_THAT(encryptedBuffer, !Contains(data));

        SECTION( "decrypt a string" ) {
            char decryptedBuffer[30] {};
            result = encryptor.decrypt(encryptedBuffer, 28, decryptedBuffer, 28);
            REQUIRE(result == true);
            REQUIRE_THAT(decryptedBuffer, Contains(data));
        }
    }

    SECTION( "encrypt a struct" ) {
        TestStruct data {10UL, "message", true};
        uint8_t encryptedBuffer[30] {};
        size_t maxLength = 30;
        size_t diff = sizeof(TestStruct) % 4;
        size_t add = diff > 0 ? 4 - diff : 0;
        size_t expectedLength = sizeof(TestStruct) + add;
        //REQUIRE(expectedLength == 24);
        //REQUIRE(14 == sizeof(TestStruct));
        REQUIRE (expectedLength <= maxLength);
        bool result = encryptor.encrypt(&data, sizeof(data), (void*)encryptedBuffer, maxLength);
        REQUIRE(result == true);
        //4 bytes spacing
        REQUIRE(maxLength == expectedLength);

        SECTION( "decrypt a struct" ) {
            TestStruct decryptedStruct {};
            result = encryptor.decrypt((void*)encryptedBuffer, expectedLength, &decryptedStruct, sizeof(decryptedStruct));
            REQUIRE(result == true);
            REQUIRE(decryptedStruct.id == 10UL);
            REQUIRE_THAT(decryptedStruct.data, Contains("message"));
            REQUIRE(decryptedStruct.verify == true);
        }
    }

}



