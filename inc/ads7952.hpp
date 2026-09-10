/**
 * @file ads7952.hpp
 * @brief Main driver class for the ADS7952 12-channel, 12-bit SAR ADC
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 * @ingroup ads7952_driver
 *
 * @details
 * Hardware-agnostic C++20 driver for the Texas Instruments ADS7952
 * multichannel SAR ADC using CRTP for zero-overhead SPI abstraction.
 *
 * @section ads7952_hpp_features Key Features
 * - 12 analog input channels, 12-bit resolution, up to 1 MSPS
 * - Manual, Auto-1, and Auto-2 channel sequencing modes
 * - Configurable input range (Vref or 2*Vref, clamped to VA)
 * - GPIO programming (4 pins with alarm routing)
 * - Per-channel alarm threshold programming (high/low)
 * - Power-down control
 * - Direct imperative SPI sequences (no state machine overhead)
 * - Proper first-frame discard after power-up per datasheet spec
 *
 * @section ads7952_hpp_usage Basic Usage
 * @code
 * class MySpi : public ads7952::SpiInterface<MySpi> {
 * public:
 *   void transfer(const uint8_t* tx, uint8_t* rx, size_t len) { ... }
 * };
 *
 * MySpi spi;
 * ads7952::ADS7952<MySpi> adc(spi, 2.5f, 5.0f);
 * adc.EnsureInitialized();  // Idempotent — safe to call repeatedly
 *
 * auto result = adc.ReadChannel(3);
 * if (result.ok()) printf("CH3: %.3f V\n", result.voltage);
 *
 * auto all = adc.ReadAllChannels();
 * for (uint8_t ch = 0; ch < 12; ch++)
 *   if (all.hasChannel(ch)) printf("CH%u: %u\n", ch, all.count[ch]);
 * @endcode
 */
#pragma once

#define ADS7952_HEADER_INCLUDED

#include <cstdint>
#include <cstddef>
#include <algorithm>

#include "ads7952_config.hpp"
#include "ads7952_registers.hpp"
#include "ads7952_spi_interface.hpp"
#include "ads7952_types.hpp"

namespace ads7952 {

/**
 * @defgroup ads7952_driver ADS7952 Driver
 * @brief Hardware-agnostic C++20 driver for the TI ADS7952 ADC family.
 */

/**
 * @defgroup ads7952_core Core Driver API
 * @ingroup ads7952_driver
 * @brief High-level class for initialization, conversion, sequencing, and programming.
 */

/**
 * @ingroup ads7952_core
 * @brief Main driver class for the ADS7952 ADC.
 *
 * Provides a synchronous, command-oriented API over the ADS7952 SPI protocol.
 * The class is transport-agnostic and relies on a CRTP SPI implementation.
 *
 * @tparam SpiType Platform-specific SPI class derived from SpiInterface<SpiType>
 */
template <typename SpiType>
class ADS7952 {
public:
  // ===========================================================================
  // Construction
  // ===========================================================================

  /**
   * @brief Construct a new ADS7952 driver instance.
   *
   * @param spi  Reference to platform-specific SPI interface
   * @param vref REFP pin voltage in volts (default 2.5V for REF5025)
   * @param va   VA supply voltage in volts (default 5.0V)
    * @param initial_range Initial input range to apply for conversions
   */
  explicit ADS7952(SpiType &spi,
                   float vref = ADS7952_CFG::DEFAULT_VREF,
               float va   = ADS7952_CFG::DEFAULT_VA,
               Range initial_range = ADS7952_CFG::DEFAULT_RANGE) noexcept;

  ADS7952(const ADS7952 &) = delete;
  ADS7952 &operator=(const ADS7952 &) = delete;

  // ===========================================================================
  // Initialization
  // ===========================================================================

  /**
   * @brief Ensure the driver is initialized — idempotent, safe to call repeatedly.
   *
   * On first call: discards the invalid power-up conversion, programs
   * defaults (Auto-1 channels, Auto-2 last channel), and enters Auto-1 mode.
   * On subsequent calls: returns true immediately without SPI traffic.
   *
   * @param force If true, re-runs the full initialization sequence even
   *              if the driver is already initialized. Use after a device
   *              reset or power cycle.
   * @return true if the driver is initialized after the call
   */
  bool EnsureInitialized(bool force = false) noexcept;

  /** @brief Check if the driver has been initialized. */
  bool IsInitialized() const noexcept { return initialized_; }

