#include <Arduino.h>

#include "Config.h"

#include "Communication.h"
#include "Motor.h"

const bool PinStates[][4] = {
  { true, false, false, false },
  { false, false, false, true },
  { false, false, true, false },
  { false, true, false, false },
};

const char Pins[] = { MOTOR_IN1, MOTOR_IN2, MOTOR_IN3, MOTOR_IN4 };

static unsigned char motorState = 0;

// These are separated into two variables since the timer interrupt runs constantly, and we don't ever want to skip a step
// if we enable the motor too soon before the next interrupt, by, for instance, setting another target mid-turn
static volatile bool motorEnabled = false;
static volatile bool motorRunning = false;
static volatile int motorStep = -MOTOR_STEPS; // This gives it a chance to calibrate on start
static volatile int motorTarget = 0;
static volatile unsigned int motorHold = MOTOR_HOLD;
static volatile int hallDebounce = 0;
static volatile int waitForLow = false;
static volatile unsigned int currentFlap = MOTOR_FLAPS; // Start out of range

volatile bool motorCalibrated = false;
volatile bool motorStalled = false;

void IRAM_ATTR doMotorISR(void *para, void *frame) {
  if ((T1C & ((1 << TCAR) | (1 << TCIT))) == 0) TEIE &= ~TEIE1;//edge int disable
  T1I = 0;
  if (motorEnabled) {
    if (deviceLastStatus != MODULE_CALIBRATING) deviceLastStatus = MODULE_MOVING;
    if (motorRunning) {
      // Since we subtract the zero offset, the last flap may actually be in between the zero point and true zero, where motorStep is negative
      // Furthermore, there's no difference between a target of 0 and calibration, and we calibrate every time we pass the hall
      if ((motorTarget == 0 && motorStep == 0) || 
          (motorTarget != 0 && (motorStep == motorTarget || motorStep + MOTOR_STEPS == motorTarget))) {
        motorRunning = motorEnabled = false;
        deviceLastStatus = MODULE_OK;
      } else {
        motorState++;
        motorState %= 4;
        motorStep++;

        if (motorStep >= MOTOR_STEPS + MOTOR_STALL_STEPS) { // Done a whole rotation without seeing the hall sensor. We've stalled.
          deviceLastStatus = MODULE_STALLED;
          motorStalled = true;
          motorRunning = motorEnabled = false;
          motorHold = 0;
          motorStep = 0;
        }
      }
    } else {
      // Advance the next time around
      motorRunning = true;
      motorHold = MOTOR_HOLD;
    }
    digitalWrite(Pins[0], PinStates[motorState][0]);
    digitalWrite(Pins[1], PinStates[motorState][1]);
    digitalWrite(Pins[2], PinStates[motorState][2]);
    digitalWrite(Pins[3], PinStates[motorState][3]);
  } else {
    if (motorHold) {
      motorHold--;
    } else {
      digitalWrite(Pins[0], false);
      digitalWrite(Pins[1], false);
      digitalWrite(Pins[2], false);
      digitalWrite(Pins[3], false);
      disableMotorTimer();
    }
  }

  if (!digitalRead(HALL)) {
    hallDebounce++;
  } else {
    hallDebounce--;
  }

  if (hallDebounce > HALL_DEBOUNCE) {
    hallDebounce = HALL_DEBOUNCE;
    // At startup, even if we're next to the hall sensor, we'll need to go around again
    if (!waitForLow) {
      motorStep = -Config.zeroOffset;
      motorCalibrated = true;
      waitForLow = true;
    }
  }
  if (hallDebounce < 0) {
    waitForLow = false;
    hallDebounce = 0;
  }
}

void motorCalibrate() {
  LOGLN("Calibrating...");
  deviceLastStatus = MODULE_CALIBRATING;

  noInterrupts();
  motorStep = 1;
  motorTarget = 0;
  motorEnabled = true;
  motorCalibrated = false;
  waitForLow = !digitalRead(HALL);
  hallDebounce = waitForLow ? HALL_DEBOUNCE : 0;
  interrupts();
  enableMotorTimer();
}

void motorSetRPM(int rpm) {
  float usecPerRev = (float)(1000000 * 60) / (float)rpm;
  float usecPerStep = usecPerRev / (float)MOTOR_STEPS;
  int timerValue = CPU_CLK_FREQ / 16 / 1000000 * usecPerStep;

  timer1_write(timerValue);
}

void motorMoveToFlap(unsigned int flap) {
  // Require a calibrate after a stall
  if (deviceLastStatus == MODULE_STALLED) return;

  // No work to be done
  if (flap == currentFlap) return;

  if (flap >= MOTOR_FLAPS) {
    flap = 0;
  }

  currentFlap = flap;

  noInterrupts();
  motorTarget = (int)((float)flap * ((float)MOTOR_STEPS / (float)MOTOR_FLAPS));
  motorEnabled = true;
  interrupts();
  enableMotorTimer();
}

unsigned int motorCurrentFlap() {
  return currentFlap;
}

void motorInit() {
  disableMotorTimer();
  // Subvert the Arduino core ISR, since it disables interrupts, which messes up flash.
  //timer1_attachInterrupt(doMotorISR); <- no!
  ETS_FRC_TIMER1_INTR_ATTACH(doMotorISR, NULL);
  ETS_FRC1_INTR_ENABLE();
  motorSetRPM(Config.rpm);
  //enableMotorTimer();
}

void disableMotorTimer() {
  timer1_disable();
}

void enableMotorTimer() {
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
}

void motorDebugPrint() {
  static int i = 0;
  if (!(i++ % 1000)) {
    LOGLN(motorStep);
  }
}