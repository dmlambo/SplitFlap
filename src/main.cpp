#include <Arduino.h>

#include <Wire.h>
#include <EEPROM.h>

#include "Config.h"
#include "Commands.h"

ModuleConfig Config;

void setup() {
  Serial.begin(57600);

  while (!Serial) { yield(); }

  delay(500);

  Serial.println("Welcome to Dave's Split Flap Experience!");

  EEPROM.begin(sizeof(ModuleConfig));
  memcpy(&Config, EEPROM.getConstDataPtr(), sizeof(ModuleConfig)); // On ESP8266 the EEPROM is mirrored in ram

  Serial.println("Active configuration:");
  Serial.print("isMaster: "); Serial.println(Config.isMaster);
  Serial.print("address: 0x"); Serial.println(Config.address, 16);
  Serial.print("zeroOffset: "); Serial.println((int)Config.zeroOffset);

  Serial.println();

  printCommandHelp();
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readString();

    handleCommand(command);
  }
}
