#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace brunt {

// Brunt Blind Engine, controlled without touching the original Wi-Fi module:
//  * listens to the STM32's 500 ms status frames on its USART3 TX (PB10, pin 21)
//  * moves the blind by pulsing the physical UP / DOWN buttons.
// Button semantics on the stock firmware: press = start, same button again = stop.
class BruntCover : public cover::Cover, public Component, public uart::UARTDevice {
 public:
  void set_up_pin(GPIOPin *pin) { this->up_pin_ = pin; }
  void set_down_pin(GPIOPin *pin) { this->down_pin_ = pin; }
  void set_press_duration(uint32_t ms) { this->press_ms_ = ms; }
  void set_stop_latency(uint32_t ms) { this->stop_latency_ms_ = ms; }
  void set_motor_load_sensor(sensor::Sensor *s) { this->load_sensor_ = s; }
  void set_encoder_sensor(sensor::Sensor *s) { this->encoder_sensor_ = s; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;

  void parse_buffer_();
  void handle_frame_(const uint8_t *frame);
  void handle_status_(const uint8_t *payload);
  void drive_();
  void press_(uint8_t button, uint8_t expect_motion);
  void finish_();
  bool status_fresh_() const;
  static uint8_t crc8_maxim_(const uint8_t *data, size_t len);

  GPIOPin *up_pin_{nullptr};
  GPIOPin *down_pin_{nullptr};
  sensor::Sensor *load_sensor_{nullptr};
  sensor::Sensor *encoder_sensor_{nullptr};
  uint32_t press_ms_{200};
  uint32_t stop_latency_ms_{100};

  // Frame assembly
  uint8_t buf_[64];
  size_t buf_len_{0};

  // Latest status (motion: 0 idle, 1 up/opening, 2 down/closing)
  bool have_status_{false};
  bool published_{false};
  bool uncalibrated_warned_{false};
  uint32_t last_status_ms_{0};
  uint8_t motion_{0};
  int32_t pos_{0};
  int32_t lim_close_{0};  // encoder count at the bottom (0 %)
  int32_t lim_open_{0};   // encoder count at the top (100 %)
  int32_t speed_{475};    // encoder counts per status frame while moving (measured ~465-485)
  int32_t last_load_{-1};
  int32_t last_encoder_{INT32_MIN};

  // Control
  float target_{-1.0f};        // < 0: nothing to do
  bool stop_requested_{false};  // user asked to stop, possibly while a start press is in flight
  bool stop_sent_{false};       // we pressed stop because the target was reached
  uint8_t started_dir_{0};      // direction we started for the current target
  uint8_t attempts_{0};
  bool awaiting_{false};        // a press is in flight; never press again until it shows up
  uint8_t expect_{0};
  uint32_t await_deadline_{0};
};

}  // namespace brunt
}  // namespace esphome
