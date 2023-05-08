#define MOTOR_IN1 D5
#define MOTOR_IN2 D6
#define MOTOR_IN3 D7
#define MOTOR_IN4 D8

#define HALL D3

#define I2C_FREQUENCY 100000

struct ModuleConfig {
  bool isMaster;
  char address; // i2c address
  char zeroOffset; // how many steps after hitting the hall effect marks the zero point (only positive)
};

extern ModuleConfig Config;