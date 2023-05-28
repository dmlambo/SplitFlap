#pragma once

#include <Arduino.h>
#include <Stream.h>

class FlashStream : public Stream {
  unsigned int _addrBegin;
  unsigned int _size;
  unsigned int _curPos;

public:
  FlashStream(unsigned int addrBegin, unsigned int size) :
    _addrBegin(addrBegin), _size(size), _curPos(0) 
  {}

  using Print::write;

  int available() { return _size - _curPos; }

  int read() {  
    if (_curPos == _size) return -1;
    unsigned char data;
    if (!ESP.flashRead(_addrBegin + _curPos, &data, sizeof(data))) return -1;
    _curPos++;
    return (int)data;
  }

  int peek() { 
    if (_curPos == _size) return -1;
    unsigned char data;
    if (!ESP.flashRead(_addrBegin + _curPos, &data, sizeof(data))) return -1;
    return (int)data;
  }  

  size_t write(uint8_t data) {
    return 0;
  }

  virtual size_t readBytes(char *buffer, size_t length) {
    unsigned int size = length;
    if (_size - _curPos < length) size = _size - _curPos;
    if (size) {
      ESP.flashRead(_addrBegin + _curPos, (unsigned char*)buffer, size);
      _curPos += size; 
    }
    return size;
  }

  virtual ssize_t streamRemaining () { return _size - _curPos; }
};
