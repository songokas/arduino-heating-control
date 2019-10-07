#include <Arduino.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <ArduinoJson.h>
#include <Ethernet.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <Time.h>
// required by arduino-mk to include it when compiling storage
#include <EEPROM.h>
#include <Timezone.h>
#include <avr/wdt.h>
#include "Common.h"
#include "Config.h"
// arduino-mk
#include <xxtea-iot-crypt.h>
#include "Encryptor.h"
#include "PacketHandler.h"
#include "Radio.h"
#include "Storage.h"
#include "TimeService.h"
#include "TemperatureSensor.h"
// required by arduino-mk
#include "HeaterInfo.h"
#include "ZoneInfo.h"

using Heating::Packet;
using Heating::printTime;
using Heating::printPacket;
using Heating::isPinAvailable;
using Heating::Config;
using Heating::Radio;
using Heating::Storage;
using TimeService::getTime;
using Heating::TemperatureSensor;
using Heating::Domain::HeaterInfo;
using Heating::Domain::ZoneInfo;
using Network::Encryptor;
using Network::PacketHandler;

#include "AcctuatorProcessor.h"

using Heating::AcctuatorProcessor;

// defines pin id for master
//#define ID 34
//#define CURRENT_TIME 1520772798
#define TIME_SYNC_INTERVAL 3600

extern int freeRam ();

