#include <Arduino.h>

#include <EEPROM.h>
#include <Wire.h>

#include "Config.h"

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

bool argToIntRange(const char* arg, int min, int max, int* out) {
  char* end;
  int value = strtol(arg, &end, 10);

  if (value > max || value < min || *end) {
    return false;
  }

  *out = value;
  return true;
}

void resetCommand(unsigned char nArgs, const char** args) {
  Serial.println("Resetting...");
  ESP.reset();
}

void factoryResetCommand(unsigned char nArgs, const char** args) {
  Serial.println("Resetting to factory defaults...");
  Config.magic = ~CONFIG_MAGIC;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();
  Serial.flush();
  ESP.reset();
}

void setAddressCommand(unsigned char nArgs, const char** args) {
  int slaveAddr = 0;

  // 0-7 and 0x78-0x7F are I2C reserved
  if (!argToIntRange(args[1], I2C_DEVADDR_MIN, I2C_DEVADDR_MAX, &slaveAddr)) {
    Serial.println("Failed: Agument not in range 8-120");
    return;
  }

  Config.address = slaveAddr;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  Serial.print("New address set to "); Serial.println(slaveAddr);
  Serial.println("Resetting...");

  ESP.reset();
}

void setZeroOffsetCommand(unsigned char nArgs, const char** args) {
  int newZero = 0;

  if (!argToIntRange(args[1], 0, 255, &newZero)) {
    Serial.println("Failed: Argument not in range 0-255");
    return;
  }

  Config.zeroOffset = newZero;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  Serial.print("New zero point set to "); Serial.println(newZero);
  motorCalibrate();
}

void setSpeedCommand(unsigned char nArgs, const char** args) {
  int newSpeed = 0;

  if (!argToIntRange(args[1], 1, 30, &newSpeed)) {
    Serial.println("Failed: Argument not in range 1-30");
    return;
  }

  Config.rpm = newSpeed;
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();

  Serial.print("New speed set to "); Serial.println(newSpeed);
  motorSetRPM(newSpeed);
}

void setMasterCommand(unsigned char nArgs, const char** args) {
  int newMaster = 0;

  if (!argToIntRange(args[1], 0, 1, &newMaster)) {
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

void sendToSlaveCommand(unsigned char nArgs, const char** args) {
  int slaveAddr = 0;

  if (!argToIntRange(args[1], 0, I2C_DEVADDR_MAX, &slaveAddr)) {
    Serial.println("Failed: Argument not in range 0-" DEFTOLIT(I2C_DEVADDR_MAX));
    return;
  }

  Serial.print("Sending data to slave "); Serial.print(slaveAddr); Serial.println(":");
  Serial.println(args[2]);

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

  if (!argToIntRange(args[1], 0, MOTOR_FLAPS-1, &newFlap)) {
    Serial.println("Failed: Flap number out of range (0-" DEFTOLIT(MOTOR_FLAPS) ")");
    return;
  }

  motorMoveToFlap(newFlap);
}

void setTimezoneCommand(unsigned char nArgs, const char** args) {
  if (strlen(args[0]) > CONFIG_TZSIZE) {
    Serial.println("Failed: Input too long (max" DEFTOLIT(CONFIG_TZSIZE) ")");
    return;
  }
  
  Serial.print("Setting timezone to "); Serial.println(args[1]);
  displaySetTimeZone(args[1]);
  strncpy(Config.timeZone, args[1], CONFIG_TZSIZE);
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();  
}

void displayCommand(unsigned char nArgs, const char** args) {
  int ephemeral;
  int time;

  if (!argToIntRange(args[2], 0, 3600, &ephemeral)) {
    Serial.println("Failed: Ephemeral out of range 0 to 3600");
    return;
  }

  if (!argToIntRange(args[3], 0, 1, &time)) {
    Serial.println("Failed: Date takes 1 or 0");
    return;
  }

  displayMessage(args[1], ephemeral, time);
}

void showHelpCommand(unsigned char nArgs, const char** args) {
  printCommandHelp();
}

static Command commands[] = {
  { .prefix = "h", .nArgs = 0, .description = "Show this help", .function = showHelpCommand, .master = false },
  { .prefix = "f", .nArgs = 1, .description = "Move to flap (f [0-" DEFTOLIT(MOTOR_FLAPS - 1) "])", .function = moveToFlapCommand, .master = false },
  { .prefix = "m", .nArgs = 1, .description = "Set master mode (m [0|1])", .function = setMasterCommand, .master = false },
  { .prefix = "a", .nArgs = 1, .description = "Set address (a [" DEFTOLIT(I2C_DEVADDR_MIN) "-" DEFTOLIT(I2C_DEVADDR_MAX) "])", .function = setAddressCommand, .master = false },
  { .prefix = "z", .nArgs = 1, .description = "Set zero point offset (z [0-255])", .function = setZeroOffsetCommand, .master = false },
  { .prefix = "c", .nArgs = 0, .description = "Calibrate motor to 0 position", calibrateMotorCommand, .master = false },
  { .prefix = "s", .nArgs = 1, .description = "Speed in RPM (s [1-30])", setSpeedCommand, .master = false },
  { .prefix = "r", .nArgs = 0, .description = "Reset", .function = resetCommand, .master = false },
  { .prefix = "msg", .nArgs = 3, .description = "Display message (msg \"message\" 10 0)", .function = displayCommand, .master = true },
  { .prefix = "x", .nArgs = 2, .description = "Send command to slave (x [0-" DEFTOLIT(I2C_DEVADDR_MAX) "] \"...\")", sendToSlaveCommand, .master = true },
  { .prefix = "tz", .nArgs = 1, .description = "Set POSIX timezone (tz \"Africa/Bamako\")", setTimezoneCommand, .master = true },
  { .prefix = "frfrfr", .nArgs = 0, .description = "Factory reset", .function = factoryResetCommand, .master = false },
};

void printCommandHelp() {
  Serial.println("Here are the commands available to you:");
  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if (!commands[i].master || Config.isMaster) {
      char buff[255];
      snprintf(buff, 255, "  %10s    %s", commands[i].prefix, commands[i].description);
      Serial.println(buff);
    }
  }
  if (!Config.isMaster) {
    Serial.println("Some commands not available in slave mode not shown");
  }  
}

// Modifies command argument!
void handleCommand(char* command) {
  if (!*command) {
    Serial.println("Empty command buffer");
    return;
  }

  Serial.print("Received command: "); Serial.println(command);

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
    Serial.print("Missing closing quotation");
    return;
  }

  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if ((!commands[i].master || Config.isMaster) && !strcmp(args[0], commands[i].prefix)) {
      if (nArgs != commands[i].nArgs + 1) {
        Serial.print("Command takes "); Serial.print(commands[i].nArgs); Serial.println(" argument(s)!");
        return;
      }
      // If our timer is running, SPI has an issue writing to flash... likely I2C will have an issue too.
      disableMotorTimer();
      commands[i].function(nArgs, args);
      enableMotorTimer();
      return;
    }
  }
  Serial.println("Unknown command");
  printCommandHelp();
}