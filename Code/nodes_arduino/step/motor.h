#pragma once
#include <Arduino.h>

enum MotorDir { DIR_OPEN, DIR_CLOSE };

void motor_init();
void motor_start(MotorDir dir);
void motor_stop();
bool motor_running();