  // ===========================================================================
  // Channel Reading
  // ===========================================================================

  /**
   * @brief Read a single ADC channel (switches to Manual mode).
   * @pre EnsureInitialized() has been called successfully.
   * @param channel Channel number (0-11)
   * @return ReadResult with count, voltage, channel, and error status
   */
  ReadResult ReadChannel(uint8_t channel) noexcept;

  /**
   * @brief Read all channels in the current Auto-1 sequence.
   *
   * Enters Auto-1 mode with channel counter reset and reads until
   * all programmed channels are received or timeout.
   * @pre EnsureInitialized() has been called successfully.
   *
   * @return ChannelReadings with per-channel counts and voltages
   */
  ChannelReadings ReadAllChannels() noexcept;

  // ===========================================================================
  // Voltage Conversion
  // ===========================================================================

  /**
   * @brief Convert raw count to voltage using the currently active reference.
   * @param count Raw 12-bit ADC count (0-4095)
   * @return Voltage in volts
   */
  float CountToVoltage(uint16_t count) const noexcept;

  /**
   * @brief Convert raw count to voltage with explicit reference.
   * @param count Raw 12-bit ADC count (0-4095)
   * @param vref  Reference voltage in volts
   * @return Voltage in volts
   */
  static constexpr float CountToVoltage(uint16_t count, float vref) noexcept {
    return (static_cast<float>(count) * vref) / static_cast<float>(reg::MAX_COUNT);
  }

  /**
   * @brief Convert a voltage to a 12-bit ADC count using the active reference.
   *
   * Instance version — uses the currently active reference voltage
   * (affected by the Range setting). Result is clamped to 0–4095.
   *
   * @param voltage Target voltage in volts
   * @return 12-bit ADC count (0–4095)
   */
  uint16_t VoltageToCount(float voltage) const noexcept;

  // ===========================================================================
  // Mode Control
  // ===========================================================================

  /**
   * @brief Enter Manual mode, selecting the given channel for conversion.
   * @param channel Channel to select (0-11, default 0)
   * @return true on success
   */
  bool EnterManualMode(uint8_t channel = 0) noexcept;

  /**
   * @brief Enter Auto-1 mode (sequences through programmed channel mask).
   * @param reset_counter true to reset channel counter to start of sequence
   * @return true on success
   */
  bool EnterAuto1Mode(bool reset_counter = true) noexcept;

  /**
   * @brief Enter Auto-2 mode (sequences channels 0 through last_channel).
   * @param reset_counter true to reset channel counter to channel 0
   * @return true on success
   */
  bool EnterAuto2Mode(bool reset_counter = true) noexcept;

  /** @brief Get the current operating mode. */
  Mode GetMode() const noexcept { return mode_; }

  // ===========================================================================
  // Programming
  // ===========================================================================

  /**
   * @brief Program the Auto-1 channel sequence.
   *
   * Bit ordering: bit N enables channel N (bit 0 = CH0, bit 11 = CH11, LSB-first).
   * Only bits [11:0] are used; upper bits are masked off.
   *
   * Use the ads7952::ChannelMask() helper or predefined constants:
   * @code
   * adc.ProgramAuto1Channels(ads7952::ChannelMask(0, 2, 4));  // CH0, CH2, CH4
   * adc.ProgramAuto1Channels(ads7952::kAllChannels);           // All 12
   * adc.ProgramAuto1Channels(ads7952::kEvenChannels);          // Even only
   * adc.ProgramAuto1Channels(ads7952::ChannelRangeMask(0, 5)); // CH0–CH5
   * @endcode
   *
   * @param channel_mask Bitmask where bit N enables channel N (bits [11:0])
   * @return true on success
   */
  bool ProgramAuto1Channels(uint16_t channel_mask) noexcept;

  /**
   * @brief Program the Auto-2 last channel.
   * @param last_channel Last channel in sequence (0-11)
   * @return true on success
   */
  bool ProgramAuto2LastChannel(uint8_t last_channel) noexcept;

  /**
   * @brief Program GPIO pin functions, direction, and alarm routing.
   *
   * Configures which GPIO pins act as inputs or outputs, assigns alarm
   * signalling roles to GPIO0/GPIO1, optionally assigns GPIO2/GPIO3 as
   * hardware range/power-down inputs, and optionally resets all device
   * registers. Sends a single GPIO_PROG mode frame.
   *
   * @param config GPIO configuration structure
   * @return true on success
   */
  bool ProgramGPIO(const GPIOConfig &config) noexcept;

