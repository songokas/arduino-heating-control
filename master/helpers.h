void printWebsite(EthernetClient & client, const AcctuatorProcessor & manager, const HeaterInfo & heaterInfo, uint8_t networkFailures)
{
    unsigned int buffIndex = 0;
    char website[Config::MAX_REQUEST_PRINT_BUFFER] {};
    unsigned int contentSize = strlen_P(websiteContent);
    for (unsigned int k = 0; k < contentSize; k++) {
        char c =  pgm_read_byte_near(websiteContent + k);
        if (c == '~') {
            client.write((uint8_t*)website, buffIndex);
            manager.printConfig(client);
            buffIndex = 0;
        } else if (c == '`') {
            client.write((uint8_t*)website, buffIndex);
            manager.printInfo(client, heaterInfo, networkFailures);
            buffIndex = 0;
        } else {
            website[buffIndex] = c;
            buffIndex++;
            if (buffIndex % Config::MAX_REQUEST_PRINT_BUFFER == 0) {
                client.write((uint8_t*)website, buffIndex);
                buffIndex = 0;
            }
        }
    }
    client.write((uint8_t*)website, buffIndex);
}

void sendHtml(EthernetClient & client, const AcctuatorProcessor & manager, const HeaterInfo & heaterInfo, uint8_t networkFailures)
{
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: text/html"));
    client.println(F("Connection: close"));  // the connection will be closed after completion of the response
    client.println();
    printWebsite(client, manager, heaterInfo, networkFailures);
}

void sendNotFound(EthernetClient & client)
{
    client.println(F("HTTP/1.1 404 NOT FOUND"));
    client.println(F("Content-Type: text/html"));
    client.println(F("Connection: close"));  // the connection will be closed after completion of the response
    client.println();
    client.println(F("NOT FOUND"));

}

void sendJson(EthernetClient & client, bool ok)
{
    if (ok) {
        client.println(F("HTTP/1.1 200 OK"));
    } else {
        client.println(F("HTTP/1.1 503 Service Unavailable"));
    }
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));  // the connection will be closed after completion of the response
    client.println();
    if (ok) {
        client.println(F("\"OK\""));
    } else {
        client.println(F("\"ERROR\""));
    }
}

bool handlePacket(AcctuatorProcessor & processor, const Packet & packet)
{
    printPacket(packet);
    if (isPinAvailable(packet.id, Config::AVAILABLE_PINS_MEGA, sizeof(Config::AVAILABLE_PINS_MEGA)/sizeof(Config::AVAILABLE_PINS_MEGA[0]))) {
        processor.handlePacket(packet);
        return true;
    } else {
        Serial << F("Ignoring packet since its pin is not available") << endl;
    }
    return false;
}

bool retrieveJson(JsonDocument & doc, const char * content)
{
    DeserializationError error = deserializeJson(doc, content);
    if (error) {
        Serial << F("deserializeJson() failed: ") << error.c_str() << endl;
        return false;
    } else {
        return true;
    }
    return false;
}

bool handleJson(AcctuatorProcessor & manager, const char * requestString, const char * pos)
{
    for (uint8_t i = 0; i < config.getZoneArrLength(); i++) {
        wdt_reset();
        auto & zone = config.getZone(i);
        if (!(zone.id > 0)) {
            continue;
        }
        char expectedTopic[MAX_LEN_TOPIC] {0};
        snprintf_P(expectedTopic, COUNT_OF(expectedTopic), HEATING_TOPIC, zone.getName());

        if (strstr(requestString, expectedTopic) > 0) {
            StaticJsonDocument<MAX_LEN_JSON_MESSAGE> doc;
            if (retrieveJson(doc, pos + 4)) {
                if (doc["temperature"].is<int16_t>()) {
                    Packet packet {zone.id, doc["temperature"]};
                    return handlePacket(manager, packet);
                } else {
                    Serial << F("Invalid json structure") << endl;
                }
            }
            return false;
        }
    }
    return false;
}

