// Host test: BruntCover against a simulated STM32 + motor with the stock button semantics.
#include "brunt_cover.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace esphome {
bool g_verbose = false;
static uint32_t g_now = 0;
uint32_t millis() { return g_now; }
static std::multimap<uint32_t, std::function<void()>> g_queue;
void schedule(uint32_t at, std::function<void()> fn) { g_queue.emplace(at, std::move(fn)); }
}  // namespace esphome
using namespace esphome;

static uint8_t crc8(const uint8_t *d, size_t n) {
  uint8_t c = 0;
  while (n--) { c ^= *d++; for (int i = 0; i < 8; i++) c = (c & 1) ? (c >> 1) ^ 0x8C : c >> 1; }
  return c;
}

struct Motor {
  int32_t lim_close = 9866, lim_open = 22707;
  double pos = 13097;
  int motion = 0;          // 0 idle, 1 up, 2 down
  int presses = 0, starts = 0;
  bool drop_next_press = false;
  void press(int button) {  // registered 50 ms after the edge (debounce)
    presses++;
    if (drop_next_press) { drop_next_press = false; std::printf("        (sim: press lost)\n"); return; }
    if (motion == 0) {
      if ((button == 1 && pos >= lim_open) || (button == 2 && pos <= lim_close)) return;
      motion = button; starts++;
    } else {
      motion = 0;  // same or opposite button while moving: stop (opposite behaviour assumed)
      pos += (button == 1 ? 1 : -1) * 0;  // coast handled by lag below
    }
  }
  void step(double dt) {
    if (motion == 1) { pos += 968 * dt; if (pos >= lim_open) { pos = lim_open; motion = 0; } }
    if (motion == 2) { pos -= 935 * dt; if (pos <= lim_close) { pos = lim_close; motion = 0; } }
  }
  std::vector<uint8_t> status() const {
    uint8_t p[28] = {0};
    int32_t ip = (int32_t) std::lround(pos);
    p[0] = uint8_t((ip - lim_close) * 100 / (lim_open - lim_close));
    p[2] = uint8_t(motion);
    p[3] = 0x16; p[4] = 0x20;
    int load = motion ? 1000 : 5;
    p[5] = uint8_t(load >> 8); p[6] = uint8_t(load);
    auto put = [&](int i, int32_t v) { p[i] = v >> 24; p[i + 1] = v >> 16; p[i + 2] = v >> 8; p[i + 3] = v; };
    put(7, ip - lim_close);
    p[11] = uint8_t(motion);
    p[13] = 4;
    put(14, lim_close); put(18, lim_open); put(22, ip);
    std::vector<uint8_t> f = {0xA9, 0xC1, 28};
    f.insert(f.end(), p, p + 28);
    f.push_back(crc8(f.data(), f.size()));
    return f;
  }
};

static Motor g_motor;

class SimPin : public GPIOPin {
 public:
  explicit SimPin(int button) : button_(button) {}
  void digital_write(bool v) override {
    if (v && !level_) { int b = button_; schedule(millis() + 50, [b]() { g_motor.press(b); }); }
    level_ = v;
  }
 private:
  int button_;
  bool level_ = false;
};

class TestCover : public brunt::BruntCover {
 public:
  using brunt::BruntCover::target_;
  bool busy() const { return target_ >= 0 || stop_requested_ || awaiting_; }
};

static SimPin up_pin(1), down_pin(2);
static TestCover cover_;
static std::mt19937 rng(42);

static void run_ms(uint32_t ms) {
  static uint32_t next_status = 500;
  uint32_t end = g_now + ms;
  while (g_now < end) {
    g_now++;
    g_motor.step(0.001);
    while (!g_queue.empty() && g_queue.begin()->first <= g_now) {
      auto fn = g_queue.begin()->second; g_queue.erase(g_queue.begin()); fn();
    }
    if (g_now >= next_status) {
      auto f = g_motor.status();
      cover_.rx.insert(cover_.rx.end(), f.begin(), f.end());
      next_status = g_now + 495 + rng() % 12;  // real frames jitter 495-506 ms
    }
    if (g_now % 16 == 0) cover_.loop();
  }
}

