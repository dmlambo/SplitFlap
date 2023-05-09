#include <Arduino.h>

#include <EEPROM.h>

#include "Config.h"

#include "Motor.h"

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
  Serial.println("Calibrating...");
  motorCalibrate();
}

void setSpeedCommand(String speed, String two, String three, String four) {
  int newSpeed = speed.toInt();

  if (newSpeed > 30 || newSpeed < 1) {
    Serial.println("Failed: Speed not in range 1-30");
    return;
  }

  Config.rpm = newSpeed;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  Serial.print("New speed set to "); Serial.println(newSpeed);
  motorSetRPM(newSpeed);
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

  Serial.println("Even Here?");
  Serial.flush();

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

void calibrateMotorCommand(String one, String two, String three, String four) {
  Serial.println("Calibrating motor...");
  motorCalibrate();
}

static Command commands[] = {
  { .prefix = 'm', .description = "Set master mode (m [0|1])", .function = setMasterCommand },
  { .prefix = 'a', .description = "Set address (a [1-255])", .function = setAddressCommand },
  { .prefix = 'z', .description = "Set zero point offset (z [0-255])", .function = setZeroOffsetCommand },
  { .prefix = 'c', .description = "Calibrate motor to 0 position", calibrateMotorCommand },
  { .prefix = 's', .description = "Speed in RPM (s [1-30])", setSpeedCommand },
  { .prefix = 'r', .description = "Reset", .function = resetCommand },
};

void printCommandHelp() {
  Serial.println("Here are the commands available to you:");
  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    char buff[255];
    snprintf(buff, 255, "  %c    %s", commands[i].prefix, commands[i].description);
    Serial.println(buff);
  }
}

void handleCommand(String command) {
  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if (command[0] == commands[i].prefix) {
      char scan[] = "X %s %s %s %s";
      char args[4][20];

      scan[0] = commands[i].prefix;

      // My kingdom for split.
      sscanf(command.c_str(), scan, &args[0], &args[1], &args[2], &args[3]);

      disableMotorTimer();
      commands[i].function(args[0], args[1], args[2], args[3]);
      enableMotorTimer();
      return;
    }
  }
  Serial.println("Unknown command");
  printCommandHelp();
}