bool handleRequest(EthernetClient & client, Storage & storage, AcctuatorProcessor & manager, const HeaterInfo & heaterInfo, uint8_t networkFailures, const Config & config)
{
    bool configUpdated = false;
    char endOfHeaders[] = "\r\n\r\n";
    if (client) {
        unsigned int i = 0;
        char * requestString = new char[Config::MAX_REQUEST_SIZE + 1] {};
        while (client.available() && i < Config::MAX_REQUEST_SIZE) {
            requestString[i] = client.read();
            i++;
        }
        requestString[i] = '\0';
        if (strstr(requestString, "/clear/") > 0) {

            Serial << F("Received clear") << endl;
            configUpdated = storage.saveConfiguration("0");
            sendJson(client, configUpdated);

        } else if (strstr(requestString, "/heating/") > 0) {

            Serial << F("Received heating") << endl;

            bool success = false;
            char * pos = strstr(requestString, endOfHeaders);
            if (pos) {
                success = handleJson(manager, requestString, pos);
            }
            sendJson(client, success);

        } else if (strstr(requestString, "application/json") > 0) {

            Serial.println(F("Received ajax"));
            char * pos = strstr(requestString, endOfHeaders);
            if (pos) {
                configUpdated = storage.saveConfiguration(pos + 4);
            } else {
                configUpdated = false;
            }
            sendJson(client, configUpdated);

        } else {

            Serial << F("Send html") << endl;
            sendHtml(client, manager, heaterInfo, networkFailures);

        }
        delete requestString;
        delay(10);
        client.stop();
        delay(10);
        Serial << F("Client disconnected") << endl;
    }
    return configUpdated;
}



bool handleRadio(IEncryptedMesh & radio, AcctuatorProcessor & processor)
{
    if (radio.isAvailable()) {
        Packet received;
        RF24NetworkHeader header;
        if (!radio.receive(&received, sizeof(received), 0, header)) {
            Serial.println(F("Failed to receive packet on master"));
            printPacket(received);
            return false;
        }
        return handlePacket(processor, received);
    }
    return false;
}


void handleHeater(AcctuatorProcessor & processor, HeaterInfo & heaterInfo)
{
    if (processor.isAnyWarmEnough()) {
        if (!heaterInfo.isOn()) {
            heaterInfo.markOn();
            digitalWrite(Config::PIN_HEATING, LOW);
        }
    } else {
        if (heaterInfo.isOn()) {
            heaterInfo.markOff();
            Serial.println(F("Disabling heating"));
            digitalWrite(Config::PIN_HEATING, HIGH);
        }
    }
}

bool syncTime(NTPClient & client)
{
    unsigned long utc = client.getEpochTime();
    if (utc == 0) {
        return false;
    }
    TimeChangeRule eestSummer = {"EESTS", Last, Sun, Mar, 3, 180};  //UTC + 3 hours
    TimeChangeRule eestWinter = {"EESTW", Last, Sun, Oct, 4, 120};   //UTC + 2 hours
    Timezone eestZone(eestSummer, eestWinter);
    time_t localTime = eestZone.toLocal(utc);
    setTime(localTime);
    return true;
}

bool maintainDhcp() {

  switch (Ethernet.maintain())
  {
    case 1:
      //renewed fail
      Serial.println(F("Error: renewed fail"));
      return false;
      break;

    case 2:
      //renewed success
      Serial.println(F("Renewed success"));
      Serial.println(Ethernet.localIP());
      break;

    case 3:
      //rebind fail
      Serial.println(F("Error: rebind fail"));
      return false;
      break;

    case 4:
      //rebind success
      Serial.println(F("Rebind success"));
      Serial.println(Ethernet.localIP());
      break;

    default:
      //nothing happened
      break;

  }
  return true;
}

bool connectToMqtt(const char * clientName, PubSubClient & client) {
    if (!client.connected()) {
        wdt_reset();
        Serial << F("Attempting MQTT connection...");
        if (client.connect(clientName)) {
            Serial << F("Mqtt connected") << endl;
            return true;

        } else {
            Serial << F("Mqtt connection failed, rc=") << client.state() << F(" try again in 5 seconds") << endl;
            return false;
        }
    }
    return true;
}

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

    const uint8_t address[] = {Config::ADDRESS_MASTER, 0, 0, 0, 0, 0};
    radio.openReadingPipe(1,address);
    radio.setChannel(RADIO_CHANNEL);
    radio.setDataRate(RF24_250KBPS);
    radio.setPALevel(RF24_PA_MAX);
    radio.startListening();
    return true;
}

void mqttCallback(const char * topic, unsigned char * payload, unsigned int len)
{
    Serial << F("Mqtt message received for: ") << topic << endl;
    char message[MAX_LEN_MESSAGE];
    memcpy(message, payload, MIN(len, COUNT_OF(message)));

    bool handled = false;
    for (uint8_t i = 0; i < config.getZoneArrLength(); i++) {
        auto & zone = config.getZone(i);
        if (!(zone.id > 0)) {
            continue;
        }
        char expectedTopic[MAX_LEN_TOPIC] {0};
        snprintf_P(expectedTopic, COUNT_OF(expectedTopic), HEATING_TOPIC, zone.getName());
        if (strncmp(expectedTopic, topic, COUNT_OF(expectedTopic)) == 0) {
            int16_t value = atoi(message);
            Packet packet { zone.id, value };
            handled = handlePacket(processor, packet);
            break;
        }
    }

    if (!handled) {
        Serial << "Not handled: " << topic << endl; 
    }
}