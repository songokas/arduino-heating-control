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
    }
    return true;
}

void handleAcctuator(PubSubClient & mqttClient, IEncryptedMesh & radio, ZoneInfo & zoneInfo)
{
    zoneInfo.print();

    if (zoneInfo.pin.id > 200) {
        char topic[MQTT_MAX_LEN_TOPIC] {0};
        snprintf_P(topic, COUNT_OF(topic), CHANNEL_SLAVE_CONTROL, zoneInfo.pin.id - 200);
        char message[6] {0};
        snprintf_P(message, COUNT_OF(message), PSTR("%u"), zoneInfo.getPwmValue());
        if (!mqttClient.publish(topic, message)) {
            zoneInfo.addError(Error::CONTROL_PACKET_FAILED);
            Serial << F("Failed to send ") << topic << F(" Message: ") << message << endl;
        } else {
            if (zoneInfo.hasConfirmation() || zoneInfo.getState() == 0) {
                zoneInfo.removeError(Error::CONTROL_PACKET_FAILED);
                zoneInfo.recordState(zoneInfo.getState());
            }
        }
        
    } else if (zoneInfo.pin.id > 100) {
        ControllPacket controllPacket {(uint8_t)(zoneInfo.pin.id - 100), zoneInfo.getPwmState()};
        if (!radio.send(&controllPacket, sizeof(controllPacket), 0, Config::ADDRESS_SLAVE, 2)) {
            zoneInfo.addError(Error::CONTROL_PACKET_FAILED);
            Serial << F("Failed to send ") << controllPacket.id << F(" State: ") << zoneInfo.getPwmState() << endl;
            return;
        } else {
            zoneInfo.removeError(Error::CONTROL_PACKET_FAILED);
            zoneInfo.recordState(zoneInfo.getState());
        }

    } else if (zoneInfo.pin.id > 0) {
        pinMode(zoneInfo.pin.id, OUTPUT);
        analogWrite(zoneInfo.pin.id, zoneInfo.getPwmState());
        zoneInfo.recordState(zoneInfo.getState());
    }

    auto zone = processor.getZoneConfigById(zoneInfo.pin.id);
    if (zone == nullptr) {
        return;
    }

    char topic[MQTT_MAX_LEN_TOPIC] {0};
    snprintf_P(topic, COUNT_OF(topic), INFO_NODE_TOPIC, zone->getName());
    char message[MQTT_MAX_LEN_MESSAGE] {0};
    snprintf_P(message, COUNT_OF(message), INFO_NODE_MESSAGE, zoneInfo.getCurrentTemperature(), zoneInfo.getState());
    if (!mqttClient.publish(topic, message)) {
        Serial << F("Failed to send info to ") << zone->getName() << endl;
    }
}

