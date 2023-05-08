#include <Arduino.h>

#include "Config.h"

const bool PinStates[][4] = {
  { true, false, false, false },
  { true, true, false, false },
  { false, true, false, false },
  { true, true, true, false },
  { false, false, true, false },
  { false, false, true, true },
  { false, false, false, true },
  { true, false, false, true },
};

const char Pins[] = { MOTOR_IN1, MOTOR_IN2, MOTOR_IN3, MOTOR_IN4 };

static unsigned char motorState = 0;

// These are separated into two variables since the timer interrupt runs constantly, and we don't ever want to skip a step
// if we enable the motor too soon before the next interrupt
static bool motorEnabled = false;
static bool motorRunning = false;
static int motorStep = 0;
static int motorTarget = 0;
static char hallDebounce = 0;

bool motorCalibrated = false;

void doMotorISR() {
  if (motorEnabled) {
    if (motorRunning) {
      if (motorStep == motorTarget) {
        motorRunning = motorEnabled = false;
      } else {
        motorState++;
        motorState %= 8;
        motorStep++;
      }
    } else {
      motorRunning = true;
    }
    digitalWrite(Pins[0], PinStates[motorState][0]);
    digitalWrite(Pins[1], PinStates[motorState][1]);
    digitalWrite(Pins[2], PinStates[motorState][2]);
    digitalWrite(Pins[3], PinStates[motorState][3]);
  } else {
    motorRunning = false;
    digitalWrite(Pins[0], false);
    digitalWrite(Pins[1], false);
    digitalWrite(Pins[2], false);
    digitalWrite(Pins[3], false);
  }

  if (digitalRead(HALL)) {
    hallDebounce++;
  } else {
    hallDebounce--;
  }

  if (hallDebounce > 5) {
    hallDebounce = 5;
    // This implies that at startup, even if we're next to the hall sensor, we'll need to go around again.
    if (motorStep > 200) {
      motorStep = -Config.zeroOffset;
      motorCalibrated = true;
    }
  }
  if (hallDebounce < 0) {
    hallDebounce = 0;
  }
}

void motorCalibrate() {
  noInterrupts();
  motorStep = 1;
  motorTarget = 0;
  motorEnabled = true;
  motorCalibrated = false;
  interrupts();
}

void motorInit(float rpm) {
  float usecPerRev = (float)(1000000 * 60) / rpm;
  float usecPerStep = usecPerRev / (float)MOTOR_STEPS;
  int timerValue = CPU_CLK_FREQ / 16 / 1000000 * usecPerStep;

  timer1_disable();
  timer1_attachInterrupt(doMotorISR);
  timer1_write(timerValue);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
}