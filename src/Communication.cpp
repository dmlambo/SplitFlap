#include <Arduino.h>
#include <Wire.h>

#include "Config.h"

#include "Communication.h"

const char* StatusStr[] = {
  "OK",
  "MOVING",
  "STALLED",
  "CALIBRATING",
  "OVERFLOW",
  "UNAVAILABLE"
};

bool i2cOverflow = false;
Status deviceLastStatus = MODULE_OK;
unsigned char knownModules[DISPLAY_MAX_MODULES] = {0};
unsigned char nKnownModules = 0;

void enumerateModules() {
  for (unsigned char i = I2C_DEVADDR_MIN; i <= I2C_DEVADDR_MAX && nKnownModules < DISPLAY_MAX_MODULES; i++) {
    if (Wire.requestFrom((int)i, (int)1)) {
      knownModules[nKnownModules++] = i;
    }
    yield();
  }
}

char* i2cReadCommand() {
  static int buffLoc = 0;
  static char buff[I2C_BUFF_LEN] = { 0 };

  // We only read strings, and there may be, potentially, more than one string in the buffer.
  while (Wire.available()) {    
    buff[buffLoc] = Wire.read();
    if (buff[buffLoc] == 0 || buff[buffLoc] == '\n' || buff[buffLoc] == '\r') {
      buff[buffLoc] = 0; // Remove newline
      buffLoc = 0;
      return buff;
    } else {
      buffLoc++;
      if (buffLoc > I2C_BUFF_LEN - 1) {
        i2cOverflow = true;
        buffLoc = 0;
      }
    }
  }
  return NULL;
}

// The only thing we return is our status
void onRequestI2C() {
  Wire.write((unsigned char)deviceLastStatus);
}

void onReceiveI2C(size_t size) {
  // Do nothing, we just need this here to coax the Wire library to work.
}