#include "brunt_cover.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace esphome {
namespace brunt {

static const char *const TAG = "brunt";

static const uint8_t SYNC = 0xA9;
static const uint8_t TYPE_STATUS = 0xC1;
static const uint8_t STATUS_LEN = 28;
static const uint32_t STATUS_PERIOD_MS = 500;
static const uint32_t STATUS_STALE_MS = 2000;
static const uint32_t PRESS_TIMEOUT_MS = 1300;  // ~2.5 status frames for a press to show up
static const uint8_t MAX_ATTEMPTS = 3;
static const float DEADBAND = 0.01f;

static int32_t be32(const uint8_t *p) {
  return static_cast<int32_t>((uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]);
}

static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

uint8_t BruntCover::crc8_maxim_(const uint8_t *data, size_t len) {
  uint8_t crc = 0;
  while (len--) {
    crc ^= *data++;
    for (int i = 0; i < 8; i++)
      crc = (crc & 1) ? (crc >> 1) ^ 0x8C : crc >> 1;
  }
  return crc;
}

void BruntCover::setup() {
  this->up_pin_->setup();
  this->up_pin_->digital_write(false);
  this->down_pin_->setup();
  this->down_pin_->digital_write(false);
}

cover::CoverTraits BruntCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_stop(true);
  traits.set_is_assumed_state(false);
  return traits;
}

void BruntCover::dump_config() {
  LOG_COVER("", "Brunt Blind Engine", this);
  LOG_PIN("  Up button pin: ", this->up_pin_);
  LOG_PIN("  Down button pin: ", this->down_pin_);
  ESP_LOGCONFIG(TAG, "  Press duration: %u ms, stop latency: %u ms", (unsigned) this->press_ms_,
                (unsigned) this->stop_latency_ms_);
  LOG_SENSOR("  ", "Motor load", this->load_sensor_);
  LOG_SENSOR("  ", "Encoder", this->encoder_sensor_);
}

void BruntCover::loop() {
  uint8_t byte;
  while (this->available() && this->read_byte(&byte)) {
    if (this->buf_len_ == 0 && byte != SYNC)
      continue;
    this->buf_[this->buf_len_++] = byte;
    this->parse_buffer_();
  }
}

// Consume complete frames from buf_. On a bad length or CRC we may have locked onto a stray A9
// (noise, or an A9 inside a payload), so drop one byte and rescan from the next A9 we already hold.
void BruntCover::parse_buffer_() {
  while (this->buf_len_ >= 3) {
    const size_t need = 3 + size_t(this->buf_[2]) + 1;
    bool bad = this->buf_[0] != SYNC || need > sizeof(this->buf_);
    if (!bad) {
      if (this->buf_len_ < need)
        return;
      if (crc8_maxim_(this->buf_, need - 1) == this->buf_[need - 1]) {
        this->handle_frame_(this->buf_);
        this->buf_len_ -= need;
        memmove(this->buf_, this->buf_ + need, this->buf_len_);
        continue;
      }
      ESP_LOGV(TAG, "Bad CRC on type %02X", this->buf_[1]);
    }
    const uint8_t *next = static_cast<const uint8_t *>(memchr(this->buf_ + 1, SYNC, this->buf_len_ - 1));
    if (next == nullptr) {
      this->buf_len_ = 0;
      return;
    }
    this->buf_len_ -= next - this->buf_;
    memmove(this->buf_, next, this->buf_len_);
  }
}

void BruntCover::handle_frame_(const uint8_t *frame) {
  if (frame[1] == TYPE_STATUS && frame[2] == STATUS_LEN) {
    this->handle_status_(frame + 3);
  } else {
    ESP_LOGD(TAG, "Frame type %02X, %u payload bytes", frame[1], (unsigned) frame[2]);
  }
}

void BruntCover::handle_status_(const uint8_t *p) {
  const uint8_t motion = p[2] <= 2 ? p[2] : 0;
  const int32_t lim_close = be32(p + 14);
  const int32_t lim_open = be32(p + 18);
  const int32_t pos = be32(p + 22);

  if (lim_open <= lim_close) {
    if (!this->uncalibrated_warned_) {
      ESP_LOGW(TAG, "Travel limits not set (%d..%d); calibrate with the physical buttons", (int) lim_close,
               (int) lim_open);
    }
    this->uncalibrated_warned_ = true;
    return;
  }

  // Learn speed from two consecutive frames moving the same way.
  if (this->have_status_ && motion != 0 && motion == this->motion_) {
    const int32_t d = std::abs(pos - this->pos_);
    if (d > 100)
      this->speed_ = d;
  }

  const uint8_t was = this->motion_;
  this->motion_ = motion;
  this->pos_ = pos;
  this->lim_close_ = lim_close;
  this->lim_open_ = lim_open;
  this->have_status_ = true;
  this->last_status_ms_ = millis();

  const float position = clamp01(float(pos - lim_close) / float(lim_open - lim_close));
  const auto op = motion == 1   ? cover::COVER_OPERATION_OPENING
                  : motion == 2 ? cover::COVER_OPERATION_CLOSING
                                : cover::COVER_OPERATION_IDLE;
  if (!this->published_ || std::fabs(position - this->position) > 0.0005f || op != this->current_operation) {
    this->position = position;
    this->current_operation = op;
    this->publish_state(op == cover::COVER_OPERATION_IDLE);  // only persist when settled
    this->published_ = true;
  }

  const int32_t load = (int32_t(p[5]) << 8) | p[6];
  if (this->load_sensor_ != nullptr && load != this->last_load_) {
    this->last_load_ = load;
    this->load_sensor_->publish_state(load);
  }
  if (this->encoder_sensor_ != nullptr && pos != this->last_encoder_) {
    this->last_encoder_ = pos;
    this->encoder_sensor_->publish_state(pos);
  }

  // Motor stopped and we didn't ask it to: a limit was reached or someone used the physical buttons.
  if (this->target_ >= 0.0f && was != 0 && motion == 0 && !this->stop_sent_ && !(this->awaiting_ && this->expect_ == 0)) {
    ESP_LOGI(TAG, "Stopped at %.1f%% (limit or physical button)", position * 100.0f);
    this->finish_();
  }

  this->drive_();
}

