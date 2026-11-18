#pragma once

#include <cstdio>
#include <cstdlib>

__attribute__((format(printf, 2, 3))) inline void fake_log(const char *, const char *format, ...)
{
  (void)format;
}

#define ESP_LOGE(tag, ...) fake_log(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) fake_log(tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) fake_log(tag, __VA_ARGS__)
#define ESP_LOGD(tag, ...) fake_log(tag, __VA_ARGS__)
#define ESP_LOGV(tag, ...) fake_log(tag, __VA_ARGS__)
#define LOG_CLIMATE(prefix, type, obj)
