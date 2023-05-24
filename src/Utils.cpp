#include <Arduino.h>

#include <EEPROM.h>

#include "Config.h"

#include <lwip/tcp.h>

unsigned int rebootMillis = 0;
unsigned int rebootDelay = 0;

unsigned int countResets() {
  // Add a breif delay since on hard boot the reset pin may go a little wild.
  delay(RESET_COUNT_SHORT_DELAY);

  unsigned int rtcMagic = 0;
  unsigned int resetCount = 0;
  unsigned int prevResets = 0;
  if (system_get_rst_info()->reason == REASON_EXT_SYS_RST) {
    ESP.rtcUserMemoryRead(0, &rtcMagic, sizeof(rtcMagic));
    if (rtcMagic == RTC_MAGIC) {
      ESP.rtcUserMemoryRead(sizeof(rtcMagic), &resetCount, sizeof(resetCount));
    } else {
      rtcMagic = RTC_MAGIC;
      ESP.rtcUserMemoryWrite(0, &rtcMagic, sizeof(RTC_MAGIC));
    }
    resetCount++;
    ESP.rtcUserMemoryWrite(sizeof(rtcMagic), &resetCount, sizeof(resetCount));
  }

  delay(RESET_COUNT_LONG_DELAY);

  prevResets = resetCount;
  resetCount = 0;
  ESP.rtcUserMemoryWrite(sizeof(rtcMagic), &resetCount, sizeof(resetCount));

  return prevResets;
}

// There's a bug in this version of lwip/NONOS, where when the network interface (netif) is setup
// in AP, its IP is any, and when a TCP connection tries to survive to STA, the netif changing IPs
// doesn't cause the right sequence of events to close ESTABLISHED and LISTENING pcbs.
// Therefore we just plough over the active and waiting pcbs before we start the web server.
extern struct tcp_pcb* tcp_tw_pcbs;
extern struct tcp_pcb* tcp_active_pcbs;
extern "C" void tcp_abandon(struct tcp_pcb *pcb, int reset);

void tcpCleanup (void) {
  int maxRetries = 50;
  while (tcp_tw_pcbs && maxRetries--) {
    LOG("Aborting waiting PCB "); LOGLN((int)tcp_tw_pcbs, 16);
    tcp_abort(tcp_tw_pcbs);
  }
  while (tcp_active_pcbs && maxRetries--) {
    LOG("Abandoning active PCB "); LOGLN((int)tcp_active_pcbs, 16);
    tcp_abandon(tcp_active_pcbs, 0);
  }
}

bool argInRange(const char* arg, unsigned int min, unsigned int max, unsigned int* out) {
  char* end;
  unsigned long value = strtoul(arg, &end, 10);

  if (value > max || value < min || *end) {
    return false;
  }

  *out = value;
  return true;
}

bool argInRange(const char* arg, int min, int max, int* out) {
  char* end;
  long value = strtol(arg, &end, 10);

  if (value > max || value < min || *end) {
    return false;
  }

  *out = value;
  return true;
}

void markForReboot(unsigned int delay) {
  rebootMillis = millis();
  rebootDelay = delay;
}

bool shouldReboot() {
  return rebootDelay && millis() - rebootMillis > rebootDelay;
}

void saveConfig() {
  LOGLN("Writing config...");
  memcpy(EEPROM.getDataPtr(), &Config, sizeof(ModuleConfig));
  EEPROM.commit();
}