#include <Arduino.h>

#include <ESP8266WiFi.h>

#include "Communication.h"

#include "Config.h"

const char* wifiStatusStr(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD: return "No shield";
    case WL_IDLE_STATUS: return "Idle";
    case WL_NO_SSID_AVAIL: return "No SSID available";
    case WL_SCAN_COMPLETED: return "Scan completed";
    case WL_CONNECTED: return "Connected";
    case WL_CONNECT_FAILED: return "Connection failed";
    case WL_CONNECTION_LOST: return "Connection lost";
    case WL_WRONG_PASSWORD: return "Wrong password";    
    case WL_DISCONNECTED: return "Disconnected";
  }
  return "Unknown status";
}

void printConfig() {
  LOG("isMaster: "); LOGLN(Config.isMaster);
  LOG("address: "); LOGLN((int)Config.address);
  LOG("zeroOffset: "); LOGLN((int)Config.zeroOffset);
  LOG("rpm: "); LOGLN((int)Config.rpm);

  if (Config.isMaster) {
    LOG("timeZone: "); LOGLN(*Config.timeZone ? Config.timeZone : "<None set>");
    LOGLN();
    LOG("WiFi status: "); LOGLN(wifiStatusStr(WiFi.status()));
    LOG("IP address: "); LOGLN(WiFi.localIP());
    LOGLN();
    LOG("Connected to "); LOG(nKnownModules); LOG(" other modules: ");
    for (unsigned int i = 0; i < nKnownModules; i++) {
      LOG((int)knownModules[i]); LOG(" ");
    }
    LOGLN();
  }
}