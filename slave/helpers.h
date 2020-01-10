struct Acctuator {
    byte id;
    unsigned long dtReceived;
};

struct Handler {
    Acctuator acctuators[Config::MAX_SLAVE_ACCTUATORS] {};

    void handleTimeouts()
    {
        unsigned long currentTime = millis();
        for (auto & acctuator: acctuators) {
            if (acctuator.id > 0) {
                Serial.print(F("Id: "));
                Serial.print(acctuator.id);
                Serial.print(F(" Received: "));
                Serial.println(acctuator.dtReceived);
                if (currentTime - acctuator.dtReceived > Config::MAX_DELAY) {
                    Serial.print(F("Timeout: "));
                    Serial.println(acctuator.id);
                    analogWrite(acctuator.id, 0);
                    acctuator = {};
                }
            }
        }
    }

    void addTimeout(byte id)
    {
        for (auto & acctuator: acctuators) {
            if (acctuator.id == id) {
                acctuator.dtReceived = millis();
                return;
            }
        }
        for (auto & acctuator: acctuators) {
            if (acctuator.id == 0) {
                acctuator = {id, millis()};
                return;
            }
        }

    }
};

bool connectToRadio(RF24 & radio)
{
    Serial << F("Starting radio...") << endl;
    uint8_t retryRadio = 10;
    while (retryRadio-- > 0) {
        if (!radio.begin()) {
            Serial << F("Failed to initialize radio") << endl;
            delay(100);
        } else if (radio.isChipConnected()) {
            Serial << F("Initialized.") << endl;
            break;
        }
    }
    if (!radio.isChipConnected()) {
        return false;
    }
    const uint8_t slaveAddress[] = {Config::ADDRESS_SLAVE, 0, 0, 0, 0, 0};
    const uint8_t forwardAddress[] = {Config::ADDRESS_FORWARD, 0, 0, 0, 0, 0};

    radio.openReadingPipe(1, slaveAddress);
    radio.openReadingPipe(2, forwardAddress);
    radio.setChannel(RADIO_CHANNEL);
    radio.setDataRate(RF24_250KBPS);
    radio.setPALevel(RF24_PA_MAX);
    radio.startListening();
    return true;
}