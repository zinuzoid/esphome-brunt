#pragma once
namespace esphome { namespace sensor {
class Sensor { public: float state{0}; void publish_state(float v) { state = v; } };
}}
#define LOG_SENSOR(prefix, type, obj) do {} while (0)
