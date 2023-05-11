#include <Arduino.h>

#include <Wire.h>
#include <EEPROM.h>
#include <LittleFS.h>

//#include <ESPAsyncWebServer.h>
#include <ezTime.h>
#include <WiFiManager.h>

#include "Config.h"
#include "Commands.h"
#include "Communication.h"

#include "Motor.h"
#include "WebServer.h"

#include <ESP8266mDNS.h>
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

  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }  

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

    WiFiManager wifiManager;

    Serial.println("Connecting to WiFi...");
    WiFi.setHostname(WIFI_HOSTNAME);
    wifiManager.autoConnect(WIFI_AP_NAME);
    Serial.println("Success!");

    if (!MDNS.begin(WIFI_MDNS_HOSTNAME)) {
      Serial.println("Failed to create MDNS responder");
    }

    Serial.println("Initializing web server");
    WebServerInit();

    MDNS.addService("http", "tcp", 80);

    // Find all other devices
    enumerateModules();
    
    Serial.print("Found "); Serial.print(nKnownModules); Serial.println(" other I2C devices.");
  } else {
      Serial.print("Starting in slave mode at address "); Serial.println((int)Config.address);
      Wire.begin(Config.address);
      Wire.setClock(I2C_FREQUENCY);
      Wire.setClockStretchLimit(40000);
      Wire.onRequest(onRequestI2C);
  }

  motorInit();
  motorCalibrate();
}

void loop() {

  // Handle immediate events
  if (Serial.available()) {
    char commandBuff[128];
    commandBuff[Serial.readBytesUntil('\0', commandBuff, sizeof(commandBuff))] = 0;
    handleCommand(commandBuff);
  }

  char* i2cCommand = i2cReadCommand();

  if (i2cCommand && *i2cCommand) {
    Serial.println("Reveived I2C command...");
    handleCommand(i2cCommand);
  }

  if (i2cOverflow) {
    deviceLastStatus = MODULE_OVERFLOW;
    i2cOverflow = false;
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
  // ~immediate events

  // When in master mode, we run extra services; the http server, mDNS, time, etc.
  if (Config.isMaster) {
    MDNS.update();
    events(); // ezTime events
  }
}
