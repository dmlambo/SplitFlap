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

void printConfig(Print* out) {
  out->printf("Version: %u\n", (unsigned int)VERSION);
  out->printf("isMaster: %s\n", Config.isMaster ? "true" : "false");
  out->printf("address: %u\n", (unsigned int)Config.address);
  out->printf("zeroOffset: %u\n", (unsigned int)Config.zeroOffset);
  out->printf("rpm: %u\n", (unsigned int)Config.rpm);

  if (Config.isMaster) {
    out->printf("timeZone: %s\n\n", *Config.timeZone ? Config.timeZone : "<None set>");

    out->printf("WiFi status: %s\n", wifiStatusStr(WiFi.status()));
    out->printf("IP address: "); WiFi.localIP().printTo(*out); out->printf("\n\n");
    
    out->printf("Connected to %u other modules:", (unsigned int)nKnownModules);
    for (unsigned int i = 0; i < nKnownModules; i++) {
      out->printf(" %u", (unsigned int)knownModules[i]);
    }
    out->printf("\n");
  }
}