#include <Arduino.h>

#include <Wire.h>
#include <EEPROM.h>

#include "Config.h"
#include "Commands.h"

#include "Motor.h"

#include <flash_hal.h>

ModuleConfig Config = { .isMaster = 0, .address = 0xFE, .zeroOffset = 0xFE };

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
  Serial.print("address: 0x"); Serial.println(Config.address, 16);
  Serial.print("zeroOffset: "); Serial.println((int)Config.zeroOffset);
  Serial.print("rpm: "); Serial.println((int)Config.rpm);

  Serial.println();

  Serial.print("Flash location: "); Serial.println(EEPROM_start, 16);

  printCommandHelp();

  motorInit();
  motorCalibrate();
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readString();

    handleCommand(command);
  }

  if (motorCalibrated) {
    motorCalibrated = false;
    Serial.println("Motor is calibrated.");
  }
}
