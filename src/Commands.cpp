#include <Arduino.h>

#include <EEPROM.h>

#include "Config.h"

typedef void (*commandFunc)(String, String, String, String);

struct Command {
  char prefix;
  const char* description;
  commandFunc function;
};

void resetCommand(String one, String two, String three, String four) {
  Serial.println("Resetting...");
  ESP.reset();
}

void setAddressCommand(String address, String two, String three, String four) {
  int newAddress = address.toInt();

  if (newAddress > 255 || newAddress < 1) {
    Serial.println("Failed: Address not in range 1-255");
    return;
  }

  Config.address = newAddress;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  Serial.print("New address set to "); Serial.println(newAddress);
  Serial.println("Resetting...");

  ESP.reset();
}

void setZeroOffsetCommand(String address, String two, String three, String four) {
  int newZero = address.toInt();

  if (newZero > 255 || newZero < 0) {
    Serial.println("Failed: Address not in range 1-255");
    return;
  }

  Config.zeroOffset = newZero;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  Serial.print("New zero point set to "); Serial.println(newZero);
  Serial.println("Resetting...");

  ESP.reset();
}

void setMasterCommand(String address, String two, String three, String four) {
  int newMaster = address.toInt();

  if (newMaster != 0 && newMaster != 1) {
    Serial.println("Failed: Set master only takes 1 or 0");
    return;
  }

  Config.isMaster = newMaster;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  if (newMaster) {
    Serial.println("Now set to master.");
  } else {
    Serial.println("Now set to slave.");
  }
  Serial.println("Resetting...");

  ESP.reset();
}

void sendToSlaveCommand(String address, String two, String three, String four) {
  Serial.println("Send to slave not yet implemented");
}

static Command commands[] = {
  { .prefix = 'm', .description = "Set master mode (m [0|1])", .function = setMasterCommand },
  { .prefix = 'a', .description = "Set address (a [1-255])", .function = setAddressCommand },
  { .prefix = 'z', .description = "Set zero point offset (z [0-255])", .function = setZeroOffsetCommand },
  { .prefix = 's', .description = "Send to slave at address (s [0-255][otherCommand])" },
  { .prefix = 'r', .description = "Reset", .function = resetCommand },
};

void printCommandHelp() {
  Serial.println("Here are the commands available to you:");
  for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    char buff[255];
    snprintf(buff, 255, "  %c    %s", commands[i].prefix, commands[i].description);
    Serial.println(buff);
  }
}

void handleCommand(String command) {
  for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if (command[0] == commands[i].prefix) {
      char scan[] = "X %s %s %s %s";
      char args[4][20];

      scan[0] = commands[i].prefix;

      // My kingdom for split.
      sscanf(command.c_str(), scan, &args[0], &args[1], &args[2], &args[3]);

      commands[i].function(args[0], args[1], args[2], args[3]);
    }
  }
}