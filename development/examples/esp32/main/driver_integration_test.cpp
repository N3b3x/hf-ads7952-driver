/**
 * @file driver_integration_test.cpp
 * @brief Comprehensive ADS7952 driver integration test suite
 * @ingroup ads7952_examples_tests
 *
 * @details
 * Validates the complete ADS7952 driver API on real hardware:
 * - Initialization and SPI communication
 * - Manual single-channel reading
 * - Auto-1 batch channel reading
 * - Auto-2 sequential channel reading
 * - Mode switching (Manual / Auto-1 / Auto-2)
 * - Range configuration (Vref / 2×Vref)
 * - Auto-1 and Auto-2 programming
 * - Alarm threshold programming
 * - GPIO configuration and output control
 * - Power-down mode
 * - Voltage conversion accuracy
 * - Repeated-read stability
 *
 * @section driver_integration_test_hardware Hardware
 * ADS7952 connected via SPI to ESP32-S3.
 *
 * @author N3b3x
 * @date 2025
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cmath>
#include <memory>
#include <stdio.h>

#include "../../../inc/ads7952.hpp"
#include "TestFramework.h"
#include "esp32_ads7952_bus.hpp"
#include "esp32_ads7952_test_config.hpp"

// =============================================================================
// Test Section Enables
// =============================================================================
#define ENABLE_INITIALIZATION_TESTS       1
#define ENABLE_CHANNEL_READING_TESTS      1
#define ENABLE_MODE_SWITCHING_TESTS       1
#define ENABLE_AUTO_SEQUENCE_TESTS        1
#define ENABLE_RANGE_TESTS                1
#define ENABLE_ALARM_GPIO_TESTS           1
#define ENABLE_POWER_DOWN_TESTS           1
#define ENABLE_VOLTAGE_CONVERSION_TESTS   1
#define ENABLE_STABILITY_TESTS            1

static const char *TAG = "ADS7952_Test";
static TestResults g_test_results;

// Global driver instances (created in app_main)
static std::unique_ptr<Esp32Ads7952SpiBus> g_bus;
static ads7952::ADS7952<Esp32Ads7952SpiBus> *g_adc = nullptr;

// =============================================================================
// INITIALIZATION TESTS
// =============================================================================

/** @brief Verify SPI bus object exists and reports initialized state. */
static bool test_spi_bus_initialization() noexcept {
  ESP_LOGI(TAG, "Testing SPI bus initialization...");
  bool ok = g_bus && g_bus->isInitialized();
  ESP_LOGI(TAG, "  SPI bus initialized: %s", ok ? "YES" : "NO");
  return ok;
}

/** @brief Verify ADS7952 driver is initialized and mode is readable. */
static bool test_driver_initialization() noexcept {
  ESP_LOGI(TAG, "Testing ADS7952 driver initialization...");
  bool ok = g_adc->IsInitialized();
  ESP_LOGI(TAG, "  Driver initialized: %s, mode=%u",
           ok ? "YES" : "NO", static_cast<unsigned>(g_adc->GetMode()));
  return ok;
}

// =============================================================================
// CHANNEL READING TESTS
// =============================================================================

/** @brief Validate a single manual read on CH0. */
static bool test_read_single_channel_manual() noexcept {
  ESP_LOGI(TAG, "Testing manual single-channel read (CH0)...");
  auto result = g_adc->ReadChannel(0);
  ESP_LOGI(TAG, "  CH%u: count=%u, voltage=%.3f V, error=%u",
           result.channel, result.count, result.voltage,
           static_cast<unsigned>(result.error));
  return result.ok() && result.count <= ADS7952_TestConfig::ADCSpecs::MAX_COUNT;
}

/** @brief Validate manual reads across all 12 channels. */
static bool test_read_all_12_channels_manual() noexcept {
  ESP_LOGI(TAG, "Testing manual read of all 12 channels...");
  bool all_ok = true;
  for (uint8_t ch = 0; ch < ADS7952_TestConfig::ADCSpecs::NUM_CHANNELS; ++ch) {
    auto r = g_adc->ReadChannel(ch);
    if (!r.ok() || r.count > ADS7952_TestConfig::ADCSpecs::MAX_COUNT) {
      ESP_LOGE(TAG, "  CH%u FAIL: count=%u error=%u", ch, r.count,
               static_cast<unsigned>(r.error));
      all_ok = false;
    }
  }
  ESP_LOGI(TAG, "  All 12 channels read: %s", all_ok ? "PASS" : "FAIL");
  return all_ok;
}

