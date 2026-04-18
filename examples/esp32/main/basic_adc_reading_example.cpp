/**
 * @defgroup ads7952_examples Usage Examples
 * @ingroup ads7952_driver
 * @brief Reference applications demonstrating practical ADS7952 driver usage.
 */

/**
 * @defgroup ads7952_examples_esp32 ESP32 Example Applications
 * @ingroup ads7952_examples
 * @brief Example applications built on ESP-IDF and the ADS7952 C++ driver.
 */

/**
 * @defgroup ads7952_examples_tests ESP32 Integration and Validation Tests
 * @ingroup ads7952_examples
 * @brief Hardware-backed test applications validating ADS7952 driver behavior.
 */

/**
 * @defgroup ads7952_examples_support ESP32 Example Support Utilities
 * @ingroup ads7952_examples
 * @brief Shared support components used by ESP32 examples and tests.
 */

/**
 * @file basic_adc_reading_example.cpp
 * @brief Basic ADC channel reading example with voltage conversion
 * @ingroup ads7952_examples_esp32
 *
 * @details
 * Minimal end-to-end example that brings up SPI transport, initializes the
 * ADS7952 driver, performs manual per-channel reads, and then performs
 * Auto-1 batched reads with voltage conversion.
 *
 * @section basic_adc_reading_flow Execution Flow
 * - ADS7952 initialization (Auto-1/Auto-2 programming, mode entry)
 * - Reading individual ADC channels via Manual mode
 * - Reading all channels via Auto-1 batch mode
 * - Converting raw counts to voltage using the driver's built-in method
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <memory>

#include "../../../inc/ads7952.hpp"
#include "esp32_ads7952_bus.hpp"
#include "esp32_ads7952_test_config.hpp"

static const char *TAG = "ADS7952_Basic";

/** @brief ESP-IDF entry point for the basic ADS7952 read example. */
extern "C" void app_main(void) {
  ESP_LOGI(TAG, "ADS7952 Basic ADC Reading Example");
  ESP_LOGI(TAG, "===================================");
  ESP_LOGI(TAG, "Driver v%u.%u.%u",
           ads7952::ADS7952<Esp32Ads7952SpiBus>::GetDriverVersionMajor(),
           ads7952::ADS7952<Esp32Ads7952SpiBus>::GetDriverVersionMinor(),
           ads7952::ADS7952<Esp32Ads7952SpiBus>::GetDriverVersionPatch());

  // Create and initialize SPI bus
  auto bus = CreateEsp32Ads7952SpiBus();
  if (!bus || !bus->initialize()) {
    ESP_LOGE(TAG, "Failed to create/initialize SPI bus");
    return;
  }

  // Create ADS7952 driver with 2.5 V reference, 5.0 V supply
  ads7952::ADS7952<Esp32Ads7952SpiBus> adc(
      *bus, ADS7952_TestConfig::ADCSpecs::VREF, 5.0f);

  // Initialize: discards first invalid conversion, programs defaults,
  //             enters Auto-1 mode. Safe to call repeatedly.
  if (!adc.EnsureInitialized()) {
    ESP_LOGE(TAG, "ADS7952 initialization failed");
    return;
  }

  ESP_LOGI(TAG, "ADS7952 initialized — mode=%u, range=%u",
           static_cast<unsigned>(adc.GetMode()),
           static_cast<unsigned>(adc.GetRange()));

  vTaskDelay(pdMS_TO_TICKS(10));

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "=== Manual Mode: Read each channel individually ===");

  for (uint8_t ch = 0; ch < ADS7952_TestConfig::ADCSpecs::NUM_CHANNELS; ++ch) {
    auto result = adc.ReadChannel(ch);
    if (result.ok()) {
      ESP_LOGI(TAG, "  CH%02u: %4u counts  (%.3f V)",
               ch, result.count, result.voltage);
    } else {
      ESP_LOGE(TAG, "  CH%02u: read error %u", ch,
               static_cast<unsigned>(result.error));
    }
  }

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "=== Auto-1 Mode: Batch read all channels ===");

  // Switch back to Auto-1 for batch reading
  adc.EnterAuto1Mode(true);

  while (true) {
    auto readings = adc.ReadAllChannels();
    if (readings.ok()) {
      for (uint8_t ch = 0; ch < ADS7952_TestConfig::ADCSpecs::NUM_CHANNELS; ++ch) {
        if (readings.hasChannel(ch)) {
          ESP_LOGI(TAG, "  CH%02u: %4u counts  (%.3f V)",
                   ch, readings.count[ch], readings.voltage[ch]);
        }
      }
    } else {
      ESP_LOGW(TAG, "  ReadAllChannels error: %u (valid_mask=0x%03X)",
               static_cast<unsigned>(readings.error), readings.valid_mask);
    }

    ESP_LOGI(TAG, "---");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
