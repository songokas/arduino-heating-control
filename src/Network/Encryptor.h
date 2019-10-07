#ifndef ENCRYPTOR_H
#define ENCRYPTOR_H

namespace Network
{
    class Encryptor
    {
        public:
            Encryptor(const char * key);
            bool decrypt(void * encryptedMessage, size_t len, void * data, size_t size);
            bool encrypt(const void * data, size_t size, void * encryptedMessage, size_t & maxLength);
        private:
            bool initialize();
            bool initialized = false;
            const uint8_t * keybuf = nullptr;
    };
}

#endif