/** @brief Ensure invalid channel reads return InvalidChannel error. */
static bool test_read_invalid_channel() noexcept {
  ESP_LOGI(TAG, "Testing read of invalid channel (CH12)...");
  auto result = g_adc->ReadChannel(12);
  bool ok = (result.error == ads7952::Error::InvalidChannel);
  ESP_LOGI(TAG, "  Error code: %u (expected InvalidChannel=%u) → %s",
           static_cast<unsigned>(result.error),
           static_cast<unsigned>(ads7952::Error::InvalidChannel),
           ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Validate Auto-1 batch read returns valid data for enabled channels. */
static bool test_read_all_channels_auto1() noexcept {
  ESP_LOGI(TAG, "Testing Auto-1 batch read of all channels...");
  g_adc->EnterAuto1Mode(true);
  auto readings = g_adc->ReadAllChannels();
  ESP_LOGI(TAG, "  valid_mask=0x%03X, error=%u",
           readings.valid_mask, static_cast<unsigned>(readings.error));

  if (!readings.ok()) return false;

  bool all_valid = true;
  for (uint8_t ch = 0; ch < ADS7952_TestConfig::ADCSpecs::NUM_CHANNELS; ++ch) {
    if (!readings.hasChannel(ch)) {
      ESP_LOGW(TAG, "  CH%u missing from results", ch);
      all_valid = false;
    } else if (readings.count[ch] > ADS7952_TestConfig::ADCSpecs::MAX_COUNT) {
      ESP_LOGE(TAG, "  CH%u out of range: %u", ch, readings.count[ch]);
      all_valid = false;
    }
  }
  return all_valid;
}

// =============================================================================
// MODE SWITCHING TESTS
// =============================================================================

/** @brief Verify transition into Manual mode. */
static bool test_enter_manual_mode() noexcept {
  ESP_LOGI(TAG, "Testing enter Manual mode...");
  bool ok = g_adc->EnterManualMode(0);
  ok = ok && (g_adc->GetMode() == ads7952::Mode::Manual);
  ESP_LOGI(TAG, "  Mode now: %u → %s",
           static_cast<unsigned>(g_adc->GetMode()), ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Verify transition into Auto-1 mode. */
static bool test_enter_auto1_mode() noexcept {
  ESP_LOGI(TAG, "Testing enter Auto-1 mode...");
  bool ok = g_adc->EnterAuto1Mode(true);
  ok = ok && (g_adc->GetMode() == ads7952::Mode::Auto1);
  ESP_LOGI(TAG, "  Mode now: %u → %s",
           static_cast<unsigned>(g_adc->GetMode()), ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Verify transition into Auto-2 mode and restore Auto-1. */
static bool test_enter_auto2_mode() noexcept {
  ESP_LOGI(TAG, "Testing enter Auto-2 mode...");
  bool ok = g_adc->EnterAuto2Mode(true);
  ok = ok && (g_adc->GetMode() == ads7952::Mode::Auto2);
  ESP_LOGI(TAG, "  Mode now: %u → %s",
           static_cast<unsigned>(g_adc->GetMode()), ok ? "PASS" : "FAIL");
  // Return to Auto-1 for subsequent tests
  g_adc->EnterAuto1Mode(true);
  return ok;
}

/** @brief Validate full mode round-trip sequence across all modes. */
static bool test_mode_round_trip() noexcept {
  ESP_LOGI(TAG, "Testing mode round-trip: Auto1→Manual→Auto2→Auto1...");
  g_adc->EnterAuto1Mode(true);
  bool ok1 = (g_adc->GetMode() == ads7952::Mode::Auto1);
  g_adc->EnterManualMode(0);
  bool ok2 = (g_adc->GetMode() == ads7952::Mode::Manual);
  g_adc->EnterAuto2Mode(true);
  bool ok3 = (g_adc->GetMode() == ads7952::Mode::Auto2);
  g_adc->EnterAuto1Mode(true);
  bool ok4 = (g_adc->GetMode() == ads7952::Mode::Auto1);

  bool ok = ok1 && ok2 && ok3 && ok4;
  ESP_LOGI(TAG, "  Round-trip: %s", ok ? "PASS" : "FAIL");
  return ok;
}

// =============================================================================
// AUTO SEQUENCE TESTS
// =============================================================================

/** @brief Program Auto-1 subset mask and verify four channels are returned. */
static bool test_program_auto1_subset() noexcept {
  ESP_LOGI(TAG, "Testing Auto-1 programming with subset (CH0-3)...");
  bool ok = g_adc->ProgramAuto1Channels(ads7952::kFirstFour);  // CH0-CH3
  ok = ok && (g_adc->GetAuto1ChannelMask() == ads7952::kFirstFour);
  ESP_LOGI(TAG, "  Mask=0x%03X → %s", g_adc->GetAuto1ChannelMask(),
           ok ? "PASS" : "FAIL");

  // Verify batch read returns exactly 4 channels
  g_adc->EnterAuto1Mode(true);
  auto readings = g_adc->ReadAllChannels();
  uint8_t count = 0;
  for (uint8_t ch = 0; ch < 12; ++ch) {
    if (readings.hasChannel(ch)) ++count;
  }
  ESP_LOGI(TAG, "  Received %u channels (expected 4)", count);
  ok = ok && (count == 4) && readings.ok();

  // Restore all 12 channels
  g_adc->ProgramAuto1Channels(ads7952::kAllChannels);
  return ok;
}

/** @brief Program Auto-2 last channel and verify it is stored. */
static bool test_program_auto2_last_channel() noexcept {
  ESP_LOGI(TAG, "Testing Auto-2 programming (last_ch=5)...");
  bool ok = g_adc->ProgramAuto2LastChannel(5);
  ok = ok && (g_adc->GetAuto2LastChannel() == 5);
  ESP_LOGI(TAG, "  Last channel=%u → %s",
           g_adc->GetAuto2LastChannel(), ok ? "PASS" : "FAIL");

  // Restore default
  g_adc->ProgramAuto2LastChannel(11);
  return ok;
}

/** @brief Verify invalid Auto-2 last channel is rejected without state change. */
static bool test_program_auto2_invalid_channel() noexcept {
  ESP_LOGI(TAG, "Testing Auto-2 programming with invalid channel (12)...");
  uint8_t prev = g_adc->GetAuto2LastChannel();
  bool rejected = !g_adc->ProgramAuto2LastChannel(12);
  bool unchanged = (g_adc->GetAuto2LastChannel() == prev);
  bool ok = rejected && unchanged;
  ESP_LOGI(TAG, "  Rejected=%s, unchanged=%s → %s",
           rejected ? "YES" : "NO", unchanged ? "YES" : "NO",
           ok ? "PASS" : "FAIL");
  return ok;
}

// =============================================================================
// RANGE TESTS
// =============================================================================

/** @brief Verify Vref range selection and active reference value. */
static bool test_set_range_vref() noexcept {
  ESP_LOGI(TAG, "Testing set range to Vref...");
  bool ok = g_adc->SetRange(ads7952::Range::Vref);
  ok = ok && (g_adc->GetRange() == ads7952::Range::Vref);
  float expected_vref = ADS7952_TestConfig::ADCSpecs::VREF;
  bool vref_ok = (std::fabs(g_adc->GetActiveVref() - expected_vref) < 0.01f);
  ok = ok && vref_ok;
  ESP_LOGI(TAG, "  Active Vref=%.3f V (expected %.3f) → %s",
           g_adc->GetActiveVref(), expected_vref, ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Verify 2xVref range selection and clamped active reference. */
static bool test_set_range_2vref() noexcept {
  ESP_LOGI(TAG, "Testing set range to 2×Vref...");
  bool ok = g_adc->SetRange(ads7952::Range::TwoVref);
  ok = ok && (g_adc->GetRange() == ads7952::Range::TwoVref);
  // 2×Vref is clamped to min(2×2.5, 5.0) = 5.0
  float expected = 5.0f;
  bool vref_ok = (std::fabs(g_adc->GetActiveVref() - expected) < 0.01f);
  ok = ok && vref_ok;
  ESP_LOGI(TAG, "  Active Vref=%.3f V (expected %.3f) → %s",
           g_adc->GetActiveVref(), expected, ok ? "PASS" : "FAIL");
  // Restore Vref range
  g_adc->SetRange(ads7952::Range::Vref);
  return ok;
}

/** @brief Confirm channel reads remain valid after changing range. */
static bool test_reading_after_range_change() noexcept {
  ESP_LOGI(TAG, "Testing channel read after range change...");
  g_adc->SetRange(ads7952::Range::TwoVref);
  auto r1 = g_adc->ReadChannel(0);
  g_adc->SetRange(ads7952::Range::Vref);
  auto r2 = g_adc->ReadChannel(0);

  bool ok = r1.ok() && r2.ok();
  ESP_LOGI(TAG, "  2xVref: count=%u V=%.3f  |  Vref: count=%u V=%.3f → %s",
           r1.count, r1.voltage, r2.count, r2.voltage,
           ok ? "PASS" : "FAIL");
  return ok;
}

// =============================================================================
// ALARM & GPIO TESTS
// =============================================================================

/** @brief Program a low alarm threshold for CH0. */
static bool test_program_alarm_low() noexcept {
  ESP_LOGI(TAG, "Testing alarm programming (CH0, Low, threshold=1000)...");
  bool ok = g_adc->ProgramAlarm(0, ads7952::AlarmBound::Low, 1000);
  ESP_LOGI(TAG, "  Result: %s", ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Program a high alarm threshold for CH5. */
static bool test_program_alarm_high() noexcept {
  ESP_LOGI(TAG, "Testing alarm programming (CH5, High, threshold=3500)...");
  bool ok = g_adc->ProgramAlarm(5, ads7952::AlarmBound::High, 3500);
  ESP_LOGI(TAG, "  Result: %s", ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Ensure invalid channel alarm programming fails. */
static bool test_program_alarm_invalid_channel() noexcept {
  ESP_LOGI(TAG, "Testing alarm programming with invalid channel (CH12)...");
  bool ok = !g_adc->ProgramAlarm(12, ads7952::AlarmBound::Low, 1000);
  ESP_LOGI(TAG, "  Correctly rejected: %s", ok ? "YES" : "NO");
  return ok;
}

/** @brief Program low/high alarm thresholds via voltage API. */
static bool test_program_alarm_voltage() noexcept {
  ESP_LOGI(TAG, "Testing voltage-based alarm (CH2, Low=0.5V, High=2.0V)...");
  bool low_ok = g_adc->ProgramAlarmVoltage(2, ads7952::AlarmBound::Low, 0.5f);
  bool high_ok = g_adc->ProgramAlarmVoltage(2, ads7952::AlarmBound::High, 2.0f);
  bool ok = low_ok && high_ok;
  ESP_LOGI(TAG, "  Low=%s, High=%s → %s",
           low_ok ? "OK" : "FAIL", high_ok ? "OK" : "FAIL",
           ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Program default GPIO configuration frame. */
static bool test_program_gpio_default() noexcept {
  ESP_LOGI(TAG, "Testing GPIO programming (default config)...");
  ads7952::GPIOConfig config{};
  bool ok = g_adc->ProgramGPIO(config);
  ESP_LOGI(TAG, "  Result: %s", ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Configure GPIO outputs and toggle output patterns. */
static bool test_program_gpio_outputs() noexcept {
  ESP_LOGI(TAG, "Testing GPIO output control...");
  ads7952::GPIOConfig config{};
  config.direction_mask = ads7952::gpio::kAll;  // All 4 GPIOs as outputs
  bool ok = g_adc->ProgramGPIO(config);

  if (ok) {
    g_adc->SetGPIOOutputs(ads7952::gpio::kGPIO0 | ads7952::gpio::kGPIO2);  // GPIO0=high, GPIO2=high
    vTaskDelay(pdMS_TO_TICKS(1));
    g_adc->SetGPIOOutputs(ads7952::gpio::kGPIO1 | ads7952::gpio::kGPIO3);  // GPIO1=high, GPIO3=high
    vTaskDelay(pdMS_TO_TICKS(1));
    g_adc->SetGPIOOutputs(ads7952::gpio::kNone);  // All low
  }

  ESP_LOGI(TAG, "  Result: %s", ok ? "PASS" : "FAIL");
  return ok;
}

// =============================================================================
// POWER DOWN TESTS
// =============================================================================

/** @brief Validate power-down entry/exit and post-resume conversion capability. */
static bool test_power_down_and_resume() noexcept {
  ESP_LOGI(TAG, "Testing power-down and resume...");

  // Read channel before power-down
  auto before = g_adc->ReadChannel(0);
  if (!before.ok()) return false;

  // Enter power-down
  bool pd_ok = g_adc->SetPowerDown(ads7952::PowerDown::PowerDown);

  // Resume normal operation
  bool resume_ok = g_adc->SetPowerDown(ads7952::PowerDown::Normal);
  vTaskDelay(pdMS_TO_TICKS(1));  // Allow device recovery

  // Read channel after resume
  auto after = g_adc->ReadChannel(0);
  bool ok = pd_ok && resume_ok && after.ok();
  ESP_LOGI(TAG, "  Before: count=%u | After: count=%u → %s",
           before.count, after.count, ok ? "PASS" : "FAIL");
  return ok;
}

// =============================================================================
// VOLTAGE CONVERSION TESTS
// =============================================================================

/** @brief Verify static CountToVoltage at zero input count. */
static bool test_count_to_voltage_zero() noexcept {
  ESP_LOGI(TAG, "Testing CountToVoltage(0) == 0.0 V...");
  float v = ads7952::ADS7952<Esp32Ads7952SpiBus>::CountToVoltage(
      0, ADS7952_TestConfig::ADCSpecs::VREF);
  bool ok = (std::fabs(v) < 0.001f);
  ESP_LOGI(TAG, "  V(0) = %.4f → %s", v, ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Verify static CountToVoltage at full-scale input count. */
static bool test_count_to_voltage_max() noexcept {
  ESP_LOGI(TAG, "Testing CountToVoltage(4095) == Vref...");
  float v = ads7952::ADS7952<Esp32Ads7952SpiBus>::CountToVoltage(
      4095, ADS7952_TestConfig::ADCSpecs::VREF);
  bool ok = (std::fabs(v - ADS7952_TestConfig::ADCSpecs::VREF) < 0.01f);
  ESP_LOGI(TAG, "  V(4095) = %.4f → %s", v, ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Verify static CountToVoltage at mid-scale count. */
static bool test_count_to_voltage_midscale() noexcept {
  ESP_LOGI(TAG, "Testing CountToVoltage(2048) == ~Vref/2...");
  float v = ads7952::ADS7952<Esp32Ads7952SpiBus>::CountToVoltage(
      2048, ADS7952_TestConfig::ADCSpecs::VREF);
  float expected = ADS7952_TestConfig::ADCSpecs::VREF / 2.0f;
  bool ok = (std::fabs(v - expected) < 0.01f);
  ESP_LOGI(TAG, "  V(2048) = %.4f (expected %.4f) → %s",
           v, expected, ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Verify instance and static count-to-voltage conversions match. */
static bool test_instance_count_to_voltage() noexcept {
  ESP_LOGI(TAG, "Testing instance CountToVoltage matches static...");
  float v_static = ads7952::ADS7952<Esp32Ads7952SpiBus>::CountToVoltage(
      2048, ADS7952_TestConfig::ADCSpecs::VREF);
  float v_inst = g_adc->CountToVoltage(2048);
  bool ok = (std::fabs(v_static - v_inst) < 0.001f);
  ESP_LOGI(TAG, "  Static=%.4f, Instance=%.4f → %s",
           v_static, v_inst, ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Verify instance VoltageToCount around half-scale voltage. */
static bool test_voltage_to_count_instance() noexcept {
  ESP_LOGI(TAG, "Testing instance VoltageToCount...");
  // VoltageToCount(1.25V) at Vref=2.5V should be ~2048
  uint16_t count = g_adc->VoltageToCount(1.25f);
  bool ok = (std::abs(static_cast<int>(count) - 2048) <= 1);
  ESP_LOGI(TAG, "  VoltageToCount(1.25V) = %u (expected ~2048) → %s",
           count, ok ? "PASS" : "FAIL");
  return ok;
}

/** @brief Verify VoltageToCount clamps at boundary/invalid voltages. */
static bool test_voltage_to_count_boundary() noexcept {
  ESP_LOGI(TAG, "Testing VoltageToCount boundary conditions...");
  uint16_t zero = g_adc->VoltageToCount(0.0f);
  uint16_t max = g_adc->VoltageToCount(100.0f);  // Should clamp to 4095
  uint16_t neg = g_adc->VoltageToCount(-1.0f);   // Should clamp to 0
  bool ok = (zero == 0) && (max == 4095) && (neg == 0);
  ESP_LOGI(TAG, "  V(0)=%u, V(100)=%u, V(-1)=%u → %s",
           zero, max, neg, ok ? "PASS" : "FAIL");
  return ok;
}

// =============================================================================
// STABILITY TESTS
// =============================================================================

/** @brief Run repeated manual reads and collect spread statistics. */
static bool test_repeated_manual_reads() noexcept {
  ESP_LOGI(TAG, "Testing 100 repeated manual reads on CH0...");
  constexpr int N = 100;
  uint32_t sum = 0;
  uint16_t min_val = 0xFFFF, max_val = 0;
  bool all_ok = true;

  for (int i = 0; i < N; ++i) {
    auto r = g_adc->ReadChannel(0);
    if (!r.ok()) {
      all_ok = false;
      break;
    }
    sum += r.count;
    if (r.count < min_val) min_val = r.count;
    if (r.count > max_val) max_val = r.count;
  }

  float avg = static_cast<float>(sum) / static_cast<float>(N);
  uint16_t spread = max_val - min_val;
  ESP_LOGI(TAG, "  avg=%.1f, min=%u, max=%u, spread=%u",
           avg, min_val, max_val, spread);
  return all_ok;
}

/** @brief Run repeated Auto-1 batch reads and require full masks each cycle. */
static bool test_repeated_auto1_batch_reads() noexcept {
  ESP_LOGI(TAG, "Testing 50 repeated Auto-1 batch reads...");
  g_adc->EnterAuto1Mode(true);
  constexpr int N = 50;
  int full_reads = 0;

  for (int i = 0; i < N; ++i) {
    auto readings = g_adc->ReadAllChannels();
    if (readings.ok() && readings.valid_mask == ads7952::kAllChannels) {
      ++full_reads;
    }
  }

  bool ok = (full_reads == N);
  ESP_LOGI(TAG, "  %d/%d successful full reads → %s", full_reads, N,
           ok ? "PASS" : "FAIL");
  return ok;
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

/** @brief Execute enabled integration-test sections and print summary. */
extern "C" void app_main(void) {
  ESP_LOGI(TAG, "ADS7952 Driver Integration Test Suite");
  ESP_LOGI(TAG, "======================================");
  ESP_LOGI(TAG, "Driver version: %u.%u.%u",
           ads7952::ADS7952<Esp32Ads7952SpiBus>::GetDriverVersionMajor(),
           ads7952::ADS7952<Esp32Ads7952SpiBus>::GetDriverVersionMinor(),
           ads7952::ADS7952<Esp32Ads7952SpiBus>::GetDriverVersionPatch());

  // Initialize SPI bus
  g_bus = CreateEsp32Ads7952SpiBus();
  if (!g_bus || !g_bus->initialize()) {
    ESP_LOGE(TAG, "FATAL: Failed to initialize SPI bus");
    return;
  }

  // Create and initialize ADS7952 driver
  static ads7952::ADS7952<Esp32Ads7952SpiBus> adc(
      *g_bus, ADS7952_TestConfig::ADCSpecs::VREF, 5.0f, ads7952::Range::TwoVref);
  g_adc = &adc;

  if (!adc.EnsureInitialized()) {
    ESP_LOGE(TAG, "FATAL: ADS7952 initialization failed");
    return;
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  // --- Initialization Tests ---
#if ENABLE_INITIALIZATION_TESTS
  print_test_section_header(TAG, "INITIALIZATION TESTS");
  RUN_TEST(test_spi_bus_initialization);
  RUN_TEST(test_driver_initialization);
#else
  print_test_section_header(TAG, "INITIALIZATION TESTS", false);
#endif

  // --- Channel Reading Tests ---
#if ENABLE_CHANNEL_READING_TESTS
  print_test_section_header(TAG, "CHANNEL READING TESTS");
  RUN_TEST(test_read_single_channel_manual);
  RUN_TEST(test_read_all_12_channels_manual);
  RUN_TEST(test_read_invalid_channel);
  RUN_TEST(test_read_all_channels_auto1);
#else
  print_test_section_header(TAG, "CHANNEL READING TESTS", false);
#endif

  // --- Mode Switching Tests ---
#if ENABLE_MODE_SWITCHING_TESTS
  print_test_section_header(TAG, "MODE SWITCHING TESTS");
  RUN_TEST(test_enter_manual_mode);
  RUN_TEST(test_enter_auto1_mode);
  RUN_TEST(test_enter_auto2_mode);
  RUN_TEST(test_mode_round_trip);
#else
  print_test_section_header(TAG, "MODE SWITCHING TESTS", false);
#endif

  // --- Auto Sequence Tests ---
#if ENABLE_AUTO_SEQUENCE_TESTS
  print_test_section_header(TAG, "AUTO SEQUENCE TESTS");
  RUN_TEST(test_program_auto1_subset);
  RUN_TEST(test_program_auto2_last_channel);
  RUN_TEST(test_program_auto2_invalid_channel);
#else
  print_test_section_header(TAG, "AUTO SEQUENCE TESTS", false);
#endif

  // --- Range Tests ---
#if ENABLE_RANGE_TESTS
  print_test_section_header(TAG, "RANGE TESTS");
  RUN_TEST(test_set_range_vref);
  RUN_TEST(test_set_range_2vref);
  RUN_TEST(test_reading_after_range_change);
#else
  print_test_section_header(TAG, "RANGE TESTS", false);
#endif

  // --- Alarm & GPIO Tests ---
#if ENABLE_ALARM_GPIO_TESTS
  print_test_section_header(TAG, "ALARM & GPIO TESTS");
  RUN_TEST(test_program_alarm_low);
  RUN_TEST(test_program_alarm_high);
  RUN_TEST(test_program_alarm_voltage);
  RUN_TEST(test_program_alarm_invalid_channel);
  RUN_TEST(test_program_gpio_default);
  RUN_TEST(test_program_gpio_outputs);
#else
  print_test_section_header(TAG, "ALARM & GPIO TESTS", false);
#endif

  // --- Power Down Tests ---
#if ENABLE_POWER_DOWN_TESTS
  print_test_section_header(TAG, "POWER DOWN TESTS");
  RUN_TEST(test_power_down_and_resume);
#else
  print_test_section_header(TAG, "POWER DOWN TESTS", false);
#endif

  // --- Voltage Conversion Tests ---
#if ENABLE_VOLTAGE_CONVERSION_TESTS
  print_test_section_header(TAG, "VOLTAGE CONVERSION TESTS");
  RUN_TEST(test_count_to_voltage_zero);
  RUN_TEST(test_count_to_voltage_max);
  RUN_TEST(test_count_to_voltage_midscale);
  RUN_TEST(test_instance_count_to_voltage);
  RUN_TEST(test_voltage_to_count_instance);
  RUN_TEST(test_voltage_to_count_boundary);
#else
  print_test_section_header(TAG, "VOLTAGE CONVERSION TESTS", false);
#endif

  // --- Stability Tests ---
#if ENABLE_STABILITY_TESTS
  print_test_section_header(TAG, "STABILITY TESTS");
  RUN_TEST(test_repeated_manual_reads);
  RUN_TEST(test_repeated_auto1_batch_reads);
#else
  print_test_section_header(TAG, "STABILITY TESTS", false);
#endif

  // --- Summary ---
  print_test_summary(g_test_results, "ADS7952 DRIVER", TAG);
  cleanup_test_progress_indicator();
}
