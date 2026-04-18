/**
 * @file ads7952_types.hpp
 * @brief Type definitions for the ADS7952 driver
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 * @ingroup ads7952_types
 *
 * Provides enum classes, result structures, and configuration types used
 * throughout the ADS7952 driver and application code.
 */
#pragma once
#include <cstdint>
#include <type_traits>

namespace ads7952 {

/**
 * @defgroup ads7952_types Driver Types
 * @ingroup ads7952_driver
 * @brief Public enums, result payloads, and helper utilities.
 */

// =============================================================================
// Operating Mode
// =============================================================================
/** @ingroup ads7952_types */
enum class Mode : uint8_t {
  Manual,  ///< Host selects channel each frame
  Auto1,   ///< Device sequences through programmed channel mask
  Auto2,   ///< Device sequences channels 0 to last_channel
};

// =============================================================================
// Input Voltage Range
// =============================================================================
/** @ingroup ads7952_types */
enum class Range : uint8_t {
  Vref,     ///< 0 to Vref (RANGE bit = 0)
  TwoVref,  ///< 0 to 2*Vref, clamped to VA (RANGE bit = 1)
};

// =============================================================================
// Power-Down Control
// =============================================================================
/** @ingroup ads7952_types */
enum class PowerDown : uint8_t {
  Normal,    ///< Normal operation
  PowerDown, ///< Device enters power-down
};

// =============================================================================
// Alarm Bound Selection
// =============================================================================
/** @ingroup ads7952_types */
enum class AlarmBound : uint8_t {
  Low  = 0,  ///< Low alarm threshold register
  High = 1,  ///< High alarm threshold register
};

// =============================================================================
// Error Codes
// =============================================================================
/** @ingroup ads7952_types */
enum class Error : uint8_t {
  Ok = 0,            ///< Operation succeeded
  NotInitialized,    ///< Driver not initialized
  InvalidChannel,    ///< Channel number out of range (0-11)
  SpiError,          ///< SPI transfer failure
  ProgrammingFailed, ///< Device programming did not complete
  ModeChangeFailed,  ///< Mode transition did not complete
  Timeout,           ///< Operation timed out waiting for data
};

// =============================================================================
// GPIO 0/1 Alarm Assignment Modes
// =============================================================================
/** @ingroup ads7952_types */
enum class GPIO01AlarmMode : uint8_t {
  GPIO                          = 0, ///< Both as general-purpose I/O
  GPIO0_HighAndLowAlarm         = 1, ///< GPIO0 = combined hi/lo alarm output
  GPIO0_HighAlarm               = 2, ///< GPIO0 = high alarm output
  GPIO1_HighAlarm               = 4, ///< GPIO1 = high alarm output
  GPIO1_LowAlarm_GPIO0_HighAlarm = 6, ///< GPIO1=low, GPIO0=high alarm
};

// =============================================================================
// GPIO Pin Index Constants (usable with SetGPIOOutputs and GPIOConfig)
// =============================================================================
/**
 * @defgroup ads7952_gpio_bits GPIO Bit Masks
 * @ingroup ads7952_types
 * @brief Bit masks for GPIO pin addressing.
 */
namespace gpio {
  /** @ingroup ads7952_gpio_bits */
  inline constexpr uint8_t kGPIO0 = 0x01; ///< Bit 0 — GPIO pin 0
  /** @ingroup ads7952_gpio_bits */
  inline constexpr uint8_t kGPIO1 = 0x02; ///< Bit 1 — GPIO pin 1
  /** @ingroup ads7952_gpio_bits */
  inline constexpr uint8_t kGPIO2 = 0x04; ///< Bit 2 — GPIO pin 2
  /** @ingroup ads7952_gpio_bits */
  inline constexpr uint8_t kGPIO3 = 0x08; ///< Bit 3 — GPIO pin 3
  /** @ingroup ads7952_gpio_bits */
  inline constexpr uint8_t kAll   = 0x0F; ///< All 4 GPIO pins
  /** @ingroup ads7952_gpio_bits */
  inline constexpr uint8_t kNone  = 0x00; ///< No GPIO pins
} // namespace gpio

// =============================================================================
// GPIO Configuration Structure
// =============================================================================
/** @ingroup ads7952_types */
struct GPIOConfig {
  GPIO01AlarmMode alarm_mode         = GPIO01AlarmMode::GPIO;
  bool gpio2_as_range_input          = false; ///< GPIO2 controls RANGE externally
  bool gpio3_as_powerdown_input      = false; ///< GPIO3 controls power-down externally

  /**
   * @brief GPIO direction control — bits [3:0], bit N = GPIO pin N.
   *
   * Set bit to 1 for output, 0 for input. Use ads7952::gpio:: constants:
   * @code
   * cfg.direction_mask = ads7952::gpio::kGPIO2 | ads7952::gpio::kGPIO3;
   * @endcode
   */
  uint8_t direction_mask             = 0;

  bool reset_all_registers           = false; ///< Reset all programming registers
};

// =============================================================================
// Single-Channel Read Result
// =============================================================================
/** @ingroup ads7952_types */
struct ReadResult {
  uint16_t count   = 0;        ///< Raw 12-bit ADC count (0-4095)
  float    voltage = 0.0f;     ///< Converted voltage
  uint8_t  channel = 0;        ///< Channel that was read (from response)
  Error    error   = Error::Ok;

  /** @brief True when @ref error equals Error::Ok. */
  bool ok() const noexcept { return error == Error::Ok; }
};

// =============================================================================
// Multi-Channel Readings Container
// =============================================================================
/** @ingroup ads7952_types */
struct ChannelReadings {
  static constexpr uint8_t MAX_CHANNELS = 12;

