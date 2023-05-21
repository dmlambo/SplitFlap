#pragma once

#define MOTOR_IN1 D5
#define MOTOR_IN2 D6
#define MOTOR_IN3 D7
#define MOTOR_IN4 D8

#define MOTOR_STEPS 4076 // Half step
#define MOTOR_STALL_STEPS 200

#define HALL D0
#define HALL_DEBOUNCE 5

#define MOTOR_FLAPS 45

#define I2C_FREQUENCY 25000
#define I2C_BUFF_LEN 256
// As per protocol
#define I2C_DEVADDR_MIN 8
#define I2C_DEVADDR_MAX 120

#define WIFI_MDNS_HOSTNAME "splitflap"
#define WIFI_HOSTNAME "SplitFlapDisplay"
#define WIFI_AP_NAME "SplitFlapSetupAP"

#define DISPLAY_MAX_CHARS 128
#define DISPLAY_MAX_MODULES 32

// Helper to concatenate literal strings from defines
#define DEFTOSTR(x, ...) #x
#define DEFTOLIT(x, ...) DEFTOSTR(x)

#define CONFIG_MAGIC 0xFA7FEE75
#define CONFIG_TZSIZE 63

#define RTC_MAGIC 0xBEEFB015

// We save the current reset count and if it goes over RESET_WIFI_COUNT we clear the wifi creds
#define RESET_WIFI_COUNT 3
#define RESET_WIFI_SHORT_DELAY 50
#define RESET_WIFI_LONG_DELAY 1200

#ifdef DISABLE_LOGGING
#define LOGLN(...)
#define LOG(...)
#else
#define LOGLN(...) Serial.println(__VA_ARGS__)
#define LOG(...) Serial.print(__VA_ARGS__)
#endif

struct ModuleConfig {
  unsigned int magic; // Check for first-time configuration, or change of size of this struct
  bool isMaster;
  char address; // i2c address
  char zeroOffset; // how many steps after hitting the hall effect marks the zero point (only positive)
  char rpm;
  char timeZone[CONFIG_TZSIZE+1]; // plus null

  unsigned char charMap[256]; // Mapping of characters to flap numbers
};

extern ModuleConfig Config;

void printConfig();