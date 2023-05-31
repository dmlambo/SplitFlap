#include <Arduino.h>

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>

#include "Config.h"

#include "Commands.h"
#include "Communication.h"
#include "Motor.h"
#include "Display.h"
#include "Streams.h"
#include "Utils.h"

typedef bool (*commandFunc)(unsigned char, const char**, Print* out);

struct Command {
  const char* prefix;
  unsigned char nArgs;
  const char* description;
  commandFunc function;
  bool master;
};

struct CommandParams {
  commandFunc func;
  char cmdBuff[256]; 
  unsigned char nArgs;
  const char* args[16];
  Print* out;

  bool invoke() {
    if (func) {
      return func(nArgs, args, out);
    } else {
      return false;
    }
  }
};

CommandParams queuedParams = {0}; // Storage
QueuedCommand queuedCommand = { .queued = false, .params = &queuedParams };

void printCommandHelp(Print* out);

bool resetCommand(unsigned char nArgs, const char** args, Print* out) {
  out->printf("Resetting...\n");
  markForReboot(500);
  return true;
}

bool factoryResetCommand(unsigned char nArgs, const char** args, Print* out) {
  out->printf("Resetting to factory defaults...\n");
  Config.magic = ~CONFIG_MAGIC;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();
  Serial.flush();
  markForReboot(500);
  return true;
}

bool setAddressCommand(unsigned char nArgs, const char** args, Print* out) {
  int slaveAddr = 0;

  // 0-7 and 0x78-0x7F are I2C reserved
  if (!argInRange(args[1], I2C_DEVADDR_MIN, I2C_DEVADDR_MAX, &slaveAddr)) {
    out->printf("Failed: Agument not in range 8-120\n");
    return false;
  }

  Config.address = slaveAddr;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  out->printf("New address set to %u\n", slaveAddr);
  out->printf("Resetting...\n");

  markForReboot(500);
  return true;
}

bool setZeroOffsetCommand(unsigned char nArgs, const char** args, Print* out) {
  int newZero = 0;

  if (!argInRange(args[1], 0, 255, &newZero)) {
    out->print("Failed: Argument not in range 0-255\n");
    return false;
  }

  Config.zeroOffset = newZero;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  out->printf("New zero point set to %u\n", newZero);
  motorMoveToFlap(motorCurrentFlap());
  return true;
}

bool setSpeedCommand(unsigned char nArgs, const char** args, Print* out) {
  int newSpeed = 0;

  if (!argInRange(args[1], 1, 30, &newSpeed)) {
    out->printf("Failed: Argument not in range 1-30\n");
    return false;
  }

  Config.rpm = newSpeed;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  out->printf("New speed set to %u\n", newSpeed);
  motorSetRPM(newSpeed);
  return true;
}

bool setMasterCommand(unsigned char nArgs, const char** args, Print* out) {
  int newMaster = 0;

  if (!argInRange(args[1], 0, 1, &newMaster)) {
    out->printf("Failed: Set master only takes 1 or 0\n");
    return false;
  }

  Config.isMaster = newMaster;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  if (newMaster) {
    out->printf("Now set to master.\n");
  } else {
    out->printf("Now set to slave.\n");
  }
  out->printf("Resetting...\n");

  markForReboot(500);
  return true;
}

bool sendToSlaveCommand(unsigned char nArgs, const char** args, Print* out) {
  int slaveAddr = 0;

  if (!argInRange(args[1], 0, I2C_DEVADDR_MAX, &slaveAddr)) {
    out->printf("Failed: Argument not in range 0-" DEFTOLIT(I2C_DEVADDR_MAX) "\n");
    return false;
  }

  out->printf("Sending data to slave %u: %s\n", slaveAddr, args[2]);

  Wire.beginTransmission(slaveAddr);
  Wire.write(args[2]);
  Wire.write(0);
  Wire.endTransmission();
  return true;
}

bool calibrateMotorCommand(unsigned char nArgs, const char** args, Print* out) {
  out->print("Calibrating...");
  motorCalibrate();
  return true;
}

