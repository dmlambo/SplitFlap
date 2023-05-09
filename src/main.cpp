#include <Arduino.h>

#include <Wire.h>
#include <EEPROM.h>

#include "Config.h"
#include "Commands.h"

#include "Motor.h"

#include <flash_hal.h>

ModuleConfig Config = { .isMaster = 0, .address = 0xFE, .zeroOffset = 0xFE };

String i2cCommand;
bool i2cOverflow = false;

void onReceiveI2C(size_t nBytes) {
  static int buffLoc = 0;
  static char buff[256] = { 0 };

  while (Wire.available()) { 
    buff[buffLoc] = Wire.read();
    if (buff[buffLoc] == '\n' || buff[buffLoc] == '\r') {
      i2cCommand = buff;
      buffLoc = 0;
      return;
    } else {
      buffLoc++;
      if (buffLoc > 255) {
        i2cOverflow = true;
        buffLoc = 0;
      }
    }
  }
}

void setup() {
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_IN3, OUTPUT);
  pinMode(MOTOR_IN4, OUTPUT);

  pinMode(HALL, INPUT_PULLUP);

  digitalWrite(MOTOR_IN1, 1);

  Serial.begin(57600);

  while (!Serial) { yield(); }

  delay(500);

  Serial.println("Welcome to Dave's Split Flap Experience!");

  EEPROM.begin(sizeof(ModuleConfig));
  memcpy(&Config, EEPROM.getConstDataPtr(), sizeof(ModuleConfig)); // On ESP8266 the EEPROM is mirrored in ram

  if (Config.rpm > 30) {
    Config.rpm = 30;
  }

  Serial.println("Active configuration:");
  Serial.print("isMaster: "); Serial.println(Config.isMaster);
  Serial.print("address: "); Serial.println((int)Config.address);
  Serial.print("zeroOffset: "); Serial.println((int)Config.zeroOffset);
  Serial.print("rpm: "); Serial.println((int)Config.rpm);

  Serial.println();

  printCommandHelp();

  if (Config.isMaster) {
    Serial.println("Starting in master mode.");
    Wire.begin();
    Wire.setClock(I2C_FREQUENCY);
    Wire.setClockStretchLimit(40000);
  } else {
      Serial.print("Starting in slave mode at address "); Serial.println((int)Config.address);
      Wire.begin(Config.address);
      Wire.setClock(I2C_FREQUENCY);
      Wire.setClockStretchLimit(40000);
      Wire.onReceive(onReceiveI2C);
  }

  motorInit();
  motorCalibrate();
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readString();

    handleCommand(command);
  }

  noInterrupts();
  String i2cCommandCopy = i2cCommand; i2cCommand.clear();
  bool i2cOverflowCopy = i2cOverflow; i2cOverflow = false;
  interrupts();

  if (!i2cCommandCopy.isEmpty()) {
    Serial.println("Reveived I2C command...");
    handleCommand(i2cCommandCopy);
  }

  if (i2cOverflowCopy) {
    Serial.println("I2C command buffer overflow.");
  }

  if (motorCalibrated) {
    motorCalibrated = false;
    Serial.println("Motor is calibrated.");
  }

  if (motorStalled) {
    motorStalled = false;
    Serial.println("Motor stalled.");
  }

  //motorDebugPrint();
}
