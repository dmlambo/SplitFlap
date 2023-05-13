#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Wire.h>

#include "Communication.h"
#include "Display.h"

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

      doc["modules"][0]["address"] = "master"; // Master
      doc["modules"][0]["status"] = StatusStr[deviceLastStatus];

      for (int i = 0; i < nKnownModules; i++) {
        doc["modules"][i+1]["address"] = knownModules[i];
        char nRead = Wire.requestFrom(knownModules[i], 1, true);
        if (nRead) {        
          doc["modules"][i+1]["status"] = StatusStr[Wire.read()];       
        } else {
          doc["modules"][i+1]["status"] = StatusStr[MODULE_UNAVAILABLE];
        }
      }

      serializeJsonPretty(doc, buff, 1024);
      request->send(200, "application/json", buff);
  });

  server->on("^\\/display(\\/date)?(\\/ephemeral\\/([0-9]+))?(\\/)?$", HTTP_POST, [] (AsyncWebServerRequest *request) {
    if (request->contentType() != "text/plain") {
      request->send(400, "text/plain", "Content type should be text/plain");
    }
  }, 
  NULL, 
  [] (AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static char buff[DISPLAY_MAX_CHARS] = {0};
    static unsigned int buffIdx;
    if (index == 0) buffIdx = 0;
    unsigned int buffLen = DISPLAY_MAX_CHARS - buffIdx;
    bool truncated = false;

    if (buffLen > len) {
      buffLen = len;
    } else {
      truncated = true;
    }

    memcpy(&buff[index], data, buffLen);

    if (index + len == total || truncated) {
      bool date = request->pathArg(0).length();
      int displaySec = request->pathArg(2).toInt();
      Serial.println(String(buff) + " len " + (index+buffLen));
      displayMessage((const char*)buff, index+buffLen, displaySec, date);
      if (truncated) {
        request->send(200, "text/plain", "Truncated to " DEFTOLIT(DISPLAY_MAX_CHARS) " characters");
      } else {
        request->send(200);
      }
    }
  });

  server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  server->onNotFound(notFound);

  server->begin();
}
