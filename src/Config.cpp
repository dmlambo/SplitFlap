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
  Serial.print("isMaster: "); Serial.println(Config.isMaster);
  Serial.print("address: "); Serial.println((int)Config.address);
  Serial.print("zeroOffset: "); Serial.println((int)Config.zeroOffset);
  Serial.print("rpm: "); Serial.println((int)Config.rpm);

  if (Config.isMaster) {
    Serial.print("timeZone: "); Serial.println(*Config.timeZone ? Config.timeZone : "<None set>");
    Serial.println();
    Serial.print("WiFi status: "); Serial.println(wifiStatusStr(WiFi.status()));
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
    Serial.println();
    Serial.print("Connected to "); Serial.print(nKnownModules); Serial.print(" other modules: ");
    for (unsigned int i = 0; i < nKnownModules; i++) {
      Serial.print((int)knownModules[i]); Serial.print(" ");
    }
    Serial.println();
  }
}