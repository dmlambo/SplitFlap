#include <Arduino.h>

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>

#include "Config.h"

#include "Communication.h"
#include "Motor.h"
#include "Display.h"

typedef void (*commandFunc)(unsigned char, const char**);

struct Command {
  const char* prefix;
  unsigned char nArgs;
  const char* description;
  commandFunc function;
  bool master;
};

void printCommandHelp();

bool argInRange(const char* arg, unsigned int min, unsigned int max, unsigned int* out) {
  char* end;
  unsigned long value = strtoul(arg, &end, 10);

  if (value > max || value < min || *end) {
    return false;
  }

  *out = value;
  return true;
}

bool argInRange(const char* arg, int min, int max, int* out) {
  char* end;
  long value = strtol(arg, &end, 10);

  if (value > max || value < min || *end) {
    return false;
  }

  *out = value;
  return true;
}

void resetCommand(unsigned char nArgs, const char** args) {
  LOGLN("Resetting...");
  ESP.reset();
}

void factoryResetCommand(unsigned char nArgs, const char** args) {
  LOGLN("Resetting to factory defaults...");
  Config.magic = ~CONFIG_MAGIC;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();
  Serial.flush();
  ESP.reset();
}

void setAddressCommand(unsigned char nArgs, const char** args) {
  int slaveAddr = 0;

  // 0-7 and 0x78-0x7F are I2C reserved
  if (!argInRange(args[1], I2C_DEVADDR_MIN, I2C_DEVADDR_MAX, &slaveAddr)) {
    LOGLN("Failed: Agument not in range 8-120");
    return;
  }

  Config.address = slaveAddr;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  LOG("New address set to "); LOGLN(slaveAddr);
  LOGLN("Resetting...");

  ESP.reset();
}

void setZeroOffsetCommand(unsigned char nArgs, const char** args) {
  int newZero = 0;

  if (!argInRange(args[1], 0, 255, &newZero)) {
    LOGLN("Failed: Argument not in range 0-255");
    return;
  }

  Config.zeroOffset = newZero;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  LOG("New zero point set to "); LOGLN(newZero);
  motorCalibrate();
}

void setSpeedCommand(unsigned char nArgs, const char** args) {
  int newSpeed = 0;

  if (!argInRange(args[1], 1, 30, &newSpeed)) {
    LOGLN("Failed: Argument not in range 1-30");
    return;
  }

  Config.rpm = newSpeed;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  LOG("New speed set to "); LOGLN(newSpeed);
  motorSetRPM(newSpeed);
}

void setMasterCommand(unsigned char nArgs, const char** args) {
  int newMaster = 0;

  if (!argInRange(args[1], 0, 1, &newMaster)) {
    LOGLN("Failed: Set master only takes 1 or 0");
    return;
  }

  Config.isMaster = newMaster;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  if (newMaster) {
    LOGLN("Now set to master.");
  } else {
    LOGLN("Now set to slave.");
  }
  LOGLN("Resetting...");

  ESP.reset();
}

void sendToSlaveCommand(unsigned char nArgs, const char** args) {
  int slaveAddr = 0;

  if (!argInRange(args[1], 0, I2C_DEVADDR_MAX, &slaveAddr)) {
    LOGLN("Failed: Argument not in range 0-" DEFTOLIT(I2C_DEVADDR_MAX));
    return;
  }

  LOG("Sending data to slave "); LOG(slaveAddr); LOGLN(":");
  LOGLN(args[2]);

  Wire.beginTransmission(slaveAddr);
  Wire.write(args[2]);
  Wire.write(0);
  Wire.endTransmission();
}

void calibrateMotorCommand(unsigned char nArgs, const char** args) {
  motorCalibrate();
}

void moveToFlapCommand(unsigned char nArgs, const char** args) {
  int newFlap = 0;

  if (!argInRange(args[1], 0, MOTOR_FLAPS-1, &newFlap)) {
    LOGLN("Failed: Flap number out of range (0-" DEFTOLIT(MOTOR_FLAPS) ")");
    return;
  }

  motorMoveToFlap(newFlap);
}

void setTimezoneCommand(unsigned char nArgs, const char** args) {
  if (strlen(args[1]) > CONFIG_TZSIZE) {
    LOGLN("Failed: Input too long (max" DEFTOLIT(CONFIG_TZSIZE) ")");
    return;
  }

  LOG("Setting timezone to "); LOGLN(args[1]);
  displaySetTimeZone(args[1]);
  strncpy(Config.timeZone, args[1], CONFIG_TZSIZE);
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();  
}

