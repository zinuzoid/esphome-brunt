#pragma once
#include <cstdint>
#include <string>
namespace esphome {
uint32_t millis();
class GPIOPin {
 public:
  virtual ~GPIOPin() = default;
  virtual void setup() {}
  virtual void digital_write(bool v) = 0;
  virtual std::string dump_summary() const { return "mock"; }
};
}  // namespace esphome
#define LOG_PIN(prefix, pin) if ((pin) != nullptr) { ESP_LOGCONFIG(TAG, prefix "%s", (pin)->dump_summary().c_str()); }