bool moveToFlapCommand(unsigned char nArgs, const char** args, Print* out) {
  int newFlap = 0;

  if (!argInRange(args[1], 0, MOTOR_FLAPS-1, &newFlap)) {
    out->printf("Failed: Flap number out of range (0-" DEFTOLIT(MOTOR_FLAPS) ")\n");
    return false;
  }

  motorMoveToFlap(newFlap);
  return true;
}

bool setTimezoneCommand(unsigned char nArgs, const char** args, Print* out) {
  if (strlen(args[1]) > CONFIG_TZSIZE) {
    out->printf("Failed: Input too long (max" DEFTOLIT(CONFIG_TZSIZE) ")\n");
    return false;
  }

  out->printf("Setting timezone to %s\n", args[1]);
  displaySetTimeZone(args[1]);
  strncpy(Config.timeZone, args[1], CONFIG_TZSIZE);
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();  
  return true;
}

bool displayCommand(unsigned char nArgs, const char** args, Print* out) {
  int ephemeral;
  int time;

  if (!argInRange(args[2], 0, 3600, &ephemeral)) {
    out->printf("Failed: Ephemeral out of range 0 to 3600\n");
    return false;
  }

  if (!argInRange(args[3], 0, 1, &time)) {
    out->printf("Failed: Date takes 1 or 0\n");
    return false;
  }

  displayMessage(args[1], strlen(args[1]), ephemeral, time);
  return true;
}

bool showHelpCommand(unsigned char nArgs, const char** args, Print* out) {
  printCommandHelp(out);
  return true;
}

bool showConfigCommand(unsigned char nArgs, const char** args, Print* out) {
  printConfig(out);
  return true;
}

bool updateModulesCommand(unsigned char nArgs, const char** args, Print* out) {
  const char* SSID = "SFUpd";
  const char* moduleCmd = "cu SFUpd";

  if (!WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0))) {
    out->printf("Failed: Could not configure soft AP\n");
    WiFi.softAPdisconnect();
    return false;
  }

  // If we have more than 9 modules we would need a queuing system
  if (!WiFi.softAP(SSID, emptyString, 0, 1, 8)) { // hidden, 8 connections
    out->printf("Failed: Could not turn on soft AP\n");
    WiFi.softAPdisconnect();
    return false;
  }

  unsigned char nModules = 0;
  unsigned char staleModules[DISPLAY_MAX_MODULES] = { 0 };

  for (int i = 0; i < nKnownModules; i++) {
    ModuleStatus status;
    PacketStatus packetStatus = i2cReadStruct(knownModules[i], &status);
    if ((packetStatus == PACKET_OK && status.version != VERSION) || packetStatus > PACKET_EMPTY) {
      staleModules[nModules++] = knownModules[i];
    }
  }

  if (!nModules) {
    out->print("All modules up to date.\n");
    WiFi.softAPdisconnect();
    return false;
  }

  moduleContacted = false; // Modules who have connected to us.
  moduleFinishedUpdate = false; // Modules who have ended the connection.

  out->printf("%u modules need updating...\n", nModules);

  for (int n = 0; n < nModules; n++) {
    char retries = I2C_RETRIES;
    while (retries--) {
      out->printf("Contacting %d...\n", staleModules[n]);

      Wire.beginTransmission(staleModules[n]);
      Wire.write(moduleCmd);
      Wire.write(0);
      Wire.endTransmission();

      unsigned int startMillis = millis();
      do {
        delay(1000);
        if (moduleContacted) {
          out->print("Module contacted! Waiting for it to finish updating...\n");
          retries = 0;
          break;
        }
      } while (millis() - startMillis < 10000);
    }
    if (moduleContacted) {
      for (int i = 0; i < 50; i++) { // Wait 2:50
        out->print("Waiting...\n");
        delay(3000);
        if (moduleFinishedUpdate) {
          break;
        }
      }
    }
    if (moduleFinishedUpdate) {
      out->print("Done!\n");
    } else {
      out->print("Failed to update, moving on...\n");
    }
    moduleFinishedUpdate = moduleContacted = false;
  }

  // TODO: Get status from each module and compare against this one.
  out->printf("Done... check status for version numbers.\n");

  WiFi.softAPdisconnect();
  return true;
}

