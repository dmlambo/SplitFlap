#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>

#include <time.h>

#include "Communication.h"
#include "Display.h"
#include "Motor.h"

#include "Config.h"

static struct DisplayTextParams {
  char displayText[DISPLAY_MAX_CHARS+1] = {0};
  unsigned long lastDisplayMillis = 0;
  unsigned long multilineStartTime = 0;
  size_t multilinePos = 0;
  bool isTime = false;
  DisplayJustify justify = JustifyNone;
} persistentDisplayParams, ephemeralDisplayParams;

static unsigned long lastEphemeralDisplayMillis = 0;
static unsigned long ephemeralDisplayDurationMillis = 0;
static unsigned long lastSeconds = 0;
static bool displayDirty = false;

const char* findStop(const char* buff, unsigned int buffLen) {
	for (unsigned int i = 0; i < buffLen; i++) {
		if (buff[i] == ' ' || buff[i] == '\0' || buff[i] == '|') return &buff[i];
	}
	return NULL;
}

void strShiftRight(char* buff, unsigned int buffLen, unsigned int amt) {
	// Swap i with i-amt
	for (int i = buffLen-1-amt; i >= 0; i--) {
		auto idx = (i - amt + buffLen) % buffLen;
		auto tmp = buff[i];
		buff[i] = buff[idx];
		buff[idx] = tmp;
	}
}

unsigned int rightSpaces(const char* buff, unsigned int buffLen) {
	int right = 0;
	for (int n = buffLen - 1; n >= 0; n--) {
		if (buff[n] != ' ') break;
		right++;
	}
	return right;
}

unsigned int justifyText(char* dstBuff, unsigned int dstBuffLen, const char* srcBuff, unsigned int srcBuffLen, DisplayJustify justify) {
	unsigned char lineSize = 8;
	size_t srcPos = 0;
	size_t dstPos = 0;
	unsigned int outLen = 0;

	switch (justify) {
	case JustifyLeft:
	case JustifyCenter:
	case JustifyRight:
	{
		// Go through each word (space separated), stopping when we're at the end of the message, or when we've run out of display message space
		unsigned int spaceLeft = 0;
		while (srcPos < srcBuffLen && srcBuff[srcPos] != '\0' && dstPos < dstBuffLen) {
			// Trim leading spaces
			while (srcBuff[srcPos] == ' ') srcPos++;

			// Find word size
			const char* nextSpace = findStop(&srcBuff[srcPos], srcBuffLen - srcPos); // strchr(&srcBuff[srcPos], ' ');
			unsigned int wordSize = min(nextSpace ? (unsigned int)(nextSpace - &srcBuff[srcPos]) : srcBuffLen - srcPos, (unsigned int)lineSize);
			spaceLeft = min(lineSize - dstPos % lineSize, dstBuffLen - dstPos);

			// If the word fits into the space left in the display...
			if (wordSize && wordSize <= spaceLeft && srcBuff[srcPos] != '|') {
				// Copy it (plus space, if room on line, and in buffer) into the buffer
				memcpy(&dstBuff[dstPos], &srcBuff[srcPos], wordSize);
				dstPos += wordSize;
				srcPos += wordSize;
				if (spaceLeft - wordSize) {
					dstBuff[dstPos++] = ' ';
					spaceLeft--;
				}
				spaceLeft -= wordSize;
			}
			else {
				// Else, pad the rest of the display, advance dstPos to the next line
				for (unsigned int i = 0; i < spaceLeft; i++) {
					dstBuff[dstPos++] = ' ';
				}
				if (srcBuff[srcPos] == '|') { // Forced line break
					srcPos++;
				}
				spaceLeft = 0;
			}
		}
		if (spaceLeft) {
			for (unsigned int i = 0; i < spaceLeft && dstPos < dstBuffLen; i++) {
				dstBuff[dstPos++] = ' ';
			}
		}
		outLen = dstPos;
		break;
	}
	default:
		strncpy(dstBuff, srcBuff, dstBuffLen);
		outLen = strlen(dstBuff);
		break;
	}

	// Don't handle the last line if it's truncated, display it verbatim
	for (unsigned int i = 0; i + lineSize <= outLen; i += lineSize) {
		switch (justify) {
		case JustifyCenter:
		{
			// Find rightmost character
			int right = rightSpaces(&dstBuff[i], lineSize);
			right /= 2;
			strShiftRight(&dstBuff[i], lineSize, right);
			break;
		}
		case JustifyRight:
		{
			// Find rightmost character
			int right = rightSpaces(&dstBuff[i], lineSize);
			strShiftRight(&dstBuff[i], lineSize, right);
			break;
		}
		case JustifyLeft:
		default:
			// Should already be left justified
			break;
		}
	}

	return outLen;
}

