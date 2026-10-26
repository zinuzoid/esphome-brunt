#pragma once
#include <cstdint>
#include <deque>
namespace esphome { namespace uart {
class UARTDevice {
 public:
  std::deque<uint8_t> rx;
  int available() { return (int) rx.size(); }
  bool read_byte(uint8_t *b) { if (rx.empty()) return false; *b = rx.front(); rx.pop_front(); return true; }
};
}}
