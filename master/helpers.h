void printWebsite(EthernetClient & client, const AcctuatorProcessor & manager, const HeaterInfo & heaterInfo)
{
    unsigned int i = 0;
    String website;
    for (unsigned int k = 0; k < strlen_P(websiteContent); k++) {
        char c =  pgm_read_byte_near(websiteContent + k);
        if (c == '~') {
            client.print(website);
            manager.printConfig(client);
            website = "";
        } else if (c == '`') {
            client.print(website);
            manager.printInfo(client, heaterInfo);
            website = "";
        } else {
            website += c;
            i++;
            if (i % Config::MAX_REQUEST_PRINT_BUFFER == 0) {
                client.print(website);
                website = "";
            }
        }
    }
    client.print(website);
}

void sendHtml(EthernetClient & client, const AcctuatorProcessor & manager, const HeaterInfo & heaterInfo)
{
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: text/html"));
    client.println(F("Connection: close"));  // the connection will be closed after completion of the response
    client.println();
    printWebsite(client, manager, heaterInfo);
}

void sendNotFound(EthernetClient & client)
{
    client.println(F("HTTP/1.1 404 NOT FOUND"));
    client.println(F("Content-Type: text/html"));
    client.println(F("Connection: close"));  // the connection will be closed after completion of the response
    client.println();
    client.println("NOT FOUND");

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
        client.println("\"OK\"");
    } else {
        client.println("\"ERROR\"");
    }
}

void handleRequest(EthernetClient & client, Storage & storage, AcctuatorProcessor & manager, Config & config, const HeaterInfo & heaterInfo)
{
    if (client) {
        if (client.connected()) {
            unsigned int i = 0;
            String requestString;
            requestString.reserve(1200);
            while (client.available() && i < Config::MAX_REQUEST_SIZE) {
                char c = client.read();
                requestString +=c;
                i++;
            }
            if (requestString.indexOf("/clear/") > 0) {
                requestString = "";
                Serial.println(F("Send clear"));
                bool ok = storage.saveConfiguration(config, requestString);
                sendJson(client, ok);
            } else if (requestString.indexOf("application/json") > 0) {
                Serial.println(F("Send ajax"));
                int pos = requestString.indexOf("\r\n\r\n");
                String body {requestString.substring(pos + 4)};
                requestString = "";
                bool ok = storage.saveConfiguration(config, body);
                sendJson(client, ok);
            } else {
                requestString = "";
                Serial.println(F("Send html"));
                sendHtml(client, manager, heaterInfo);
            }
            delay(10);
            client.stop();
            Serial.println(F("Client disconnected"));

        }
    }
}

void handleRadio(IEncryptedMesh & radio, AcctuatorProcessor & processor)
{
    if (radio.isAvailable()) {
        Packet received;
        RF24NetworkHeader header;
        if (!radio.receive(&received, sizeof(received), 0, header)) {
            Serial.println(F("Failed to receive packet on master"));
            printPacket(received);
            return;
        }

        printPacket(received);
        if (isPinAvailable(received.id, Config::AVAILABLE_PINS_MEGA, sizeof(Config::AVAILABLE_PINS_MEGA)/sizeof(Config::AVAILABLE_PINS_MEGA[0]))) {
            processor.handlePacket(received);
        } else {
            Serial.println(F("Ignoring packet since its pin is not available"));
        }
    }
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

void maintainDhcp() {

  switch (Ethernet.maintain())
  {
    case 1:
      //renewed fail
      Serial.println("Error: renewed fail");
      break;

    case 2:
      //renewed success
      Serial.println("Renewed success");
      Serial.println(Ethernet.localIP());
      break;

    case 3:
      //rebind fail
      Serial.println("Error: rebind fail");
      break;

    case 4:
      //rebind success
      Serial.println("Rebind success");
      Serial.println(Ethernet.localIP());
      break;

    default:
      //nothing happened
      break;

  }
}
