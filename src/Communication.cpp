#include <Arduino.h>
#include <Wire.h>
#include <CRC32.h>

#include "Config.h"

#include "Communication.h"
#include "Motor.h"

const char* StatusStr[] = {
  "OK",
  "MOVING",
  "STALLED",
  "CALIBRATING",
  "OVERFLOW",
  "UNAVAILABLE"
};

const char* UpdateStatusStr[] = {
  "OK",
  "CRC MISMATCH",
  "SIZE MISMATCH",
  "FS FULL",
  "UPDATE FAILED",
};

bool i2cOverflow = false;
Status deviceLastStatus = MODULE_OK;
unsigned char knownModules[DISPLAY_MAX_MODULES] = {0};
unsigned char nKnownModules = 0;
bool moduleContacted = 0;
bool moduleFinishedUpdate = 0;

void enumerateModules() {
  nKnownModules = 0;
  for (unsigned char i = I2C_DEVADDR_MIN; i <= I2C_DEVADDR_MAX && nKnownModules < DISPLAY_MAX_MODULES; i++) {
    ModuleStatus status;
    if (i2cReadStruct(i, &status) == PACKET_OK) {
      knownModules[nKnownModules++] = i;
    }
  }
}

char* i2cReadCommand() {
  static int buffLoc = 0;
  static char buff[I2C_BUFF_LEN] = { 0 };

  // We only read strings, and there may be, potentially, more than one string in the buffer.
  while (Wire.available()) {    
    buff[buffLoc] = Wire.read();
    if (buff[buffLoc] == 0 || buff[buffLoc] == '\n' || buff[buffLoc] == '\r') {
      buff[buffLoc] = 0; // Remove newline
      buffLoc = 0;
      return buff;
    } else {
      buffLoc++;
      if (buffLoc > I2C_BUFF_LEN - 1) {
        i2cOverflow = true;
        buffLoc = 0;
      }
    }
  }
  return NULL;
}

template<typename T> PacketStatus i2cReadStruct(unsigned char addr, T* dst, unsigned char retries) {
  PacketStatus status = PACKET_EMPTY;
  while (retries--) {
    while (Wire.available()) Wire.read();
    delay(1);
    unsigned char nRead = Wire.requestFrom((uint8_t)addr, (uint8_t)sizeof(ModulePacket<T>), (uint8_t)true);
    if (nRead == sizeof(ModulePacket<T>)) {
      unsigned int crc;
      if (Wire.readBytes((unsigned char*)&crc, sizeof(crc)) != sizeof(crc)) {
        //LOGLN("CRC Length");
        status = P_STATUS(status, PACKET_UNDERFLOW);
        continue;
      }
      unsigned char packetRead = Wire.readBytes((unsigned char*)dst, sizeof(T));
      if (packetRead != sizeof(T)) {
        //LOG("Packet size: "); LOG(sizeof(T)); LOG(" read: "); LOGLN(packetRead);
        status = P_STATUS(status, PACKET_UNDERFLOW);
        continue;
      }
      delay(1);
      unsigned int packetCrc = CRC32::calculate(dst, 1);
      if (packetCrc != crc) {
        //LOG("Packet CRC: "); LOG(packetCrc); LOG(" CRC: "); LOGLN(crc);
        status = P_STATUS(status, PACKET_CRC);
        continue;
      }
    } else {
      //LOG("Packet length: "); LOG(sizeof(ModulePacket<T>)); LOG(" read: "); LOGLN(nRead);
      if (!nRead) {
        status = P_STATUS(status, PACKET_EMPTY);
      } else if (nRead < sizeof(ModulePacket<T>)) {
        status = P_STATUS(status, PACKET_UNDERFLOW);
      } else {
        status = P_STATUS(status, PACKET_OVERFLOW);
      }
      continue;
    }
    return PACKET_OK;
  }
  return status;
}

template PacketStatus i2cReadStruct(unsigned char addr, ModuleStatus* dst, unsigned char retries);

// The only thing we return is our status
void onRequestI2C() {
  ModulePacket<ModuleStatus> packet;
  packet.data.flap = motorCurrentFlap();
  packet.data.status = deviceLastStatus;
  packet.data.version = VERSION;
  packet.data.zeroOffset = Config.zeroOffset;
  packet.crc = CRC32::calculate(&packet.data, 1);
  LOG("CRC: "); LOGLN(packet.crc);
  Wire.write((unsigned char*)&packet, sizeof(packet));
}

void onReceiveI2C(size_t size) {
  // Do nothing, we just need this here to coax the Wire library to work.
}