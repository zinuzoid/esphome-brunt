#pragma once
#include <cstdio>
namespace esphome { extern bool g_verbose; uint32_t millis(); }
#define ESP_LOG_(lvl, tag, fmt, ...) do { if (esphome::g_verbose || lvl[0]=='W') std::printf("%7.3f [%s] " fmt "\n", esphome::millis()/1000.0, lvl, ##__VA_ARGS__); } while (0)
#define ESP_LOGD(tag, fmt, ...) ESP_LOG_("D", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) ESP_LOG_("I", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) ESP_LOG_("W", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) do {} while (0)
#define ESP_LOGCONFIG(tag, fmt, ...) ESP_LOG_("C", tag, fmt, ##__VA_ARGS__)