static void cmd_pos(float t) { cover::CoverCall c; c.pos = t; cover_.test_control(c); }
static void cmd_stop() { cover::CoverCall c; c.stop = true; cover_.test_control(c); }
static double pct() { return (g_motor.pos - g_motor.lim_close) * 100.0 / (g_motor.lim_open - g_motor.lim_close); }

static int failures = 0;
static void check(bool ok, const char *what) {
  std::printf("  %s %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) failures++;
}

// Run until the component settles, then confirm nothing moves afterwards (no stray toggles).
static void settle(const char *name, double expect_pct, double tol) {
  int start_presses = g_motor.presses;
  for (int i = 0; i < 60 && (cover_.busy() || g_motor.motion); i++) run_ms(500);
  double settled = pct();
  run_ms(5000);
  std::printf("%s: %.1f%% (expected %.0f%%), %d presses\n", name, pct(), expect_pct, g_motor.presses - start_presses);
  check(std::fabs(pct() - expect_pct) <= tol, "final position");
  check(g_motor.motion == 0 && std::fabs(pct() - settled) < 0.01, "stays stopped");
  check(std::fabs(cover_.position * 100 - pct()) < 0.5, "reported position matches");
}

static void replay_log(const char *path) {
  std::ifstream in(path);
  std::string line;
  TestCover c;
  int frames = 0;
  while (std::getline(in, line)) {
    auto bar = line.find(']');
    if (bar == std::string::npos) continue;
    std::istringstream hs(line.substr(bar + 1, line.find("  |") - bar - 1));
    std::string h;
    while (hs >> h) c.rx.push_back(uint8_t(std::stoul(h, nullptr, 16)));
    c.rx.push_back(0x55); c.rx.push_back(0xA9);  // inject junk incl. a fake sync between frames
    c.loop();
    frames++;
  }
  std::printf("replayed %d frames from real log: final position %.3f, publishes %d\n", frames, c.position, c.publishes);
  check(std::fabs(c.position - 0.0f) < 1e-6 && c.publishes > 40, "real log parses through junk, ends at 0% (bottom)");
}

int main(int argc, char **argv) {
  if (argc > 2) g_verbose = true;
  if (argc > 1) replay_log(argv[1]);

  cover_.set_up_pin(&up_pin);
  cover_.set_down_pin(&down_pin);
  cover_.setup();
  run_ms(1200);
  std::printf("start at %.1f%%, reported %.1f%%\n", pct(), cover_.position * 100);

  cmd_pos(0.50f); settle("25 -> 50", 50, 2.5);
  cmd_pos(1.00f); settle("50 -> 100 (limit)", 100, 0.01);
  cmd_pos(0.00f); settle("100 -> 0 (limit)", 0, 0.01);
  cmd_pos(0.30f); settle("0 -> 30", 30, 2.5);
  cmd_pos(0.32f); settle("30 -> 32 (tiny)", 32, 2.5);
  cmd_pos(0.325f); settle("32 -> 32.5 (inside deadband)", 32, 2.5);

  cmd_pos(0.90f); run_ms(2000); cmd_stop(); settle("stop mid-move", 46, 6);
  cmd_pos(0.10f); run_ms(20); cmd_stop(); settle("stop while start press in flight", 46, 2);

  cmd_pos(0.00f); run_ms(2500); cmd_pos(0.80f); settle("reverse mid-move, down -> 80", 80, 2.5);

  cmd_pos(0.20f); run_ms(2000);
  g_motor.press(2);  // someone presses the physical DOWN button: stops the motor
  int before = g_motor.starts;
  settle("physical button stops our move", pct(), 0.01);
  check(g_motor.starts == before, "no restart after physical stop");

  g_motor.drop_next_press = true;
  cmd_pos(0.60f); settle("lost press is retried", 60, 2.5);

  std::printf("\n%s (%d failures)\n", failures ? "FAILED" : "ALL PASSED", failures);
  return failures ? 1 : 0;
}
