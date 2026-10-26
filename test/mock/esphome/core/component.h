#pragma once
#include <functional>
#include <string>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
namespace esphome {
namespace setup_priority { const float DATA = 600.0f; }
void schedule(uint32_t at, std::function<void()> fn);
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  virtual float get_setup_priority() const { return 0; }
 protected:
  void set_timeout(const std::string &, uint32_t ms, std::function<void()> fn) { schedule(millis() + ms, fn); }
};
}  // namespace esphome