  /**
   * @brief Program an alarm threshold for a single channel (count-based).
   *
   * The 12-bit ADC count (0–4095) is internally truncated to 10 bits
   * for the hardware alarm register (only the top 10 bits are stored).
   *
   * @param channel        Channel number (0–11)
   * @param bound          Low or High alarm threshold
   * @param threshold_12bit 12-bit ADC count (0–4095); internally right-shifted
   *                        by 2 to produce 10-bit hardware threshold
   * @return true on success
   *
   * @code
   * adc.ProgramAlarm(0, ads7952::AlarmBound::High, 3000);  // Raw count
   * @endcode
   */
  bool ProgramAlarm(uint8_t channel, AlarmBound bound,
                    uint16_t threshold_12bit) noexcept;

  /**
   * @brief Program an alarm threshold for a single channel (voltage-based).
   *
   * Converts the voltage to a 12-bit count using the currently active
   * reference voltage, then programs the hardware alarm register.
   *
   * @warning The hardware alarm register stores a raw count, not a voltage.
   *          If the range is later changed via SetRange(), the stored count
   *          will correspond to a different physical voltage and the alarm
   *          will trigger at the wrong level. Re-call ProgramAlarmVoltage()
   *          after any SetRange() call to re-synchronize.
   *
   * @param channel   Channel number (0–11)
   * @param bound     Low or High alarm threshold
   * @param voltage   Threshold voltage in volts (clamped to valid range)
   * @return true on success
   *
   * @code
   * adc.ProgramAlarmVoltage(0, ads7952::AlarmBound::Low,  0.5f);
   * adc.ProgramAlarmVoltage(0, ads7952::AlarmBound::High, 2.0f);
   *
   * // After a range change, re-program to restore correct thresholds:
   * adc.SetRange(ads7952::Range::TwoVref);
   * adc.ProgramAlarmVoltage(0, ads7952::AlarmBound::Low,  0.5f);  // re-sync
   * adc.ProgramAlarmVoltage(0, ads7952::AlarmBound::High, 2.0f);  // re-sync
   * @endcode
   */
  bool ProgramAlarmVoltage(uint8_t channel, AlarmBound bound,
                           float voltage) noexcept;

  // ===========================================================================
  // Range & Power
  // ===========================================================================

  /**
   * @brief Set the input voltage range and update active reference.
   *
   * Changes the hardware reference voltage used for SAR conversions by
   * sending a CONTINUE frame with the updated range bit. Also updates
   * the internal active_vref_ used by CountToVoltage() and VoltageToCount().
   *
   * @warning Alarm thresholds are stored in the hardware as raw counts.
   *          Changing the range changes the count produced for any given
   *          physical voltage, so existing alarm thresholds will fire at
   *          different voltages after a range change. If alarms were
   *          programmed using ProgramAlarmVoltage(), re-call it after
   *          SetRange() to correct the hardware registers.
   *
   * @param range Vref or TwoVref
   * @return true on success
   */
  bool SetRange(Range range) noexcept;

  /** @brief Get the current input range setting. */
  Range GetRange() const noexcept { return range_; }

  /**
   * @brief Set the power-down mode.
   * @param pd Normal or PowerDown
   * @return true on success
   */
  bool SetPowerDown(PowerDown pd) noexcept;

  // ===========================================================================
  // GPIO Output Control
  // ===========================================================================

  /**
   * @brief Set GPIO output pin levels (for pins configured as outputs).
   *
   * Bit ordering: bit N controls GPIO pin N (bit 0 = GPIO0, bit 3 = GPIO3).
   * 1 = high, 0 = low. Use ads7952::gpio:: constants:
   * @code
   * adc.SetGPIOOutputs(ads7952::gpio::kGPIO2 | ads7952::gpio::kGPIO3);
   * adc.SetGPIOOutputs(ads7952::gpio::kNone);  // All low
   * @endcode
   *
   * @param gpio_state Bits [3:0] — 1 = high, 0 = low per pin
   */
  void SetGPIOOutputs(uint8_t gpio_state) noexcept;

  // ===========================================================================
  // Diagnostics
  // ===========================================================================

  /** @brief Get the active voltage reference (affected by range setting). */
  float GetActiveVref() const noexcept { return active_vref_; }

  /** @brief Get the configured Vref (REFP pin voltage). */
  float GetVref() const noexcept { return vref_; }