  uint16_t count[MAX_CHANNELS]   = {};      ///< Raw counts per channel
  float    voltage[MAX_CHANNELS] = {};      ///< Converted voltages per channel

  /**
   * @brief Bitmask indicating which channels contain valid data.
   *
   * Bit ordering: bit N corresponds to channel N (bit 0 = CH0, bit 11 = CH11).
   * Only bits [11:0] are meaningful; upper bits are always 0.
   *
   * Use `hasChannel(ch)` for single-channel checks, or inspect the mask
   * directly for bulk operations:
   * @code
   * for (uint8_t ch = 0; ch < 12; ++ch)
   *   if (readings.valid_mask & (1U << ch)) { ... }
   * @endcode
   */
  uint16_t valid_mask            = 0;

  Error    error                 = Error::Ok;

  /** @brief True when @ref error equals Error::Ok. */
  bool ok() const noexcept { return error == Error::Ok; }

  /** @brief Check if a specific channel has valid data in this reading. */
  bool hasChannel(uint8_t ch) const noexcept {
    return ch < MAX_CHANNELS && (valid_mask & (1U << ch));
  }

  /**
   * @brief Count channels currently marked valid.
   * @return Number of set bits in @ref valid_mask for channels [0..11].
   */
  uint8_t validChannelCount() const noexcept {
    uint16_t x = valid_mask;
    x = x - ((x >> 1) & 0x5555);
    x = (x & 0x3333) + ((x >> 2) & 0x3333);
    return static_cast<uint8_t>((((x + (x >> 4)) & 0x0F0F) * 0x0101) >> 8);
  }
};

// =============================================================================
// Channel Mask Helpers
// =============================================================================
/**
 * @defgroup ads7952_channel_masks Channel Mask Helpers
 * @ingroup ads7952_types
 * @brief Helpers and constants for Auto-1 sequence mask construction.
 */

/**
 * @ingroup ads7952_channel_masks
 * @brief Construct a channel enable bitmask from a list of channel numbers.
 *
 * Bit ordering: bit N corresponds to channel N (bit 0 = CH0, bit 11 = CH11).
 * Only channels 0–11 are valid; out-of-range values are silently masked.
 *
 * @tparam Channels Variadic uint8_t channel numbers
 * @param channels Channel numbers to include (0–11)
 * @return uint16_t bitmask suitable for ProgramAuto1Channels()
 *
 * @code
 * adc.ProgramAuto1Channels(ads7952::ChannelMask(0, 2, 4, 6, 8, 10));
 * @endcode
 */
template <typename... Channels>
constexpr uint16_t ChannelMask(Channels... channels) noexcept {
  static_assert((std::is_integral_v<Channels> && ...),
                "ChannelMask arguments must be integers");
  uint16_t mask = 0;
  ((mask |= static_cast<uint16_t>(1U << (static_cast<uint8_t>(channels) & 0x0F))), ...);
  return mask & 0x0FFF;
}

/**
 * @ingroup ads7952_channel_masks
 * @brief Build a contiguous channel range mask from first to last (inclusive).
 * @param first First channel index (0-11).
 * @param last Last channel index (0-11), must be >= @p first.
 * @return Bitmask containing channels [first..last], or 0 if invalid input.
 */
constexpr uint16_t ChannelRangeMask(uint8_t first, uint8_t last) noexcept {
  if (first > 11 || last > 11 || first > last) return 0;
  // (1 << (last+1)) - (1 << first) generates contiguous set bits
  return static_cast<uint16_t>(
      ((1U << (last + 1)) - (1U << first)) & 0x0FFF);
}

/// @ingroup ads7952_channel_masks
/// @name Predefined Auto-1 Channel Masks
/// Bit ordering: bit N = channel N, bit 0 = CH0 (LSB).
/// @{
inline constexpr uint16_t kAllChannels  = 0x0FFF; ///< CH0–CH11 (all 12)
inline constexpr uint16_t kEvenChannels = 0x0555; ///< CH0, CH2, CH4, CH6, CH8, CH10
inline constexpr uint16_t kOddChannels  = 0x0AAA; ///< CH1, CH3, CH5, CH7, CH9, CH11
inline constexpr uint16_t kFirstFour    = 0x000F; ///< CH0–CH3
inline constexpr uint16_t kSecondFour   = 0x00F0; ///< CH4–CH7
inline constexpr uint16_t kThirdFour    = 0x0F00; ///< CH8–CH11
/// @}

// =============================================================================
// Voltage-Count Conversion Helpers
// =============================================================================
/**
 * @defgroup ads7952_conversion Conversion Helpers
 * @ingroup ads7952_types
 * @brief Lightweight utility functions for engineering-unit conversion.
 */

/**
 * @ingroup ads7952_conversion
 * @brief Convert a voltage to a 12-bit ADC count.
 *
 * Useful for computing alarm thresholds from engineering units.
 * The result is clamped to 0–4095.
 *
 * @param voltage Target voltage in volts
 * @param vref    Reference voltage in volts
 * @return uint16_t 12-bit ADC count
 *
 * @code
 * uint16_t low  = ads7952::VoltageToCount(0.5f, 2.5f);  // → 819
 * uint16_t high = ads7952::VoltageToCount(2.0f, 2.5f);  // → 3276
 * adc.ProgramAlarm(0, ads7952::AlarmBound::Low, low);
 * @endcode
 */
constexpr uint16_t VoltageToCount(float voltage, float vref) noexcept {
  if (vref <= 0.0f || voltage <= 0.0f) return 0;
  float raw = (voltage / vref) * 4095.0f;
  if (raw > 4095.0f) return 4095;
  return static_cast<uint16_t>(raw);
}

} // namespace ads7952