void displayCommand(unsigned char nArgs, const char** args) {
  int ephemeral;
  int time;

  if (!argInRange(args[2], 0, 3600, &ephemeral)) {
    LOGLN("Failed: Ephemeral out of range 0 to 3600");
    return;
  }

  if (!argInRange(args[3], 0, 1, &time)) {
    LOGLN("Failed: Date takes 1 or 0");
    return;
  }

  displayMessage(args[1], strlen(args[1]), ephemeral, time);
}

void showHelpCommand(unsigned char nArgs, const char** args) {
  printCommandHelp();
}

void showConfigCommand(unsigned char nArgs, const char** args) {
  printConfig();
}

void updateModulesCommand(unsigned char nArgs, const char** args) {
  const char* SSID = "SFUpd";
  const char* moduleCmd = "cu SFUpd";

  if (!WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0))) {
    LOGLN("Failed: Could not configure soft AP");
    WiFi.softAPdisconnect();
    return;
  }

  // If we have more than 9 modules we would need a queuing system
  if (!WiFi.softAP(SSID, emptyString, 0, 1, 8)) { // hidden, 8 connections
    LOGLN("Failed: Could not turn on soft AP");
    WiFi.softAPdisconnect();
    return;
  }

  unsigned char nModules = 0;
  unsigned char staleModules[DISPLAY_MAX_MODULES] = { 0 };

  for (int i = 0; i < nKnownModules; i++) {
    ModuleStatus status;
    PacketStatus packetStatus = i2cReadStruct(knownModules[i], &status);
    if ((packetStatus == PACKET_OK && status.version != VERSION) || packetStatus != PACKET_EMPTY) {
      staleModules[nModules++] = knownModules[i];
    }
  }

  if (!nModules) {
    LOGLN("All modules up to date.");
    WiFi.softAPdisconnect();
    return;
  }

  for (int i = 0; i < nModules; i++) {
    char retries = I2C_RETRIES;

    while (retries--) {
      delay(10);
      Wire.beginTransmission(staleModules[i]);
      Wire.write(moduleCmd);
      Wire.write(0);
      if (!Wire.endTransmission()) {
        retries = 0;
      }
    }
  }

  modulesContacted = 0; // Modules who have connected to us.
  modulesFinishedUpdate = 0; // Modules who have ended the connection.

  LOG("Contacted "); LOG(nModules); LOGLN(" modules...");
  LOGLN("Waiting for connections...");
  unsigned long curMillis = millis();
  while (millis() - curMillis < 300000) {
    LOG(modulesContacted); LOG(" modules have made contact, "); LOG(modulesFinishedUpdate); LOGLN(" have finished downloading.");
    if (modulesFinishedUpdate == nModules) {
      LOGLN("All modules finished downloading firmware!");
      break;
    }
    delay(1000);
  }
  // TODO: Get status from each module and compare against this one.
  LOGLN("Done... check status for version numbers.");

  WiFi.softAPdisconnect();
}

void beginUpdateCommand(unsigned char nArgs, const char** args) {
  WiFi.enableSTA(true);
  WiFi.setAutoReconnect(false);
  WiFi.begin(args[1]);

  LOGLN("Starting WiFi update process...");
  LOG("Connecting to SSID "); LOGLN(args[1]);

  if (!WiFi.waitForConnectResult(60000)) {
    LOG("Failed: Could not connect to AP after 60s: "); LOGLN(args[1]);
    WiFi.disconnect(true, true);
    return;
  }

  WiFiClient client;
  HTTPClient httpClient;

  //httpClient.setReuse(true);
  httpClient.begin(client, String("http://") + WiFi.gatewayIP().toString() + "/update.bin");
  
  int responseCode = httpClient.GET();

  if (responseCode != HTTP_CODE_OK) {
    LOG("Failed: HTTP response code: "); LOGLN(responseCode);
    httpClient.end();
    WiFi.disconnect(true, true);
    return;
  }

  unsigned int size = httpClient.getSize();

  LOG("Remote firmware size: "); LOGLN(size);

  if(!Update.begin(size, U_FLASH)) {
    LOGLN("Failed: Update.begin returned false:");
    LOGLN(Update.getErrorString());
    httpClient.end();
    WiFi.disconnect(true, true);
    return;
  }

  unsigned int totalRead = 0;
  unsigned int retry = 5;

  while (httpClient.connected() && --retry) {
    unsigned char buff[64];
    unsigned int read = httpClient.getStream().readBytes(buff, sizeof(buff));
    //unsigned int read = client.readBytes(buff, sizeof(buff));
    if (!read) {
      continue;
    }
    retry = 5;
    if (Update.write(buff, read) != read) {
      LOGLN("Failed: Couldn't write all bytes from stream:");
      LOGLN(Update.getErrorString());
      WiFi.disconnect(true, true);
      return;
    }
    totalRead += read;
    if (totalRead == size) break;
  }

  LOG("Read "); LOG(totalRead); LOGLN(" bytes from server");

  httpClient.end();

  if(!Update.end()) {
    LOGLN("Failed: Update end returned false:");
    LOGLN(Update.getErrorString());
    WiFi.disconnect(true, true);
    return;
  }

  LOGLN("Update finished, rebooting...");
  delay(1000);
  WiFi.disconnect(true, true);
  delay(1000);
  ESP.restart();
}

