#include <Arduino.h>

#include <Wire.h>
#include <EEPROM.h>
#include <LittleFS.h>

//#include <ESPAsyncWebServer.h>
#include <WiFiManager.h>
#include <ESP8266TrueRandom.h>

#include "Config.h"
#include "Commands.h"
#include "Communication.h"
#include "Display.h"

#include "Motor.h"
#include "WebServer.h"

#include <ESP8266mDNS.h>
#include <flash_hal.h>

// Default config
ModuleConfig Config = { 
  .magic = CONFIG_MAGIC + sizeof(ModuleConfig), 
  .isMaster = 0, 
  .address = 0, 
  .zeroOffset = 0,
  .rpm = 15, 
  .timeZone = { 0 }, // Use whichever NTP decides for us
  #include "DefaultCharMap.inc" 
};

void setup() {
  bool firstBoot = true; 

  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_IN3, OUTPUT);
  pinMode(MOTOR_IN4, OUTPUT);

  digitalWrite(MOTOR_IN1, 0);
  digitalWrite(MOTOR_IN2, 0);
  digitalWrite(MOTOR_IN3, 0);
  digitalWrite(MOTOR_IN4, 0);

  // D0 *DOES NOT* have a pullup.
  pinMode(HALL, INPUT_PULLUP);

  Serial.begin(57600);

  while (!Serial) { yield(); }

  Serial.println();
  Serial.println("Welcome to Dave's Split Flap Experience!");

  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }  

  EEPROM.begin(sizeof(ModuleConfig));

  // Valid saved config
  if (((unsigned int *)EEPROM.getConstDataPtr())[0] == CONFIG_MAGIC + sizeof(ModuleConfig)) {
    firstBoot = false;
    memcpy(&Config, EEPROM.getConstDataPtr(), sizeof(ModuleConfig));

    Serial.println("Active configuration loaded from flash:");
  } else {
    // First bootup
    Serial.println("First time boot or config size mismatch, using defaults:");
  }

  printConfig();

  Serial.println();

  printCommandHelp();

  // On first flash, we start in slave mode, but as a master I2C device. We wait a random amount of time
  // and try to assign our random address by first requesting data from said address. If no one responds
  // we keep it, if someone responds we move onto another address. Once we find an empty address, we 
  // write the config and reboot. However, we need to give other modules a time to do this entire
  // song and dance, so that we ensure we take control of the bus at some point (there's no arbitration)
  // and give others a chance to assign their own address and reboot into slave mode.
  if (firstBoot) {
    Serial.print("Searching for unused I2C address between " DEFTOLIT(I2C_DEVADDR_MIN+DISPLAY_MAX_MODULES) " and " DEFTOLIT(I2C_DEVADDR_MAX));

    Wire.begin();

    Config.address = ESP8266TrueRandom.random(I2C_DEVADDR_MIN+DISPLAY_MAX_MODULES, I2C_DEVADDR_MAX);

    bool addrFound = false;
    int attempts = 100;

    while (!addrFound && attempts--) { 
      delayMicroseconds(ESP8266TrueRandom.random(0, 40000));
      unsigned char bytesRead = Wire.requestFrom((int)Config.address, (int)1);
      if (bytesRead) {
        Serial.print("Found module at address "); Serial.println((int)Config.address);
        attempts = 100;
        Config.address = ESP8266TrueRandom.random(I2C_DEVADDR_MIN+32, I2C_DEVADDR_MAX);
      }
    }

    Serial.print("Found unused address "); Serial.println((int)Config.address);
    Serial.println("Writing config...");
    memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
    EEPROM.commit();
    Serial.println("Rebooting...");
    Serial.flush();
    ESP.reset();
  }

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
      Wire.onReceive(onReceiveI2C);
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
    displayEvents();
  }
}