const char websiteContent[] PROGMEM {"<!DOCTYPE html>\n<html>\n    <head>\n        <title>Sildymas</title>\n        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\">\n        <meta charset=\"utf-8\"  />\n        <link rel=\"icon\" href=\"data:;base64,iVBORw0KGgo=\">\n        <script src=\"https://cdnjs.cloudflare.com/ajax/libs/rivets/0.9.6/rivets.bundled.min.js\" type=\"text/javascript\"></script>\n        <link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0-alpha.6/css/bootstrap.min.css\" integrity=\"sha384-rwoIResjU2yc3z8GV/NPeZWAv56rSmLldC3R/AZzGRnGxQQKnKkoFVhFQhNUwEyJ\" crossorigin=\"anonymous\">\n        <style>\n            html {\n                font-size: 14px;\n            }\n            body {\n                font-family: -apple-system,system-ui,BlinkMacSystemFont,\"Segoe UI\",Roboto,\"Helvetica Neue\",Arial,sans-serif;\n            }\n            input[type=\"number\"] {\n                width: 5em;\n            }\n        </style>\n\n    </head>\n    <body>\n        <div class=\"container\">\n            <div class=\"row justify-content-lg-center\">\n                <div class=\"col\">\n                    <info>\n                        <h2>Latest data</h2>\n                        <div class=\"row\" style=\"padding: 10px 0\">\n                            <div class=\"col\">Heating:\n                                <span rv-show=\"heater.on\" style=\"background-color:green\">&nbsp;&nbsp;&nbsp;</span>\n                                <span rv-hide=\"heater.on\" style=\"background-color:red\">&nbsp;&nbsp;&nbsp;</span>\n                                <br/>\n                                <small>Boot:{heater.iT|unixToTime}</small>\n                            </div>\n                            <div class=\"col\">\n                                <div rv-each-time=\"heater.t\">\n                                    On: {time.dtOn|unixToTime} Off: {time.dtOff|unixToTime}\n                                </div>\n                            </div>\n                        </div>\n                        <table class=\"table table-hover\">\n                            <colgroup>\n                                <col width=\"20%\" />\n                                <col />\n                                <col />\n                            </colgroup>\n                            <thead class=\"thead-light\">\n                                <tr>\n                                    <th>Zone</th>\n                                    <th>Data</th>\n                                    <th>History</th>\n                                </tr>\n                            </thead>\n                            <tbody>\n                                <tr rv-each-zone=\"zones\">\n                                    <td>\n                                        <div>\n                                            <strong>{zone.n}</strong>\n                                        </div>\n                                        <div>\n                                            <small>\n                                                pin:{zone.pin}\n                                                state: {zone.st}\n                                            </small>\n                                        </div>\n                                    </td>\n                                    <td>\n                                        <div>Current temperature: {zone.cT|double}</div>\n                                        <div>Expected temperature: {zone.eT|double}</div>\n                                        <div>Last Received: {zone.dtR|unixToTime}</div>\n                                        <div class=\"text-warning\" rv-each-error=\"zone.er\">\n                                            <small>{error|errorToString}</small>\n                                        </div>\n                                    </td>\n                                    <td>\n                                        <div rv-each-time=\"zone.sT\">\n                                            On: {time.dtOn|unixToTime} Off: {time.dtOff|unixToTime}\n                                        </div>\n                                    </td>\n\n                                </tr>\n                                <tr rv-hide=\"zones\">\n                                    <td colspan=\"3\">No data received</td>\n                                </tr>\n                            </tbody>\n                        </table>\n                    </info>\n                    <settings>\n                        <h2>Settings</h2>\n                        <table class=\"table\">\n                            <colgroup>\n                                <col width=\"30%\" />\n                                <col />\n                            </colgroup>\n                            <tbody>\n                                <tr>\n                                    <td>Constant temperature:</td>\n                                    <td class=\"text-left\"><input class=\"form-control-inline form-control-sm\" type=\"number\" step=\"0.1\" rv-value=\"settings.config.constantTemperature | double\" /> C</td>\n                                </tr>\n                                <tr>\n                                    <td>Constant temperature enabled:</td>\n                                    <td class=\"text-left\"><input type=\"checkbox\" rv-checked=\"settings.config.constantTemperatureEnabled\" /></td>\n                                </tr>\n                                <tr>\n                                    <td>Acctuator warmup time:</td>\n                                    <td class=\"text-left\"><input class=\"form-control-inline form-control-sm\" type=\"number\" step=\"0.1\" rv-value=\"settings.config.acctuatorWarmupTime | number\" /> seconds</td>\n                                </tr>\n                                <tr>\n                                    <td>Heater pump stop time:</td>\n                                    <td class=\"text-left\"><input class=\"form-control-inline form-control-sm\" type=\"number\" step=\"0.1\" rv-value=\"settings.config.heaterPumpStopTime | number\" /> seconds</td>\n                                </tr>\n                                <tr>\n                                    <td>Min pwm signal for acctuator:</td>\n                                    <td class=\"text-left\"><input class=\"form-control-inline form-control-sm\" type=\"number\" rv-value=\"settings.config.minPwmState | number\" /> %</td>\n                                </tr>\n                                <tr>\n                                    <td>Min temperature diff between expected and current temperature that pwm will apply:</td>\n                                    <td class=\"text-left\"><input class=\"form-control-inline form-control-sm\" type=\"number\" step=\"0.1\" min=\"0.1\" max=\"1\" rv-value=\"settings.config.minTemperatureDiffForPwm | double\" /></td>\n                                </tr>\n                                <tr>\n                                    <td>Temperature drop for heating to restart:</td>\n                                    <td class=\"text-left\"><input class=\"form-control-inline form-control-sm\" type=\"number\" step=\"0.1\" min=\"0.1\" max=\"2\" rv-value=\"settings.config.temperatureDropWait | double\" /></td>\n                                </tr>\n                            </tbody>\n                        </table>\n                        <table class=\"table table-hover\">\n                            <colgroup>\n                                <col width=\"30%\" />\n                                <col />\n                            </colgroup>\n                            <thead>\n                                <tr>\n                                    <th>Zone</th>\n                                    <th>Temperatures</th>\n                                </tr>\n                            </thead>\n                            <tbody>\n                                <tr rv-each-zone=\"settings.config.zones\">\n                                    <td class=\"text-left\">\n                                        <div class=\"form-group\">\n                                            <button class=\"btn btn-primary btn-sm\" rv-on-click=\"settings.removeZone\">-</button>\n                                            <input type=\"text\" class=\"form-control-inline form-control-sm\" rv-value=\"zone.n\" maxlength=\"30\" placeholder=\"e.g. bathroom\" />\n                                            <input type=\"number\" class=\"form-control-inline form-control-sm\" rv-value=\"zone.id | number\" max=\"199\" size=\"3\" placeholder=\"e.g. 2\" />\n                                        </div>\n                                    </td>\n                                    <td class=\"text-left\">\n                                        <div rv-each-data=\"zone.t\">\n                                            <div class=\"form-group\">\n                                                <label>From:<input type=\"number\" class=\"form-control form-control-sm\" rv-value=\"data.f | number\" min=\"0\" max=\"23\" size=\"4\" placeholder=\"e.g. 21\" /></label>\n                                                <label>To: <input type=\"number\" class=\"form-control form-control-sm\" rv-value=\"data.to | number\" min=\"0\" max=\"24\" size=\"4\" placeholder=\"e.g. 23\" /></label>\n                                                <label>Expected temperature: <input type=\"number\" class=\"form-control form-control-sm\" rv-value=\"data.eT | double\" step=\"0.1\" min=\"15\" max=\"30\" size=\"4\" placeholder=\"e.g. 21\" /></label>\n                                                <button class=\"btn btn-primary btn-sm\" rv-on-click=\"settings.removeTime\">-</button>\n                                            </div>\n                                        </div>\n                                        <button class=\"btn btn-primary btn-sm\" rv-on-click=\"settings.addTime\">+</button>\n                                    </td>\n                                </tr>\n                                <tr><td colspan=\"2\"><button class=\"btn btn-primary btn-sm\" rv-on-click=\"settings.addZone\">+</button></td></tr>\n                            </tbody>\n                        </table>\n                        <button class=\"btn btn-primary btn-sm\" rv-on-click=\"settings.save\" rv-disabled=\"settings.updating\">Update</button>\n                        <button class=\"btn btn-primary btn-sm\" rv-on-click=\"settings.clear\" rv-disabled=\"settings.updating\">Reset</button>\n                    </settings>\n                </div>\n            </div>\n        </div>\n        <script type=\"text/javascript\">\n            function ajax(url, data, obj) {\n                var request = new XMLHttpRequest();\n                request.onreadystatechange = function() {\n                    if (this.readyState == 4) {\n                        if (this.status == 200) {\n                            obj.reload();\n                        } else {\n                            alert('Failed to update !! Status : ' + this.status + ' Text: ' + this.responseText);\n                        }\n                        obj.updating = false;\n                    }\n                };\n                request.onerror = function() {\n                    alert('Unknow error occured');\n                }\n                request.timeout = 5000;\n                request.open(\"POST\", url);\n                request.setRequestHeader(\"Content-Type\", \"application/json\");\n                request.send(data);\n            }\n\n            class Settings {\n                constructor(json) {\n                    this.config = json;\n                    this.updating = false;\n                }\n                addZone() {\n                    this.config.zones.push({t:[{}]});\n                }\n                addTime(ev, context, model) {\n                    this.config.zones[model['%zone%']].t.push({});\n                }\n                removeZone(ev, context, model) {\n                    this.config.zones.splice(model.index, 1);\n                }\n                removeTime(ev, context, model) {\n                    this.config.zones[model['%zone%']].t.splice(model.index, 1);\n                }\n                clear() {\n                    this.updating = true;\n                    ajax('/clear/', {}, this);\n                }\n                save() {\n                    this.updating = true;\n                    const data = JSON.stringify(this.config);\n                    ajax('/', data, this);\n\n                }\n                reload() {\n                    window.location.reload();\n                }\n            };\n            const info = `;\n            const settings = ~;\n\n            rivets.configure({\n                handler: function(context, ev, binding) {\n                    var position = binding.keypath.indexOf('.');\n                    if (position > 0) {\n                        var property = binding.keypath.substring(0, position);\n                        return this.call(binding.view.models[property], ev, context, binding.view.models);\n                    }\n                    return this.call(binding.view.models, ev, context, binding.view.models);\n                }\n            });\n            rivets.formatters.unixToTime = function(value) {\n                if (!(value > 0)) {\n                    return 'n/a';\n                }\n                var d = new Date();\n                var date = new Date((value + (d.getTimezoneOffset() * 60)) * 1000);\n                return date.toLocaleDateString('lt-LT') + ' ' + date.toLocaleTimeString('lt-LT')\n            };\n            rivets.formatters.errorToString = function(value) {\n                if (value == 1) {\n                    return \"Unable to send data to slave\";\n                }\n                return value + \" - n/a\";\n            };\n            rivets.formatters.number = {\n                read: function(value) { return Math.round(value); },\n                publish: function(value) { return Math.round(value); }\n            };\n            rivets.formatters.double = {\n                read: function(value) { return value ? Math.round(value * 100) / 100 : 0; },\n                publish: function(value) { return value ? Math.round(value * 100) / 100 : 0; },\n            };\n\n            rivets.bind(document.getElementsByTagName('info')[0], info);\n            rivets.bind(document.getElementsByTagName('settings')[0], {settings:new Settings(settings)});\n        </script>\n    </body>\n</html>"};


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