bool BruntCover::status_fresh_() const {
  return this->have_status_ && (millis() - this->last_status_ms_) < STATUS_STALE_MS;
}

void BruntCover::control(const cover::CoverCall &call) {
  if (!this->status_fresh_()) {
    ESP_LOGW(TAG, "No recent status from the STM32; ignoring command");
    return;
  }
  if (call.get_stop()) {
    this->finish_();
    this->stop_requested_ = true;
    this->drive_();
    return;
  }
  if (call.get_position().has_value()) {
    this->finish_();
    this->stop_requested_ = false;
    this->target_ = clamp01(*call.get_position());
    this->drive_();
  }
}

void BruntCover::finish_() {
  this->target_ = -1.0f;
  this->stop_sent_ = false;
  this->started_dir_ = 0;
  this->attempts_ = 0;
}

void BruntCover::press_(uint8_t button, uint8_t expect_motion) {
  GPIOPin *pin = button == 1 ? this->up_pin_ : this->down_pin_;
  ESP_LOGD(TAG, "Pressing %s (expect motion %u)", button == 1 ? "UP" : "DOWN", expect_motion);
  pin->digital_write(true);
  this->set_timeout(button == 1 ? "up_release" : "down_release", this->press_ms_,
                    [pin]() { pin->digital_write(false); });
  this->awaiting_ = true;
  this->expect_ = expect_motion;
  this->await_deadline_ = millis() + PRESS_TIMEOUT_MS;
}

void BruntCover::drive_() {
  // A second press of the same button toggles the motor, so never press again until the previous
  // press is visible in the status (or clearly got lost).
  if (this->awaiting_) {
    if (this->motion_ == this->expect_) {
      this->awaiting_ = false;
    } else if (int32_t(millis() - this->await_deadline_) < 0) {
      return;
    } else {
      ESP_LOGW(TAG, "Button press had no visible effect");
      this->awaiting_ = false;
    }
  }

  if (this->stop_requested_) {
    if (this->motion_ != 0) {
      this->press_(this->motion_, 0);
    } else {
      this->stop_requested_ = false;
    }
    return;
  }

  if (this->target_ < 0.0f)
    return;

  const float cur = this->position;
  const bool to_limit = this->target_ <= 0.0f || this->target_ >= 1.0f;
  uint8_t want;
  if (to_limit) {
    want = this->target_ >= 1.0f ? 1 : 2;
    const bool at_limit = want == 1 ? this->pos_ >= this->lim_open_ : this->pos_ <= this->lim_close_;
    if (this->motion_ == 0 && at_limit) {
      this->finish_();
      return;
    }
  } else {
    const float err = this->target_ - cur;
    if (this->motion_ == 0 && (this->stop_sent_ || std::fabs(err) <= DEADBAND)) {
      ESP_LOGD(TAG, "Target %.1f%% reached at %.1f%%", this->target_ * 100.0f, cur * 100.0f);
      this->finish_();
      return;
    }
    want = err > 0 ? 1 : 2;
  }

  if (this->motion_ == 0) {
    if (this->attempts_ >= MAX_ATTEMPTS) {
      ESP_LOGW(TAG, "Giving up after %u presses", this->attempts_);
      this->finish_();
      return;
    }
    this->attempts_++;
    this->started_dir_ = want;
    this->press_(want, want);
    return;
  }

  if (this->motion_ != want) {
    // Moving the wrong way. If it's the way we started, we overshot the target: stopping finishes the
    // move. Otherwise it was already moving the other way: stop it, then start towards the target.
    if (this->motion_ == this->started_dir_)
      this->stop_sent_ = true;
    this->press_(this->motion_, 0);
    return;
  }

  if (!to_limit) {
    // Stop on this frame if the next one would land further from the target than now.
    const float remaining = std::fabs(this->target_ - cur) * float(this->lim_open_ - this->lim_close_);
    const float lead = float(this->speed_) * (0.5f + float(this->stop_latency_ms_) / float(STATUS_PERIOD_MS));
    if (remaining <= lead) {
      this->stop_sent_ = true;
      this->press_(this->motion_, 0);
    }
  }
  // Moving towards a limit: the firmware stops there by itself.
}

}  // namespace brunt
}  // namespace esphome
