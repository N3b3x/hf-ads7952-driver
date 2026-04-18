/**
 * @file alarm_gpio_example.cpp
 * @brief Demonstrates alarm threshold programming and GPIO control
 * @ingroup ads7952_examples_esp32
 * @example alarm_gpio_example.cpp
 *
 * @details
 * Demonstrates alarm-threshold programming and GPIO behavior on ADS7952.
 *
 * @section alarm_gpio_flow Example Flow
 * - Programming alarm thresholds (low and high) on multiple channels
 * - Configuring GPIO pins as outputs
 * - Toggling GPIO output levels
 * - Configuring GPIO0/1 as alarm outputs
 * - Reading channels and checking against alarm thresholds
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <memory>

#include "../../../inc/ads7952.hpp"
#include "esp32_ads7952_bus.hpp"
#include "esp32_ads7952_test_config.hpp"

static const char *TAG = "ADS7952_AlarmGPIO";

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "ADS7952 Alarm & GPIO Example");
  ESP_LOGI(TAG, "=============================");

  // Create and initialize SPI bus + driver
  auto bus = CreateEsp32Ads7952SpiBus();
  if (!bus || !bus->initialize()) {
    ESP_LOGE(TAG, "Failed to initialize SPI bus");
    return;
  }

  ads7952::ADS7952<Esp32Ads7952SpiBus> adc(
      *bus, ADS7952_TestConfig::ADCSpecs::VREF, 5.0f);

  if (!adc.EnsureInitialized()) {
    ESP_LOGE(TAG, "ADS7952 initialization failed");
    return;
  }

  ESP_LOGI(TAG, "ADS7952 initialized");
  vTaskDelay(pdMS_TO_TICKS(10));

  // =========================================================================
  // 1. GPIO CONFIGURATION — Set GPIO2 and GPIO3 as outputs
  // =========================================================================
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "--- GPIO Configuration ---");

  {
    ads7952::GPIOConfig gpio_cfg{};
    gpio_cfg.direction_mask = ads7952::gpio::kGPIO2 | ads7952::gpio::kGPIO3;
    gpio_cfg.alarm_mode = ads7952::GPIO01AlarmMode::GPIO;  // GPIO0/1 as I/O

    if (adc.ProgramGPIO(gpio_cfg)) {
      ESP_LOGI(TAG, "  GPIO configured: GPIO2,3 as outputs");
    } else {
      ESP_LOGE(TAG, "  GPIO programming failed");
    }
  }

  // =========================================================================
  // 2. GPIO OUTPUT TOGGLING
  // =========================================================================
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "--- GPIO Output Toggle ---");

  for (int i = 0; i < 4; ++i) {
    adc.SetGPIOOutputs(ads7952::gpio::kGPIO2);  // GPIO2 = HIGH
    ESP_LOGI(TAG, "  GPIO2=HIGH, GPIO3=LOW");
    vTaskDelay(pdMS_TO_TICKS(250));

    adc.SetGPIOOutputs(ads7952::gpio::kGPIO3);  // GPIO3 = HIGH
    ESP_LOGI(TAG, "  GPIO2=LOW,  GPIO3=HIGH");
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  adc.SetGPIOOutputs(ads7952::gpio::kNone);
  ESP_LOGI(TAG, "  All GPIO outputs LOW");

  // =========================================================================
  // 3. ALARM THRESHOLD PROGRAMMING
  // =========================================================================
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "--- Alarm Threshold Programming ---");

  // CH0: Voltage-based API — simply pass voltage directly
  constexpr float ALARM_LOW_V  = 0.5f;
  constexpr float ALARM_HIGH_V = 2.0f;

  ESP_LOGI(TAG, "  CH0 Low  alarm: %.2f V (voltage API)", ALARM_LOW_V);
  ESP_LOGI(TAG, "  CH0 High alarm: %.2f V (voltage API)", ALARM_HIGH_V);

  if (adc.ProgramAlarmVoltage(0, ads7952::AlarmBound::Low, ALARM_LOW_V)) {
    ESP_LOGI(TAG, "  CH0 Low alarm programmed OK");
  } else {
    ESP_LOGE(TAG, "  CH0 Low alarm programming FAILED");
  }

  if (adc.ProgramAlarmVoltage(0, ads7952::AlarmBound::High, ALARM_HIGH_V)) {
    ESP_LOGI(TAG, "  CH0 High alarm programmed OK");
  } else {
    ESP_LOGE(TAG, "  CH0 High alarm programming FAILED");
  }

  // CH4: Count-based API — pass raw 12-bit ADC counts directly
  constexpr uint16_t CH4_LOW_COUNT  = 500;
  constexpr uint16_t CH4_HIGH_COUNT = 3500;

  if (adc.ProgramAlarm(4, ads7952::AlarmBound::Low, CH4_LOW_COUNT)) {
    ESP_LOGI(TAG, "  CH4 Low alarm (count=%u) programmed OK", CH4_LOW_COUNT);
  }
  if (adc.ProgramAlarm(4, ads7952::AlarmBound::High, CH4_HIGH_COUNT)) {
    ESP_LOGI(TAG, "  CH4 High alarm (count=%u) programmed OK", CH4_HIGH_COUNT);
  }

  // =========================================================================
  // 4. ALARM OUTPUTS — Configure GPIO0 as alarm output
  // =========================================================================
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "--- GPIO0 as Alarm Output ---");

  {
    ads7952::GPIOConfig alarm_gpio{};
    alarm_gpio.alarm_mode = ads7952::GPIO01AlarmMode::GPIO0_HighAndLowAlarm;
    alarm_gpio.direction_mask = ads7952::gpio::kGPIO2 | ads7952::gpio::kGPIO3;

    if (adc.ProgramGPIO(alarm_gpio)) {
      ESP_LOGI(TAG, "  GPIO0 configured as combined hi/lo alarm output");
    }
  }

  // =========================================================================
  // 5. MONITORING LOOP — Read and check against thresholds
  // =========================================================================
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "--- Alarm Monitoring Loop ---");

  while (true) {
    auto r0 = adc.ReadChannel(0);
    auto r4 = adc.ReadChannel(4);

    // CH0: Compare using voltage (matches the voltage-based alarm programming)
    if (r0.ok()) {
      const char *status = "NORMAL";
      if (r0.voltage < ALARM_LOW_V) status = "** LOW ALARM **";
      else if (r0.voltage > ALARM_HIGH_V) status = "** HIGH ALARM **";

      ESP_LOGI(TAG, "  CH0: %4u (%.3f V)  %s",
               r0.count, r0.voltage, status);
    }

    // CH4: Compare using counts (matches the count-based alarm programming)
    if (r4.ok()) {
      const char *status = "NORMAL";
      if (r4.count < CH4_LOW_COUNT) status = "** LOW ALARM **";
      else if (r4.count > CH4_HIGH_COUNT) status = "** HIGH ALARM **";

      ESP_LOGI(TAG, "  CH4: %4u (%.3f V)  %s",
               r4.count, r4.voltage, status);
    }

    // Toggle GPIO3 as a heartbeat indicator
    static bool toggle = false;
    adc.SetGPIOOutputs(toggle ? ads7952::gpio::kGPIO3 : ads7952::gpio::kNone);
    toggle = !toggle;

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
