#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>

#include <time.h>

#include "Communication.h"
#include "Motor.h"

#include "Config.h"

// TODO: Automatically "scroll" through an entire message
char currentDisplayText[DISPLAY_MAX_MODULES+1] = {0};
char ephemeralDisplayText[DISPLAY_MAX_CHARS+1] = {0};
char persistentDisplayText[DISPLAY_MAX_CHARS+1] = {0};

static unsigned long lastEphemeralDisplayMillis = 0;
static unsigned long ephemeralDisplayDurationMillis = 0;
static unsigned long lastTime = 0;
static bool ephemeralTime = false;
static bool persistentTime = false;
static bool displayDirty = false;

void displayEvents() {
  if (ephemeralDisplayDurationMillis) {
    if (millis() - lastEphemeralDisplayMillis > ephemeralDisplayDurationMillis) {
      ephemeralDisplayDurationMillis = 0;
      displayDirty = true;
    }
  }

  char (&displayBuff)[] = ephemeralDisplayDurationMillis ? ephemeralDisplayText : persistentDisplayText;
  bool time = (ephemeralDisplayDurationMillis && ephemeralTime) || (!ephemeralDisplayDurationMillis && persistentTime);
  unsigned long curTime = millis() / 1000; // 1s update

  if (time && curTime != lastTime) {
    displayDirty = true;
  }

  if (displayDirty) {
    memset(currentDisplayText, 0, sizeof(currentDisplayText));
    
    if (time) {
    static bool timeConfigured = false;

      if (!timeConfigured) {
        if (!WiFi.isConnected()) {      
          LOGLN("Wifi unavailable, couldn't configure time");
          strcpy(currentDisplayText, "NO WIFI");
        } else {
          tm timeInfo;
          LOGLN("Configuring time");
          configTime(Config.timeZone, "pool.ntp.org");          
          if (getLocalTime(&timeInfo)) {
            timeConfigured = true;
          } else {
            strcpy(currentDisplayText, "NO TIME");
          }
        }
      } 
      if (timeConfigured) {
        tm timeInfo;
        if (getLocalTime(&timeInfo)) {
          strftime(currentDisplayText, DISPLAY_MAX_MODULES, displayBuff, &timeInfo);
        } else {
          strcpy(currentDisplayText, "NO TIME");
        }
      }
    } else {
      strncpy(currentDisplayText, displayBuff, DISPLAY_MAX_MODULES);
    }

    for (int i = 0; i < DISPLAY_MAX_MODULES; i++) {
      // Range the input to flaps
      currentDisplayText[i] = currentDisplayText[i] < 32 ? 32 : (currentDisplayText[i] & 127);
    }

    LOG("Displaying: ");
    LOGLN(currentDisplayText);

    motorMoveToFlap(Config.charMap[(currentDisplayText[0]&127)-32]);

    for (unsigned int i = 0; i < nKnownModules; i++) {
      unsigned char res;
      unsigned char nRetries = 5;
      char buff[6];
      snprintf(buff, 6, "f %d", Config.charMap[(currentDisplayText[i+1]&127)-32]);
      do {
        Wire.beginTransmission(knownModules[i]);
        Wire.write(buff);
        Wire.write(0);
        res = Wire.endTransmission();
      } while (res != 0 && nRetries--);
    }

    displayDirty = false;
  }
  lastTime = curTime;
}

void displayMessage(const char* message, unsigned int len, unsigned int seconds, bool time) {
  char (&displayBuff)[] = seconds ? ephemeralDisplayText : persistentDisplayText;
  unsigned int cpyLen = DISPLAY_MAX_CHARS > len ? len : DISPLAY_MAX_CHARS;
  
  if (seconds) {
    ephemeralTime = time;
  } else {
    persistentTime = time;  
  }
  
  // Clear ephemeral message either way
  lastEphemeralDisplayMillis = millis();
  ephemeralDisplayDurationMillis = seconds * 1000;
  ephemeralDisplayText[0] = 0;

  memcpy(displayBuff, message, cpyLen);
  displayBuff[cpyLen] = 0;

  // Nothing happens until the next loop(), see displayEvents()
  displayDirty = true;
}

void displaySetTimeZone(const char* timezone) {
  if (timezone && *timezone) {
    setenv("TZ", timezone, 1);
  } else {
    unsetenv("TZ");
  }
  tzset();  
}