inline char charToFlap(char c) {
  #include "CharFlapMap.h"
  return CharFlapMap[((c < 32 ? 32 : c) & 127)-32];
}

void displayEvents() {
  char currentDisplayText[DISPLAY_MAX_CHARS] = {0};
  unsigned int currentDisplayTextLen = 0;
  unsigned int lineSize = nKnownModules + 1;

  if (ephemeralDisplayDurationMillis) {
    if (millis() - lastEphemeralDisplayMillis > ephemeralDisplayDurationMillis) {
      ephemeralDisplayDurationMillis = 0;
      displayDirty = true;
    }
  }

  auto &params = ephemeralDisplayDurationMillis ? ephemeralDisplayParams : persistentDisplayParams;

  unsigned long curTime = millis();
  unsigned long curSeconds = curTime / 1000; // 1s update

  if (params.isTime && curSeconds != lastSeconds) {
    displayDirty = true;
  }

  if (params.multilineStartTime && curTime - params.multilineStartTime > Config.multilineDelay) {
    params.multilinePos += lineSize;
    params.multilineStartTime = curTime;
    displayDirty = true;
  }

  if (displayDirty) {
    memset(currentDisplayText, 0, sizeof(currentDisplayText));
    
    if (params.isTime) {
      static bool timeConfigured = false;

      if (!timeConfigured) {
        if (!WiFi.isConnected()) {      
          LOGLN("Wifi unavailable, couldn't configure time");
          strcpy(currentDisplayText, "NO WIFI");
          currentDisplayTextLen = sizeof("NO WIFI") - 1;
        } else {
          tm timeInfo;
          LOGLN("Configuring time");
          configTime(Config.timeZone, "pool.ntp.org");          
          if (getLocalTime(&timeInfo)) {
            timeConfigured = true;
          } else {
            strcpy(currentDisplayText, "NO TIME");
            currentDisplayTextLen = sizeof("NO TIME") - 1;
          }
        }
      } 
      if (timeConfigured) {
        tm timeInfo;
        if (getLocalTime(&timeInfo)) {
          char timeBuff[DISPLAY_MAX_CHARS+1];
          strftime(timeBuff, DISPLAY_MAX_CHARS, params.displayText, &timeInfo);
          currentDisplayTextLen = justifyText(currentDisplayText, DISPLAY_MAX_CHARS, timeBuff, DISPLAY_MAX_CHARS, params.justify);
        } else {
          strcpy(currentDisplayText, "NO TIME");
          currentDisplayTextLen = sizeof("NO TIME") - 1;
        }
      }
    } else {
      currentDisplayTextLen = justifyText(currentDisplayText, DISPLAY_MAX_CHARS, params.displayText, DISPLAY_MAX_CHARS, params.justify);
    }

    if (!params.multilineStartTime && currentDisplayTextLen > lineSize) {
      params.multilineStartTime = millis();
      params.multilinePos = 0;
    }

    if (params.multilinePos >= currentDisplayTextLen) params.multilinePos = 0;

    LOG("Displaying: ");
    for (unsigned int i = 0; i < lineSize; i++) {
      LOG(currentDisplayText[params.multilinePos + i]);
    }
    LOGLN();

    motorMoveToFlap(charToFlap(currentDisplayText[params.multilinePos]));

    for (unsigned int i = 0; i < nKnownModules; i++) {
      unsigned char res;
      unsigned char nRetries = 5;
      unsigned int curIdx = params.multilinePos + i + 1;
      char curChar = curIdx < currentDisplayTextLen ? currentDisplayText[curIdx] : ' ';
      char buff[6];
      snprintf(buff, 6, "f %d", charToFlap(curChar));
      do {
        Wire.beginTransmission(knownModules[i]);
        Wire.write(buff);
        Wire.write(0);
        res = Wire.endTransmission();
        delayMicroseconds(500); // To make it easier to see in the logic analyzer
      } while (res != 0 && nRetries--);
    }

    displayDirty = false;
  }
  lastSeconds = curSeconds;
}

void displayMessage(const char* message, unsigned int len, unsigned int seconds, bool time, DisplayJustify justify) {
  auto &params = seconds ? ephemeralDisplayParams : persistentDisplayParams;
  
  params.isTime = time;
  params.justify = justify;
  strncpy(params.displayText, message, DISPLAY_MAX_CHARS);
  
  // Clear ephemeral message either way
  lastEphemeralDisplayMillis = millis();
  ephemeralDisplayDurationMillis = seconds * 1000;

  // These need to be derived from the final display string which happens in displayEvents
  params.multilineStartTime = params.multilinePos = 0;

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