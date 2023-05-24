#pragma once

extern unsigned int rebootMillis;

unsigned int countResets();
void tcpCleanup (void);
bool argInRange(const char* arg, unsigned int min, unsigned int max, unsigned int* out);
bool argInRange(const char* arg, int min, int max, int* out);
void markForReboot(unsigned int delay);
bool shouldReboot();
void saveConfig();
