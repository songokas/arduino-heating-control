#include <Arduino.h>
#include <xxtea-iot-crypt.h>

#include "Encryptor.h"

using Network::Encryptor;

Encryptor::Encryptor(const char * key)
{
    keybuf = reinterpret_cast<const uint8_t*>(key);
}

bool Encryptor::initialize()
{
    if (initialized) {
        return true;
    }
    if(xxtea_setup(keybuf, strlen((char *)keybuf)) != XXTEA_STATUS_SUCCESS) {
        Serial.println(F("Encryption setup failed!"));
        return false;
    }
    initialized = true;
    return true;
}

bool Encryptor::encrypt(const void * data, size_t size, void * encryptedMessage, size_t & maxLength)
{
    if (!initialize()) {
        return false;
    }
    const uint8_t * original = reinterpret_cast<const uint8_t*>(data);
    uint8_t * encrypted = reinterpret_cast<uint8_t*>(encryptedMessage);

    if(xxtea_encrypt(original, size, encrypted, &maxLength) != XXTEA_STATUS_SUCCESS) {
        Serial.println(F("Encryption Failed!"));
        return false;
    }
    return true;
}

bool Encryptor::decrypt(void * encryptedMessage, size_t len, void * data, size_t size)
{
    if (!initialize()) {
        return false;
    }
    uint8_t * encrypted = reinterpret_cast<uint8_t*>(encryptedMessage);
    uint8_t * output = reinterpret_cast<uint8_t*>(data);
    if(xxtea_decrypt(encrypted, len, output, size) != XXTEA_STATUS_SUCCESS) {
        Serial.println(F("Decryption Failed!"));
        return false;
    }
    return true;
}


