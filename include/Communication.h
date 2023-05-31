#pragma once

#include <Updater.h>

#include "Config.h"

#define DATA_PACKET_SIZE 32

enum Status { 
  MODULE_OK,
  MODULE_MOVING,
  MODULE_STALLED,
  MODULE_CALIBRATING,
  MODULE_OVERFLOW, // Likely communication problem
  MODULE_UNAVAILABLE,
};

enum PacketStatus {
  PACKET_OK = 0,

  // Non-OK statuses in ascending priority, ORed together
  PACKET_EMPTY = 1, 
  PACKET_UNDERFLOW = 3,
  PACKET_OVERFLOW = 7,
  PACKET_CRC = 15,
};

#define P_STATUS(prevStatus, newStatus) (PacketStatus)(prevStatus | newStatus)

#pragma pack(push, 1)

// Note that if the size of this changes, all CRCs will fail
struct ModuleStatus {
  unsigned int version;
  Status status;
  unsigned char flap;
  unsigned char zeroOffset;
};

template<typename T> struct ModulePacket {
  unsigned int crc;
  T data;
};

#pragma pack(pop)

extern const char* StatusStr[];
extern const char* UpdateStatusStr[];
extern bool i2cOverflow;
extern Status deviceLastStatus;
extern unsigned char knownModules[];
extern unsigned char nKnownModules;
extern bool moduleContacted;
extern bool moduleFinishedUpdate;

void enumerateModules();
void onRequestI2C();
void onReceiveI2C(size_t size);
char* i2cReadCommand();
template<typename T> PacketStatus i2cReadStruct(unsigned char addr, T* dst, unsigned char retries = I2C_RETRIES);