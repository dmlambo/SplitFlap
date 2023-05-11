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

// Helper to concatenate literal strings from defines
#define DEFTOSTR(x, ...) #x
#define DEFTOLIT(x, ...) DEFTOSTR(x)

struct ModuleConfig {
  bool isMaster;
  char address; // i2c address
  char zeroOffset; // how many steps after hitting the hall effect marks the zero point (only positive)
  char rpm;
};

extern ModuleConfig Config;