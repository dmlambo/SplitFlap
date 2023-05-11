#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Wire.h>

#include "Communication.h"

#include "WebServer.h"

AsyncWebServer* server = NULL;

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void WebServerInit() {
  server = new AsyncWebServer(80);

  server->on("/status", HTTP_GET, [] (AsyncWebServerRequest *request) {
      char buff[1024];
      DynamicJsonDocument doc(1024);
      doc["health"] = "OK";

      doc["modules"][0]["address"] = 0; // Master
      doc["modules"][0]["status"] = StatusStr[deviceLastStatus];

      for (int i = 0; i < nKnownModules; i++) {
        doc["modules"][i+1]["address"] = knownModules[i];
        char nRead = Wire.requestFrom(knownModules[i], 1, true);
        if (nRead) {        
          doc["modules"][i+1]["status"] = StatusStr[Wire.read()];       
        } else {
          doc["modules"][i+1]["status"] = MODULE_UNAVAILABLE;       
        }
        //yield();
      }

      serializeJsonPretty(doc, buff, 1024);
      request->send(200, "application/json", buff);
  });

  // Send a GET request to <IP>/sensor/<number>
  server->on("^\\/sensor\\/([0-9]+)$", HTTP_GET, [] (AsyncWebServerRequest *request) {
      String sensorNumber = request->pathArg(0);
      request->send(200, "text/plain", "Hello, sensor: " + sensorNumber);
  });

// Send a GET request to <IP>/sensor/<number>/action/<action>
  server->on("^\\/sensor\\/([0-9]+)\\/action\\/([a-zA-Z0-9]+)$", HTTP_GET, [] (AsyncWebServerRequest *request) {
      String sensorNumber = request->pathArg(0);
      String action = request->pathArg(1);
      request->send(200, "text/plain", "Hello, sensor: " + sensorNumber + ", with action: " + action);
  });

  server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  server->onNotFound(notFound);

  server->begin();
}
