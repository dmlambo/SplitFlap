#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Wire.h>

#include "Communication.h"
#include "Display.h"

#include "WebServer.h"

AsyncWebServer* server = NULL;

class FlashStream : public Stream {
  unsigned int _addrBegin;
  unsigned int _size;
  unsigned int _curPos;

public:
  FlashStream(unsigned int addrBegin, unsigned int size) :
    _addrBegin(addrBegin), _size(size), _curPos(0) 
  {}

  using Print::write;

  int available() { return _size - _curPos; }

  int read() {  
    if (_curPos == _size) return -1;
    unsigned char data;
    if (!ESP.flashRead(_addrBegin + _curPos, &data, sizeof(data))) return -1;
    _curPos++;
    return (int)data;
  }

  int peek() { 
    if (_curPos == _size) return -1;
    unsigned char data;
    if (!ESP.flashRead(_addrBegin + _curPos, &data, sizeof(data))) return -1;
    return (int)data;
  }  

  size_t write(uint8_t data) {
    return 0;
  }

  virtual size_t readBytes(char *buffer, size_t length) {
    unsigned int size = length;
    if (_size - _curPos < length) size = _size - _curPos;
    if (size) {
      ESP.flashRead(_addrBegin + _curPos, (unsigned char*)buffer, size);
      _curPos += size; 
    }
    return size;
  }

  virtual ssize_t streamRemaining () { return _size - _curPos; }
};

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void WebServerInit() {
  server = new AsyncWebServer(80);

  server->on("/status", HTTP_GET, [] (AsyncWebServerRequest *request) {
      // So JsonArduino doesn't allow just-in-time serialization... this is a limiting factor in 
      // how many modules, or what status we can represent
      StaticJsonDocument<1024> doc;

      doc["health"] = "OK";

      doc["modules"][0]["address"] = "master";
      doc["modules"][0]["status"] = StatusStr[deviceLastStatus];
      doc["modules"][0]["version"] = VERSION;

      for (int i = 0; i < nKnownModules; i++) {
        doc["modules"][i+1]["address"] = knownModules[i];
        doc["modules"][i+1]["status"] = "Unknown";

        ModuleStatus status;

        switch (i2cReadStruct(knownModules[i], &status)) {    
          case PACKET_OK:
            doc["modules"][i+1]["status"] = StatusStr[status.status];
            doc["modules"][i+1]["zeroOffset"] = status.zeroOffset;
            doc["modules"][i+1]["flapNumber"] = status.flap;
            doc["modules"][i+1]["version"] = status.version;
            break;
          case PACKET_CRC:
            LOGLN("Module status CRC does not match");
            break;
          case PACKET_OVERFLOW:
          case PACKET_UNDERFLOW:
            LOGLN("Packet not the right length or I2C problem");
            break;
          default:
            LOGLN("Unknown error reading module status");
        }
      }

      AsyncResponseStream* resp = request->beginResponseStream("application/json");
      serializeJsonPretty(doc, *resp);
      resp->setCode(200);
      request->send(resp);
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
      LOGLN(String(buff) + " len " + (index+buffLen));
      displayMessage((const char*)buff, index+buffLen, displaySec, date);
      if (truncated) {
        request->send(200, "text/plain", "Truncated to " DEFTOLIT(DISPLAY_MAX_CHARS) " characters");
      } else {
        request->send(200);
      }
    }
  });

  server->on("/update.bin", HTTP_GET, [] (AsyncWebServerRequest *request) {
    FlashStream* sketchFlashStream = new FlashStream(0, ESP.getSketchSize());
    modulesContacted++;
    request->onDisconnect([sketchFlashStream] (void) { delete sketchFlashStream; modulesFinishedUpdate++; });
    AsyncWebServerResponse* resp = request->beginResponse(*sketchFlashStream, "application/binary", ESP.getSketchSize());
    resp->addHeader("Connection", "keep-alive");
    resp->setCode(200);
    request->send(resp);
  });

  server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  server->onNotFound(notFound);

  server->begin();
}
