#pragma once

void motorInit();
void motorSetRPM(int rpm);
void motorCalibrate();
void disableMotorTimer();
void enableMotorTimer();
void motorMoveToFlap(unsigned int flap);
void motorDebugPrint();

extern volatile bool motorCalibrated;
extern volatile bool motorStalled;