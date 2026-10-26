#pragma once
#include <optional>
namespace esphome { namespace cover {
enum CoverOperation { COVER_OPERATION_IDLE, COVER_OPERATION_OPENING, COVER_OPERATION_CLOSING };
class CoverTraits {
 public:
  void set_supports_position(bool) {}
  void set_supports_stop(bool) {}
  void set_is_assumed_state(bool) {}
};
class CoverCall {
 public:
  bool stop{false};
  std::optional<float> pos;
  bool get_stop() const { return stop; }
  const std::optional<float> &get_position() const { return pos; }
};
class Cover {
 public:
  virtual ~Cover() = default;
  float position{0};
  CoverOperation current_operation{COVER_OPERATION_IDLE};
  int publishes{0};
  void publish_state(bool = true) { publishes++; }
  virtual CoverTraits get_traits() = 0;
  void test_control(const CoverCall &c) { control(c); }
 protected:
  virtual void control(const CoverCall &call) = 0;
};
}}
#define LOG_COVER(prefix, type, obj) ESP_LOGCONFIG(TAG, "%s%s", prefix, type)