bool beginUpdateCommand(unsigned char nArgs, const char** args, Print* out) {
  WiFi.enableSTA(true);
  WiFi.setAutoReconnect(false);
  WiFi.begin(args[1]);

  out->printf("Starting WiFi update process...\n");
  out->printf("Connecting to SSID %s\n", args[1]);

  if (!WiFi.waitForConnectResult(60000)) {
    out->printf("Failed: Could not connect to AP after 60s\n");
    WiFi.disconnect(true, true);
    return false;
  }

  WiFiClient client;
  HTTPClient httpClient;

  httpClient.begin(client, String("http://") + WiFi.gatewayIP().toString() + "/update.bin");
  
  int responseCode = httpClient.GET();

  if (responseCode != HTTP_CODE_OK) {
    out->printf("Failed: HTTP response code: %u\n", responseCode);
    httpClient.end();
    WiFi.disconnect(true, true);
    return false;
  }

  unsigned int size = httpClient.getSize();

  out->printf("Remote firmware size: %u\n", size);

  if(!Update.begin(size, U_FLASH)) {
    out->print("Failed: Update.begin returned false:\n");
    out->printf("  %s\n", Update.getErrorString().c_str());
    httpClient.end();
    WiFi.disconnect(true, true);
    return false;
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
      out->print("Failed: Couldn't write all bytes from stream:\n");
      out->printf("  %s\n", Update.getErrorString().c_str());
      WiFi.disconnect(true, true);
      return false;
    }
    totalRead += read;
    if (totalRead == size) break;
  }

  out->printf("Read %u bytes from server\n", totalRead);

  httpClient.end();

  if(!Update.end()) {
    out->print("Failed: Update end returned false:\n");
    out->printf("  %s\n", Update.getErrorString().c_str());
    WiFi.disconnect(true, true);
    return false;
  }

  out->print("Update finished, rebooting...\n");
  delay(1000);
  WiFi.disconnect(true, true);
  markForReboot(500);
  return true;
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

void printCommandHelp(Print* out) {
  out->print("Here are the commands available to you:\n");
  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if (!commands[i].master || Config.isMaster) {
      out->printf("%8s    %s\n", commands[i].prefix, commands[i].description);
    }
  }
  if (!Config.isMaster) {
    out->print("Some commands not available in slave mode not shown\n");
  }  
}

static bool parseCommand(char* command, CommandParams& params, Print* out) {
  params.func = NULL;
  params.nArgs = 0;
  params.out = out;

  if (!*command) {
    out->print("Empty command buffer\n");
    return false;
  }

	bool quote = false;
	bool delim = true;

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
            params.args[params.nArgs++] = pos;
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
          params.args[params.nArgs++] = pos;
        }
        break;
		}
	}

  if (quote) {
    out->print("Missing closing quotation\n");
    return false;
  }

  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if ((!commands[i].master || Config.isMaster) && !strcmp(params.args[0], commands[i].prefix)) {
      if (params.nArgs != commands[i].nArgs + 1) {
        out->printf("Command takes %d argument(s)!\n", commands[i].nArgs);
        return false;
      }
      params.func = commands[i].function;
      return true;
    }
  }
  out->print("Unknown command\n");
  printCommandHelp(out);
  return false;
}

bool handleCommandAsync(char* command, Print* out) {
  CommandParams params = {0};

  if (parseCommand(command, params, out)) {
    noInterrupts();
    bool queued = false;
    if (!queuedCommand.queued) {
      queuedParams = params;
      queued = queuedCommand.queued = true;
      queuedCommand.finished = false;
    }
    interrupts();

    if (!queued) {
      out->print("Queued command overflow\n");
      return false;
    }
    return true;
  } else {
    return false;
  }
}

// Modifies command argument!
bool handleCommand(char* command, Print* out) {
  CommandParams params;

  if (!parseCommand(command, params, out)) {
    return false;
  }
  return handleCommand(&params);
}

bool handleCommand(CommandParams* params) {
  return params->invoke();
}