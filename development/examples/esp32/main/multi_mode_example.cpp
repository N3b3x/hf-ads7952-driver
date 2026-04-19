/**
 * @file multi_mode_example.cpp
 * @brief Demonstrates Manual, Auto-1, and Auto-2 operating modes
 * @ingroup ads7952_examples_esp32
 *
 * @details
 * This example shows:
 * - Manual mode: read specific channels one at a time
 * - Auto-1 mode: batch read a configurable channel subset
 * - Auto-2 mode: sequential read from CH0 to a programmable last channel
 * - Mode switching between all three modes
 * - Reprogramming Auto-1 channel mask and Auto-2 last channel
 *
 * @example multi_mode_example.cpp
 * Example that cycles through all ADS7952 operating modes.
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <memory>

#include "../../../inc/ads7952.hpp"
#include "esp32_ads7952_bus.hpp"
#include "esp32_ads7952_test_config.hpp"

static const char *TAG = "ADS7952_MultiMode";

/** @brief ESP-IDF entrypoint for the ADS7952 multi-mode demonstration app. */
extern "C" void app_main(void) {
  ESP_LOGI(TAG, "ADS7952 Multi-Mode Example");
  ESP_LOGI(TAG, "===========================");

  // Create and initialize SPI bus + driver
  auto bus = CreateEsp32Ads7952SpiBus();
  if (!bus || !bus->initialize()) {
    ESP_LOGE(TAG, "Failed to initialize SPI bus");
    return;
  }

  ads7952::ADS7952<Esp32Ads7952SpiBus> adc(
      *bus, ADS7952_TestConfig::ADCSpecs::VREF, 5.0f, ads7952::Range::TwoVref);

  if (!adc.EnsureInitialized()) {
    ESP_LOGE(TAG, "ADS7952 initialization failed");
    return;
  }

  ESP_LOGI(TAG, "ADS7952 initialized successfully");
  vTaskDelay(pdMS_TO_TICKS(10));

  while (true) {
    // =========================================================================
    // 1. MANUAL MODE — Read specific channels individually
    // =========================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Manual Mode: Read CH0, CH5, CH11 ---");

    constexpr uint8_t manual_channels[] = {0, 5, 11};
    for (uint8_t ch : manual_channels) {
      auto r = adc.ReadChannel(ch);
      if (r.ok()) {
        ESP_LOGI(TAG, "  CH%02u: %4u counts  (%.3f V)", ch, r.count, r.voltage);
      } else {
        ESP_LOGE(TAG, "  CH%02u: error %u", ch,
                 static_cast<unsigned>(r.error));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    // =========================================================================
    // 2. AUTO-1 MODE — Batch read with full channel mask (all 12)
    // =========================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Auto-1 Mode: Batch read all 12 channels ---");

    adc.ProgramAuto1Channels(ads7952::kAllChannels);  // All 12 channels
    adc.EnterAuto1Mode(true);

    auto all = adc.ReadAllChannels();
    if (all.ok()) {
      for (uint8_t ch = 0; ch < 12; ++ch) {
        if (all.hasChannel(ch)) {
          ESP_LOGI(TAG, "  CH%02u: %4u counts  (%.3f V)",
                   ch, all.count[ch], all.voltage[ch]);
        }
      }
    } else {
      ESP_LOGW(TAG, "  ReadAllChannels error: %u (mask=0x%03X)",
               static_cast<unsigned>(all.error), all.valid_mask);
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    // =========================================================================
    // 3. AUTO-1 MODE — Subset: only even channels (0,2,4,6,8,10)
    // =========================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Auto-1 Mode: Even channels only ---");

    adc.ProgramAuto1Channels(ads7952::kEvenChannels);  // CH0,2,4,6,8,10
    adc.EnterAuto1Mode(true);

    auto even = adc.ReadAllChannels();
    if (even.ok()) {
      for (uint8_t ch = 0; ch < 12; ch += 2) {
        if (even.hasChannel(ch)) {
          ESP_LOGI(TAG, "  CH%02u: %4u counts  (%.3f V)",
                   ch, even.count[ch], even.voltage[ch]);
        }
      }
    }

    // Restore full mask for next loop
    adc.ProgramAuto1Channels(ads7952::kAllChannels);

    vTaskDelay(pdMS_TO_TICKS(500));

    // =========================================================================
    // 4. AUTO-2 MODE — Sequential read CH0–CH5
    // =========================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Auto-2 Mode: Channels 0-5 sequential ---");

    adc.ProgramAuto2LastChannel(5);
    adc.EnterAuto2Mode(true);

    // Auto-2 reads CH0 through last_channel; use manual reads after entering
    for (uint8_t ch = 0; ch <= 5; ++ch) {
      auto r = adc.ReadChannel(ch);
      if (r.ok()) {
        ESP_LOGI(TAG, "  CH%02u: %4u counts  (%.3f V)", ch, r.count, r.voltage);
      }
    }

    // Restore default
    adc.ProgramAuto2LastChannel(11);

    vTaskDelay(pdMS_TO_TICKS(500));

    // =========================================================================
    // 5. RANGE COMPARISON — Vref vs 2×Vref on CH0
    // =========================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Range Comparison on CH0 ---");

    adc.SetRange(ads7952::Range::Vref);
    auto r_vref = adc.ReadChannel(0);

    adc.SetRange(ads7952::Range::TwoVref);
    auto r_2vref = adc.ReadChannel(0);

    // Restore default full-scale range
    adc.SetRange(ads7952::Range::TwoVref);

    if (r_vref.ok() && r_2vref.ok()) {
      ESP_LOGI(TAG, "  Vref range:  count=%u  voltage=%.3f V",
               r_vref.count, r_vref.voltage);
      ESP_LOGI(TAG, "  2xVref range: count=%u  voltage=%.3f V",
               r_2vref.count, r_2vref.voltage);
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Cycle complete, repeating in 3 seconds ===");
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}