bool handleJson(AcctuatorProcessor & manager, const char * requestString, const char * pos)
{
    for (uint8_t i = 0; i < config.getZoneArrLength(); i++) {
        resetWatchDog();
        auto & zone = config.getZone(i);
        if (!(zone.id > 0)) {
            continue;
        }
        char expectedTopic[MQTT_MAX_LEN_TOPIC] {0};
        snprintf_P(expectedTopic, COUNT_OF(expectedTopic), HEATING_TOPIC, zone.getName());

        if (strstr(requestString, expectedTopic) > 0) {
            StaticJsonDocument<MAX_LEN_JSON_MESSAGE> doc;
            if (retrieveJson(doc, pos + 4)) {
                if (doc["temperature"].is<int16_t>()) {
                    Packet packet {zone.id, doc["temperature"]};
                    return handlePacket(manager, packet);
                } else if (doc["expectedTemperature"].is<int16_t>()) {
                    Packet packet {zone.id, 0, doc["expectedTemperature"]};
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

void handleHeater(AcctuatorProcessor & processor, HeaterInfo & heaterInfo, PubSubClient & mqttClient)
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

    char topic[MQTT_MAX_LEN_TOPIC] {0};
    snprintf_P(topic, COUNT_OF(topic), INFO_MASTER_TOPIC);
    char message[MQTT_MAX_LEN_MESSAGE] {0};
    snprintf_P(message, COUNT_OF(message), INFO_MASTER_MESSAGE, heaterInfo.isOn() ? 100 : 0);
    if (!mqttClient.publish(topic, message)) {
        Serial << F("Failed to send info about heater") << endl;
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

bool connectToMqtt(PubSubClient & client, const char * clientName, const char * subscribeTopic, const char * secondTopic)
{
    if (!client.connected()) {
        Serial << F("Attempting MQTT connection...");
        uint16_t mqttAttempts = 0;
        while (!client.connect(clientName) && mqttAttempts < 6) {
            mqttAttempts++;
            Serial.print(".");
            delay(500 * mqttAttempts);
            resetWatchDog();
        }
        if (mqttAttempts >= 6) {
            Serial << F("Mqtt connection failed, rc=") << client.state() << F(" try again in 5 seconds") << endl;
            return false;
        } else {
            Serial << F("connected.") << endl;
            if (client.subscribe(subscribeTopic) && client.subscribe(secondTopic)) {
                Serial << F("Subscribed to: ") << subscribeTopic << F(" ") << secondTopic << endl;
            } else {
                Serial << F("Failed to subscribe to: ") << subscribeTopic << F(" ") << secondTopic << endl;
            }
        }
        resetWatchDog();
    }
    return true;
}

bool sendKeepAlive(PubSubClient & client,  const char * keepAliveTopic, unsigned long time)
{
    char topic[MQTT_MAX_LEN_TOPIC] {0};
    snprintf_P(topic, COUNT_OF(topic), keepAliveTopic);
    char liveMsg[16] {0};
    sprintf(liveMsg, "%lu", time);
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
    char message[MQTT_MAX_LEN_MESSAGE] {0};
    memcpy(message, payload, MIN(len, COUNT_OF(message) - 1));

    for (uint8_t i = 0; i < config.getZoneArrLength(); i++) {
        auto & zone = config.getZone(i);
        if (!(zone.id > 0)) {
            continue;
        }
        char expectedTopic[MQTT_MAX_LEN_TOPIC] {0};
        snprintf_P(expectedTopic, COUNT_OF(expectedTopic), HEATING_TOPIC, zone.getName());
        if (strncmp(expectedTopic, topic, strlen(expectedTopic)) == 0) {
            int16_t value = atoi(message);
            Packet packet { zone.id, value };
            handlePacket(processor, packet);
            return;
        }
        char zigbeeTopic[MQTT_MAX_LEN_TOPIC] {0};
        snprintf_P(zigbeeTopic, COUNT_OF(zigbeeTopic), HEATING_ZIGBEE_TOPIC, zone.getName());
        if (strncmp(zigbeeTopic, topic, strlen(zigbeeTopic)) == 0) {
            StaticJsonDocument<MAX_LEN_JSON_MESSAGE> doc;
            if (retrieveJson(doc, message)) {
                if (doc["temperature"].is<float>()) {
                    Packet packet {zone.id, (int16_t)(doc["temperature"].as<float>() * 100)};
                    handlePacket(processor, packet);
                } else {
                    Serial << F("Invalid json structure") << endl;
                }
            }
            return;
        }



    }

    char confirmTopic[MQTT_MAX_LEN_TOPIC] {0};
    snprintf_P(confirmTopic, COUNT_OF(confirmTopic), CHANNEL_SLAVE_CONFIRM);
    if (strncmp(confirmTopic, topic, strlen(confirmTopic)) == 0) {
        StaticJsonDocument<MAX_LEN_JSON_MESSAGE> doc;
        if (retrieveJson(doc, message)) {
            auto json = doc["PWM"].as<JsonObject>();
            for (const auto & pwm: json) {

                if (!pwm.value().is<int16_t>()) {
                    continue;
                }

                int16_t val = pwm.value().as<int16_t>();
                if (!(val > 0)) {
                    continue;
                }
                auto title = pwm.key().c_str();
                int pwmIndex {0};
                sscanf(title, "PWM%d", &pwmIndex);
                if (!(pwmIndex > 0)) {
                    continue;
                }
                // mqtt zones above 200
                uint8_t id = pwmIndex + 200;
                auto zoneConfig = processor.getZoneConfigById(id);
                if (zoneConfig == nullptr) {
                    continue;

                }
                ZoneInfo & zoneInfo = processor.getAvailableZoneInfoById(id);
                Serial << "Confirmation received" << id << endl;
                zoneInfo.dtConfirmationReceived = now();
            }
        }
        return;
    }

    Serial << "Not handled: " << topic << endl; 
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
        //Serial << F("socket status: ") << client.status() << F(" local port: ") << client.localPort() << F(" server port: ") << EthernetServer::server_port[sock] << endl;
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

struct TimeOperation
{
    bool updated;
    unsigned long elapsed;
};

TimeOperation updateTime()
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
    return TimeOperation{updated, end - begin};
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
    return TimeOperation{updated, 0};
#endif
}