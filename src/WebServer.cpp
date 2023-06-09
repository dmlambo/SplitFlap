#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Wire.h>

#include <StreamUtils/Streams/MemoryStream.hpp>

#include "Commands.h"
#include "Communication.h"
#include "Display.h"
#include "Motor.h"
#include "Streams.h"
#include "Utils.h"

#include "WebServer.h"

struct StaticAllocator {
  char* _buff;
  size_t _pos;
  size_t _size;

  StaticAllocator(char* buff, size_t size) {
    _buff = buff;
    _pos = 0;
    _size = size;
  }

  void* allocate(size_t n) {
    if (_pos + n > _size) return 0;
    char* ret = _buff+_pos;
    _pos += n;
    return ret;
  }

  void deallocate(void* p) {
  }
};

// We store these buffers globally since they're so large, and cause crazy heap fragmentation, and eventual crashes.
char cmdBuff[128]; // Async command buffer
char memBuff[512]; // Async logging stream
BlockingStream<StreamUtils::BasicMemoryStream<StaticAllocator>> memStream(sizeof(memBuff), StaticAllocator(memBuff, sizeof(memBuff)));

class NoDelayWebServer : public AsyncWebServer {
public:
  NoDelayWebServer(uint16_t port) : AsyncWebServer(port) {
    _server.setNoDelay(true);
  }
};

AsyncWebServer* server = NULL;

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void WebServerInit() {
  server = new NoDelayWebServer(80);

  server->on("/status", HTTP_GET, [] (AsyncWebServerRequest *request) {
      // So JsonArduino doesn't allow just-in-time serialization... this is a limiting factor in 
      // how many modules, or what status we can represent
      StaticJsonDocument<1024> doc;

      doc["health"] = "OK";

      doc["modules"][0]["address"] = "master";
      doc["modules"][0]["multilineDelay"] = Config.multilineDelay;
      doc["modules"][0]["status"] = StatusStr[deviceLastStatus];
      doc["modules"][0]["zeroOffset"] = (unsigned char)Config.zeroOffset;
      doc["modules"][0]["flapNumber"] = motorCurrentFlap();
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

  server->on("/cmd", HTTP_POST, [] (AsyncWebServerRequest *request) {
    if (request->contentType() != "text/plain") {
      request->send(400, "text/plain", "Content type should be text/plain");
    }
    if (request->contentLength() == 0) {
      request->send(400, "text/plain", "Empty request");
    }
  },
  NULL,
  [] (AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index == 0) {
      // gets deleted in the request destructor
      memStream.flush();
    }

    if ((unsigned int)memStream.available() + len > 255) {
      request->send(400, "text/plain", "Request longer than 255");
      return;
    }

    memStream.write(data, len);

    if (index + len == total) {
      cmdBuff[memStream.readBytes(cmdBuff, 255)] = 0;

      Serial.printf("Command buff: %s\n", cmdBuff);

      bool async = handleCommandAsync(cmdBuff, &memStream);

      // The content type event-stream is important to keep the browser from caching the first 1kb or so
      AsyncWebServerResponse* resp = request->beginChunkedResponse("text/event-stream; charset=us-ascii", [async, request] (uint8_t* data, size_t len, size_t index) -> size_t {
        if (memStream.available()) {
          size_t read = memStream.readBytes((char*)data, len);
          return read;
        } else {
          if (!async) return 0;
          if (!queuedCommand.finished) {
            return RESPONSE_TRY_AGAIN;
          }
          // We dequeue the request in onDisconnect, since we could have a disconnect in the middle of a command, which would leave the queue dangling.
          return 0;
        }
      });
      resp->setCode(async ? 200 : 400);
      request->onDisconnect([async] () {
        if (async) {
          queuedCommand.queued = false;
        }
      });
      request->send(resp);
    }
  });

  server->on("^\\/display(\\/(left|right|center))?(\\/date)?(\\/ephemeral\\/([0-9]+))?(\\/)?$", HTTP_POST, [] (AsyncWebServerRequest *request) {
    if (request->contentType() != "text/plain; ") {
      request->send(400, "text/plain", "Content type should be text/plain");
    }
  }, 
  NULL, 
  [] (AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static char buff[DISPLAY_MAX_CHARS+1] = {0};
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
      DisplayJustify justify = JustifyNone;
      bool date = request->pathArg(2).length();
      int displaySec = request->pathArg(4).toInt();
      if (!request->pathArg(1).isEmpty()) {
        if (request->pathArg(1) == "left") {
          justify = JustifyLeft;
        } else if (request->pathArg(1) == "right") {
          justify = JustifyRight;
        } else {
          justify = JustifyCenter;
        }
      }
      buff[index+buffLen] = '\0';
      LOGLN(String(buff) + " len " + (index+buffLen));
      displayMessage((const char*)buff, index+buffLen, displaySec, date, justify);
      if (truncated) {
        request->send(200, "text/plain", "Truncated to " DEFTOLIT(DISPLAY_MAX_CHARS) " characters");
      } else {
        request->send(200);
      }
    }
  });

  server->on("/update.bin", HTTP_GET, [] (AsyncWebServerRequest *request) {
    FlashStream* sketchFlashStream = new FlashStream(0, ESP.getSketchSize());
    moduleContacted = true;
    request->onDisconnect([sketchFlashStream] (void) { delete sketchFlashStream; moduleFinishedUpdate = true; });
    AsyncWebServerResponse* resp = request->beginResponse(*sketchFlashStream, "application/binary", ESP.getSketchSize());
    resp->addHeader("Connection", "keep-alive");
    resp->setCode(200);
    request->send(resp);
  });

  server->onFileUpload([] (AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if(!index){
      LOGLN("Got upload file request:");

      int command = 0;
      
      if (request->url() == "/update") {
        LOGLN("Firmware");
        command = U_FLASH;
      } else if (request->url() == "/updatefs") {
        LOGLN("Filesystem");
        command = U_FS;
        close_all_fs();
      } else {
        LOGLN("Unknown file upload request");
        notFound(request);
        return;
      }
      LOGLN("Update begin...");
      Update.runAsync(true);
      size_t freeSpace = command == U_FLASH ? (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000 : FS_end - FS_start;
      if (!Update.begin(freeSpace, command)) {
        request->send(400, "text/plain", "Update begin failed: " + Update.getErrorString());
        return;
      }
    }

    if (Update.write(data, len) != len) {
      request->send(400, "text/plain", "Update write underflow: " + Update.getErrorString());
      return;
    }

    if(final){
      LOGLN("Finished. Running update.");
      AsyncResponseStream* resp = request->beginResponseStream("text/plain");
      resp->setCode(200);
      resp->printf("Update accepted... attempting to flash %d bytes (this could take a few minutes)", Update.progress());
      LOG("Flash image size: "); LOGLN(Update.progress());
      if (!Update.end(true)) { // We don't know the size of the file when we start.
        resp->printf("\nUpdate failed: %s", Update.getErrorString().c_str());
        request->send(resp);
      } else {
        resp->write("\nSuccess! Rebooting!");
        request->send(resp);
        markForReboot(500);
      }
    }
  });

  server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  server->onNotFound(notFound);

  server->begin();
}
