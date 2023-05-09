#include "WebServer.h"

AsyncWebServer* server = NULL;

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void WebServerInit() {
  server = new AsyncWebServer(80);

  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", "Hello, world");
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

  server->onNotFound(notFound);

  server->begin();
}
