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

template<class T>
class BlockingStream : public T {
private:
  unsigned int _timeout = 1000; // millis

public:
  using T::T;

  void setWriteTimeout(unsigned int timeout) {
    _timeout = timeout;
  }

  size_t write(uint8_t data) {
    size_t written = T::write(data);
    for (unsigned long startMillis = millis(); !written && millis() - startMillis < _timeout; yield()) {
      written = T::write(data);
    }
    return written;
  }

  size_t write(const char *buffer, size_t size) {
    size_t written = T::write(buffer, size);
    for (unsigned long startMillis = millis(); written < size && millis() - startMillis < _timeout; yield()) {
      written += T::write(buffer+written, size-written);
    }
    return written;
  }

  size_t write(const uint8_t *buffer, size_t size) {
    size_t written = T::write(buffer, size);
    for (unsigned long startMillis = millis(); written < size && millis() - startMillis < _timeout; yield()) {
      written += T::write(buffer+written, size-written);
    }
    return written;
  }

  using T::write;
};