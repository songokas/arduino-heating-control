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

bool handleRequest(EthernetClient & client, Storage & storage, AcctuatorProcessor & manager, const HeaterInfo & heaterInfo, uint8_t networkFailures)
{

    bool configUpdated = false;
    if (client) {
        if (client.connected()) {
            unsigned int i = 0;
            char * requestString = new char[Config::MAX_REQUEST_SIZE + 1] {};
            while (client.available() && i < Config::MAX_REQUEST_SIZE) {
                requestString[i] = client.read();
                i++;
            }
            requestString[i] = '\0';
            if (strstr(requestString, "/clear/") > 0) {
                Serial.println(F("Send clear"));
                configUpdated = storage.saveConfiguration("0");
                sendJson(client, configUpdated);
            } else if (strstr(requestString, "application/json") > 0) {
                Serial.println(F("Send ajax"));
                char * pos = strstr(requestString, "\r\n\r\n");
                if (pos) {
                    configUpdated = storage.saveConfiguration(pos + 4);
                } else {
                    configUpdated = false;
                }
                sendJson(client, configUpdated);
            } else {
                Serial.println(F("Send html"));
                sendHtml(client, manager, heaterInfo, networkFailures);
            }
            delete requestString;
            delay(10);
            client.stop();
            Serial.println(F("Client disconnected"));
        }
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

        printPacket(received);
        if (isPinAvailable(received.id, Config::AVAILABLE_PINS_MEGA, sizeof(Config::AVAILABLE_PINS_MEGA)/sizeof(Config::AVAILABLE_PINS_MEGA[0]))) {
            processor.handlePacket(received);
            return true;
        } else {
            Serial.println(F("Ignoring packet since its pin is not available"));
        }
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


enum class ConnectionStatus
{
    Connected,
    FailedToConnect
};

ConnectionStatus reconnectToMqtt(PubSubClient & client) {
    // Loop until we're reconnected
    if (!client.connected()) {
        DPRINTLN(F("Attempting MQTT connection..."));
        // Attempt to connect
        if (client.connect("ESP8266Client")) {
            DPRINTLN(F("Mqtt connected"));
            return ConnectionStatus::Connected;

        } else {
            Serial << F("Mqtt connection failed, rc=") << client.state() << F(" try again in 5 seconds") << endl;
            return ConnectionStatus::FailedToConnect;
        }
    }
    return ConnectionStatus::Connected;
}
