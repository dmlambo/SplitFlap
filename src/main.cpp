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
#include "Utils.h"

#include <ESP8266mDNS.h>
#include <flash_hal.h>

#include <lwip/tcp.h>

// Default config
ModuleConfig Config = { 
  .magic = CONFIG_MAGIC + sizeof(ModuleConfig), 
  .isMaster = 0, 
  .address = 0, 
  .zeroOffset = 0,
  .rpm = 15, 
  .timeZone = { 0 },
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

  // It's somewhat important where this goes, since we rely on reset timing to determine whether to forget WiFi or become the master
  unsigned int resetCount = countResets();

  Serial.begin(57600);

  while (!Serial) { yield(); }

  LOGLN();
  LOGLN("Welcome to Dave's Split Flap Experience!");

  // Debug.. 
  delay(2000); 

  if(!LittleFS.begin()){
    LOGLN("An Error has occurred while mounting LittleFS");
    return;
  }

  EEPROM.begin(sizeof(ModuleConfig));

  // Valid saved config
  if (((unsigned int *)EEPROM.getConstDataPtr())[0] == CONFIG_MAGIC + sizeof(ModuleConfig)) {
    firstBoot = false;
    memcpy(&Config, EEPROM.getConstDataPtr(), sizeof(ModuleConfig));

    LOGLN("Active configuration loaded from flash");
  } else {
    // First bootup
    LOGLN("First time boot or config size mismatch, using defaults");
  }

  printConfig(&Serial);

  LOGLN();

  printCommandHelp(&Serial);

  // On first flash, we start in slave mode, but as a master I2C device. We wait a random amount of time
  // and try to assign our random address by first requesting data from said address. If no one responds
  // we keep it, if someone responds we move onto another address. Once we find an empty address, we 
  // write the config and reboot. However, we need to give other modules a time to do this entire
  // song and dance, so that we ensure we take control of the bus at some point (there's no arbitration)
  // and give others a chance to assign their own address and reboot into slave mode.
  if (firstBoot) {
    LOG("Searching for unused I2C address between " DEFTOLIT(I2C_DEVADDR_MIN+DISPLAY_MAX_MODULES) " and " DEFTOLIT(I2C_DEVADDR_MAX));

    Wire.begin();

    Config.address = ESP8266TrueRandom.random(I2C_DEVADDR_MIN+DISPLAY_MAX_MODULES, I2C_DEVADDR_MAX);

    bool addrFound = false;
    int attempts = 100;

    while (!addrFound && attempts--) { 
      ModuleStatus status;
      delayMicroseconds(ESP8266TrueRandom.random(0, 40000));
      if (i2cReadStruct(Config.address, &status) == PACKET_OK) {
        LOG("Found module at address "); LOGLN((int)Config.address);
        attempts = 100;
        Config.address = ESP8266TrueRandom.random(I2C_DEVADDR_MIN+32, I2C_DEVADDR_MAX);
      }
    }

    LOG("Found unused address "); LOGLN((int)Config.address);
    saveConfig();
    LOGLN("Rebooting...");
    Serial.flush();
    ESP.reset();
  }

  LOG("Reset count: "); LOGLN(resetCount);
  if (resetCount > 1) {
    if (resetCount >= RESET_BECOME_MASTER) {
      LOGLN("Reset " DEFTOLIT(RESET_BECOME_MASTER) " times, becoming master");
      Config.isMaster = true;
      saveConfig();
    }
  }

  if (Config.isMaster) {
    LOGLN("Starting in master mode.");

    Wire.begin();
    Wire.setClock(I2C_FREQUENCY);
    Wire.setClockStretchLimit(40000);

    {
      WiFiManager wifiManager;

      if (resetCount >= RESET_WIFI_COUNT) { 
        LOGLN("WiFi credentials reset due to external button sequence");
        wifiManager.resetSettings();
      }

      LOGLN("Connecting to WiFi...");
      WiFi.setHostname(WIFI_HOSTNAME);
      while (!wifiManager.autoConnect(WIFI_AP_NAME)) {
        LOGLN("Failed to connect to network. Retrying.");
      }
      LOGLN("Success!");
    }

    delay(1000);

    tcpCleanup();

    // Not sure if needed, but we should probably wait for lwip's fast and slow timer procs to run (250/500ms)
    delay(1000);;

    if (!MDNS.begin(WIFI_MDNS_HOSTNAME)) {
      LOGLN("Failed to create MDNS responder");
    }

    LOGLN("Initializing web server");
    WebServerInit();

    MDNS.addService("http", "tcp", 80);

    // Find all other devices
    enumerateModules();
    
    LOG("Found "); LOG(nKnownModules); LOGLN(" other I2C devices.");
  } else {
      LOG("Starting in slave mode at address "); LOGLN((int)Config.address);
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
    handleCommand(commandBuff, &Serial);
  }

  char* i2cCommand = i2cReadCommand();

  if (i2cCommand && *i2cCommand) {
    //LOGLN("Reveived I2C command...");
    handleCommand(i2cCommand, &Serial);
  }

  if (i2cOverflow) {
    deviceLastStatus = MODULE_OVERFLOW;
    i2cOverflow = false;
    LOGLN("I2C command buffer overflow.");
  }

  if (motorCalibrated) {
    motorCalibrated = false;
    LOGLN("Motor is calibrated.");
  }

  if (motorStalled) {
    motorStalled = false;
    LOGLN("Motor stalled.");
  }
  // ~immediate events

  // When in master mode, we run extra services; the http server, mDNS, time, etc.
  if (Config.isMaster) {
    MDNS.update();
    displayEvents();
  }

  if (shouldReboot()) {
    ESP.restart();
  }
}
