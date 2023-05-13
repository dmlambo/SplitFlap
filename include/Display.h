#pragma once

extern char currentDisplayText[];
extern char ephemeralDisplayText[];
extern char persistentDisplayText[];

void displayEvents();
void displayMessage(const char* message, unsigned int len, unsigned int seconds = 0, bool time = false);
void displaySetTimeZone(const char* timezone);