static Command commands[] = {
  { "h",      0, "Show this help",                                                                showHelpCommand,        false },
  { "f",      1, "Move to flap (f [0-" DEFTOLIT((MOTOR_FLAPS - 1))"])",                           moveToFlapCommand,      false },
  { "m",      1, "Set master mode (m [0|1])",                                                     setMasterCommand,       false },
  { "a",      1, "Set address (a [" DEFTOLIT(I2C_DEVADDR_MIN) "-" DEFTOLIT(I2C_DEVADDR_MAX) "])", setAddressCommand,      false },
  { "z",      1, "Set zero point offset (z [0-255])",                                             setZeroOffsetCommand,   false },
  { "c",      0, "Calibrate motor to 0 position",                                                 calibrateMotorCommand,  false },
  { "s",      1, "Speed in RPM (s [1-30])",                                                       setSpeedCommand,        false },
  { "r",      0, "Reset",                                                                         resetCommand,           false },
  { "cfg",    0, "Show configuration",                                                            showConfigCommand,      false },
  { "msg",    3, "Display message (msg \"message\" 10 0)",                                        displayCommand,         true },
  { "x",      2, "Send command to slave (x [0-" DEFTOLIT(I2C_DEVADDR_MAX) "] \"...\")",           sendToSlaveCommand,     true },
  { "tz",     1, "Set POSIX timezone (tz \"PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00\")",            setTimezoneCommand,     true },
  { "cu",     1, "Connect to adhoc update AP (cu UpdateSSID)",                                    beginUpdateCommand,     false },
  { "xu",     0, "Update modules with host firmware",                                             updateModulesCommand,   true },
  { "frfrfr", 0, "Factory reset",                                                                 factoryResetCommand,    false },
};

void printCommandHelp() {
  LOGLN("Here are the commands available to you:");
  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if (!commands[i].master || Config.isMaster) {
      char buff[255];
      snprintf(buff, 255, "%8s    %s", commands[i].prefix, commands[i].description);
      LOGLN(buff);
    }
  }
  if (!Config.isMaster) {
    LOGLN("Some commands not available in slave mode not shown");
  }  
}

// Modifies command argument!
void handleCommand(char* command) {
  if (!*command) {
    LOGLN("Empty command buffer");
    return;
  }

  // LOG("Received command: "); LOGLN(command);

	bool quote = false;
	bool delim = true;

	const char* args[16] = { 0 };
	unsigned char nArgs = 0;

	for (char* pos = command; *pos != 0; pos++) {
		switch (*pos) {
      case '\r':
      case '\n':
        *pos = 0;
        pos--; // will break next loop
        break;
      case '"':
        *pos = 0;
        quote = !quote;
        if (quote) {
          delim = true;
        } else {
          if (delim) // Empty quotes serve as an argument
            args[nArgs++] = pos;
          else
            delim = true;
        }
        break;
      case ' ':
        if (!quote) {
          delim = true;
          *pos = 0;
          break;
        }
        // Fallthrough if we're between quotations
      default:
        if (delim) {
          delim = false;
          args[nArgs++] = pos;
        }
        break;
		}
	}

  if (quote) {
    LOG("Missing closing quotation");
    return;
  }

  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if ((!commands[i].master || Config.isMaster) && !strcmp(args[0], commands[i].prefix)) {
      if (nArgs != commands[i].nArgs + 1) {
        LOG("Command takes "); LOG(commands[i].nArgs); LOGLN(" argument(s)!");
        return;
      }
      // If our timer is running, SPI has an issue writing to flash... likely I2C will have an issue too.
      disableMotorTimer();
      commands[i].function(nArgs, args);
      enableMotorTimer();
      return;
    }
  }
  LOGLN("Unknown command");
  printCommandHelp();
}