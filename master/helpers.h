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
    //client.println(F("OK"));
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
    if (client) {
        char endOfHeaders[] = "\r\n\r\n";

        Serial << F("Handle request memory left:") << freeRam() << endl;

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

bool syncTimeZoneTime(unsigned long utc)
{
    TimeChangeRule eestSummer = {"EESTS", Last, Sun, Mar, 3, 180};  //UTC + 3 hours
    TimeChangeRule eestWinter = {"EESTW", Last, Sun, Oct, 4, 120};   //UTC + 2 hours
    Timezone eestZone(eestSummer, eestWinter);
    time_t localTime = eestZone.toLocal(utc);
    setTime(localTime);
    return true;
}


bool maintainDhcp()
{

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

bool connectToMqtt(PubSubClient & client, const char * clientName, const char * subscribeTopic)
{
    if (!client.connected()) {
        Serial << F("Attempting MQTT connection...");
        uint16_t mqttAttempts = 0;
        while (!client.connect(clientName) && mqttAttempts < 6) {
            mqttAttempts++;
            Serial.print(".");
            delay(500 * mqttAttempts);
            wdt_reset();
        }
        if (mqttAttempts >= 6) {
            Serial << F("Mqtt connection failed, rc=") << client.state() << F(" try again in 5 seconds") << endl;
            return false;
        } else {
            Serial << F("connected.") << endl;
            if (client.subscribe(subscribeTopic)) {
                Serial << F("Subscribed to: ") << SUBSCRIBE_TOPIC << endl;
            } else {
                Serial << F("Failed to subscribe to: ") << SUBSCRIBE_TOPIC << endl;
            }
        }
        wdt_reset();
    }
    return true;
}

bool sendKeepAlive(PubSubClient & client, const char * clientName,  const char * keepAliveTopic)
{
    char topic[MAX_LEN_TOPIC] {0};
    snprintf_P(topic, COUNT_OF(topic), keepAliveTopic);
    char liveMsg[16] {0};
    sprintf(liveMsg, "%lu", millis());
    Serial << F("Mqtt send ") << topic << F(" ") << liveMsg << endl;
    return client.publish(topic, liveMsg);
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
        if (strncmp(expectedTopic, topic, strlen(expectedTopic)) == 0) {
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

#ifdef NTP_TIME

bool syncTime(NTPClient & client)
{
    unsigned long utc = client.getEpochTime();
    if (utc == 0) {
        return false;
    }
    return syncTimeZoneTime(utc);
}

#else

void rtcSetup(RtcDS1302<ThreeWire> & rtc)
{
    rtc.Begin();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

    if (!rtc.IsDateTimeValid()) {
        //if (rtc.LastError() != 0) {
        //    Serial << F("RTC communications error = ") << rtc.LastError() << endl;
        //} else {
            Serial << F("RTC lost confidence in the DateTime!") << endl;
            rtc.SetDateTime(compiled);
        //}
    }

    if (!rtc.GetIsRunning()) {
        Serial << F("RTC starting") << endl;
        rtc.SetIsRunning(true);
    }

    RtcDateTime now = rtc.GetDateTime();
    if (now < compiled) {
        Serial << F("RTC update with compile time") << endl;;
        rtc.SetDateTime(compiled);
    }

    // never assume the Rtc was last configured by you, so
    // just clear them to your needed state
    //rtc.Enable32kHzPin(false);
    //rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone); 
}

#endif

void checkSockStatus(EthernetServer & server)
{
    for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
        EthernetClient client(sock);
        Serial << F("socket status: ") << client.status() << F(" local port: ") << client.localPort() << F(" server port: ") << EthernetServer::server_port[sock] << endl;
        if (EthernetServer::server_port[sock] == 80) {
            if (client.status() != 0x14) {
                Serial << F("Server is not listening. Restarting") << endl;
                client.stop();
                server.begin();
            }
        } else if (EthernetServer::server_port[sock] > 0) {
            Serial << F("Server socket is not closed. Closing") << endl;
            client.stop();
        }
    }
}

bool updateTime()
{
    bool updated = false;
#ifdef NTP_TIME
    EthernetUDP udpConnection;
    NTPClient timeClient(udpConnection, "europe.pool.ntp.org", 0, 1800000UL);
    unsigned long begin = millis();
    timeClient.begin();
    bool timeSet = timeClient.update(5000);
    unsigned long end = millis();
    if (timeSet) {
        Serial << F("Updating time. Took: ") << end - begin << endl;
        syncTime(timeClient);
        updated = true;
    } else {
        Serial << F("Time not updated. Took: ") << end - begin << endl;
    }
    // random freeze if udp connection is kept
    timeClient.end();
#else

    ThreeWire myWire(10, 9, 11); // DATA, SCLK, RESET
    RtcDS1302<ThreeWire> rtc(myWire);
    //RtcDS3231<TwoWire> rtc(myWire);
    rtcSetup(rtc);
    if (rtc.IsDateTimeValid()) {
        RtcDateTime now = rtc.GetDateTime();
        setTime(now.Epoch32Time());
        updated = true;
    } else {
        Serial << F("Time not updated") << endl;
    }
#endif

    wdt_reset();
    return updated;
}