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