  /** @brief Get the configured VA supply voltage. */
  float GetVA() const noexcept { return va_; }

  /**
   * @brief Update the Vref value for voltage calculations.
   *
   * Use this for runtime calibration when Vref is measured externally.
   * Updates both vref_ and two_vref_ (2×Vref) accordingly.
   *
   * @param vref New Vref voltage in volts (clamped to 1.0V–2.5V)
   * @note Does not send any SPI commands — purely for software calculations.
   */
  void SetVref(float vref) noexcept;

  /**
   * @brief Update the VA supply voltage value.
   *
   * Use this for runtime calibration when VA is measured (e.g., via
   * resistor divider on an ADC channel). VA is used for validation
   * and ratiometric sensor calculations.
   *
   * @param va New VA voltage in volts (clamped to 2.7V–5.5V)
   * @note Does not send any SPI commands — purely for software calculations.
   */
  void SetVA(float va) noexcept;

  /** @brief Get the Auto-1 programmed channel mask.
   *  Bit ordering: bit N = channel N (bit 0 = CH0, bit 11 = CH11). */
  uint16_t GetAuto1ChannelMask() const noexcept { return auto1_mask_; }

  /** @brief Get the Auto-2 programmed last channel. */
  uint8_t GetAuto2LastChannel() const noexcept { return auto2_last_ch_; }

  // ===========================================================================
  // Raw frame trace (bring-up)
  // ===========================================================================
  /**
   * @brief Clock raw 16-bit frames and return every response word unparsed.
   *
   * Frame 0 is a MANUAL select of @p channel with the driver's range /
   * power / GPIO bits; frames 1..n-1 are CONTINUE. Each returned word is
   * the full DO15:0 (channel address in the top nibble, 12-bit result
   * below), so a bench can see whether the address pipeline advances and
   * whether the data field follows it — the two halves of "every channel
   * reads the same count". Leaves the device in Manual mode.
   *
   * @param channel Channel to select in frame 0 (0-11).
   * @param out     Destination for @p n response words.
   * @param n       Number of frames (including the select frame).
   * @return Number of words written (0 if not initialized / bad channel).
   */
  uint8_t RawManualFrames(uint8_t channel, uint16_t* out, uint8_t n) noexcept;

  // ===========================================================================
  // Version
  // ===========================================================================
  /** @brief Driver semantic major version. */
  static constexpr uint8_t GetDriverVersionMajor() noexcept { return 1; }
  /** @brief Driver semantic minor version. */
  static constexpr uint8_t GetDriverVersionMinor() noexcept { return 0; }
  /** @brief Driver semantic patch version. */
  static constexpr uint8_t GetDriverVersionPatch() noexcept { return 0; }

private:
  // ===========================================================================
  // Internal Helpers
  // ===========================================================================

  /** @brief Perform a single 16-bit SPI transfer (MSB first). */
  uint16_t spiTransfer16(uint16_t command) noexcept;

  /**
   * @brief Internal initialization — performs the actual hardware setup.
   * @return true on success
   */
  bool Initialize() noexcept;

  /** @brief Build the range/power/gpio portion of a mode control frame. */
  uint16_t commonControlBits() const noexcept;

  /** @brief Count set bits in a 16-bit value. */
  static constexpr uint8_t popcount16(uint16_t x) noexcept;

  // ===========================================================================
  // State
  // ===========================================================================
  SpiType &spi_;

  // Voltage references
  float vref_;          ///< REFP pin voltage (1.0V to 2.5V per datasheet)
  float va_;            ///< VA supply voltage (for validation/ratiometric use)
  float two_vref_;      ///< 2 × vref (full-scale for 2×Vref range mode)
  float active_vref_;   ///< Currently selected reference for conversions

  // Device state tracking
  bool     initialized_       = false;
  Mode     mode_              = Mode::Manual;
  Range    range_             = ADS7952_CFG::DEFAULT_RANGE;
  PowerDown power_down_       = PowerDown::Normal;

  // Programming state tracking
  uint16_t auto1_mask_        = ADS7952_CFG::DEFAULT_AUTO1_MASK;
  uint8_t  auto2_last_ch_     = ADS7952_CFG::DEFAULT_AUTO2_LAST_CH;
  uint8_t  gpio_output_state_ = 0;
};

} // namespace ads7952

// Template implementation — included only at end of this header
#include "ads7952.ipp"
