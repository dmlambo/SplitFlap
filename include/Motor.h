#pragma once

void motorInit();
void motorSetRPM(int rpm);
void motorCalibrate();
void disableMotorTimer();
void enableMotorTimer();
void motorMoveToFlap(unsigned int flap);
void motorDebugPrint();

extern bool motorCalibrated;
extern bool motorStalled;