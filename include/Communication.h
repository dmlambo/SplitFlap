#pragma once

#include "Config.h"

enum Status { 
  MODULE_OK,
  MODULE_MOVING,
  MODULE_STALLED,
  MODULE_CALIBRATING,
  MODULE_OVERFLOW, // Likely communication problem
  MODULE_UNAVAILABLE,
};

extern const char* StatusStr[];
extern bool i2cOverflow ;
extern Status deviceLastStatus;
extern unsigned char knownModules[];
extern unsigned char nKnownModules;

void enumerateModules();
void onRequestI2C();
void onReceiveI2C(size_t size);
char* i2cReadCommand();