void handleRadio(Radio & radio, AcctuatorProcessor & processor)
{
    if (radio.isAvailable(Config::ADDRESS_MASTER)) {
        Packet received;
        if (!radio.receive(Config::ADDRESS_MASTER, &received, sizeof(received))) {
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

int main()
{
    init();

    wdt_disable();

    // heater off
    pinMode(Config::PIN_HEATING, OUTPUT);
    digitalWrite(Config::PIN_HEATING, HIGH);

    Serial.begin(Config::SERIAL_RATE); 

    RF24 rf(Config::PIN_RADIO_CE, Config::PIN_RADIO_CSN);
    Encryptor encryptor(KEY);
    PacketHandler packageHandler;
    Radio radio(rf, packageHandler, encryptor);

    if (Ethernet.begin(Config::MASTER_MAC) == 0) {
        Serial.println(F("Failed to obtain address"));
        Ethernet.begin(Config::MASTER_MAC, Config::MASTER_IP);
    }
    delay(1000);

    Serial.print(F("server is at "));
    Serial.println(Ethernet.localIP());

    EthernetServer server(80);
    server.begin();

    wdt_enable(WDTO_8S);

    setSyncProvider(getTime);
    setSyncInterval(TIME_SYNC_INTERVAL);

    if (timeStatus() == timeNotSet) {
        Serial.print(F("Faile to set time using:"));
        Serial.println(CURRENT_TIME);
        setTime(CURRENT_TIME);
    }

    time_t initTime = now();
    Serial.print(F("current time: "));
    printTime(initTime);
    Serial.println();

    Storage storage;
    Config config {};
    HeaterInfo heaterInfo {initTime};
    storage.loadConfiguration(config);
    AcctuatorProcessor processor(config);

    OneWire oneWire(Config::PIN_TEMPERATURE); 
    DallasTemperature sensors(&oneWire);
    TemperatureSensor sensor(sensors);

    // 1 minute
    unsigned long TIME_PERIOD = 60000UL;
    unsigned long startTime = millis();

    while(true) {


        handleRadio(radio, processor);
        EthernetClient client = server.available();
        handleRequest(client, storage, processor, config, heaterInfo);

        unsigned long currentTime = millis();

        if (currentTime - startTime >= TIME_PERIOD) {

            wdt_reset();

            //Serial.println(F("Read sensor "));
            //Serial.println(millis());

            Packet packet { ID, sensor.read(), 0};

            wdt_reset();

            //Serial.print(F("Handle packet "));
            //Serial.println(millis());


            processor.handlePacket(packet);

            //Serial.print(F("Handle states "));
            //Serial.println(millis());

            wdt_reset();

            processor.handleStates();

            //Serial.print(F("Handle heater "));
            //Serial.println(millis());

            handleHeater(processor, heaterInfo);

            wdt_reset();

            //Serial.print(F("Apply states heater "));
            //Serial.println(millis());

            processor.applyStates(radio, heaterInfo);

            //Serial.print(F("All sessions "));
            //Serial.println(millis());

            wdt_reset();
            packageHandler.printAllSessions();

            startTime = currentTime;

        }

        wdt_reset();
        maintainDhcp();
    }
    return 0;
} 
