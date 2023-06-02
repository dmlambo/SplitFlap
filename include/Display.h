#pragma once

enum DisplayJustify {
  JustifyNone, // Each line consists of the number of characters there are, copied verbatim
  JustifyLeft,
  JustifyCenter,
  JustifyRight
};

void displayEvents();
void displayMessage(const char* message, unsigned int len, unsigned int seconds = 0, bool time = false, DisplayJustify justify = JustifyLeft);
void displaySetTimeZone(const char* timezone);