/**
 * @file ads7952_registers.hpp
 * @brief Register definitions and SPI frame constants for the ADS7952
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 * @ingroup ads7952_registers
 *
 * Complete register map derived from TI ADS79xx datasheet SLAS605C.
 * The ADS7952 uses 16-bit full-duplex SPI frames:
 *   - DI[15:12] mode select, DI[11] program enable, DI[10:7] channel/reset,
 *     DI[6] range, DI[5] power-down, DI[4] data descriptor, DI[3:0] GPIO
 *   - DO[15:12] channel address, DO[11:0] 12-bit ADC data
 */
#pragma once
#include <cstdint>

namespace ads7952 {
namespace reg {

/**
 * @defgroup ads7952_registers Register and Frame Constants
 * @ingroup ads7952_driver
 * @brief SPI frame bitfields and parser helpers for ADS7952 command/response words.
 */

// =============================================================================
// Command Frame Mode Select — bits [15:12]
// Datasheet Tables 1-7
// =============================================================================
/** @ingroup ads7952_registers */
namespace Mode {
  inline constexpr uint16_t CONTINUE      = 0x0000;  // 0000: Continue previous
  inline constexpr uint16_t MANUAL        = 0x1000;  // 0001: Manual channel select
  inline constexpr uint16_t AUTO_1        = 0x2000;  // 0010: Auto-1 sequencing
  inline constexpr uint16_t AUTO_2        = 0x3000;  // 0011: Auto-2 sequencing
  inline constexpr uint16_t GPIO_PROG     = 0x4000;  // 0100: GPIO programming
  inline constexpr uint16_t AUTO_1_PROG   = 0x8000;  // 1000: Auto-1 programming
  inline constexpr uint16_t AUTO_2_PROG   = 0x9000;  // 1001: Auto-2 programming
  inline constexpr uint16_t ALARM_GROUP_0 = 0xC000;  // 1100: Alarm group 0 (CH0-3)
  inline constexpr uint16_t ALARM_GROUP_1 = 0xD000;  // 1101: Alarm group 1 (CH4-7)
  inline constexpr uint16_t ALARM_GROUP_2 = 0xE000;  // 1110: Alarm group 2 (CH8-11)
  inline constexpr uint16_t ALARM_GROUP_3 = 0xF000;  // 1111: Alarm group 3 (CH12-15)
  inline constexpr uint16_t MASK          = 0xF000;
} // namespace Mode

// =============================================================================
// Bit 11: Programming enable
// =============================================================================
inline constexpr uint16_t PROGRAM_ENABLE  = 0x0800;
inline constexpr uint16_t PROGRAM_RETAIN  = 0x0000;

// =============================================================================
// Bit 10: Channel counter reset (Auto modes only)
// =============================================================================
inline constexpr uint16_t RESET_COUNTER    = 0x0400;
inline constexpr uint16_t NO_RESET_COUNTER = 0x0000;

// =============================================================================
// Bits [10:7]: Manual mode channel select
// =============================================================================
/**
 * @ingroup ads7952_registers
 * @brief Encode manual-mode channel selection bits DI[10:7].
 * @param ch Channel index (only low 4 bits are encoded).
 * @return Encoded channel-select field for command frame composition.
 */
inline constexpr uint16_t ChannelSelect(uint8_t ch) noexcept {
  return static_cast<uint16_t>((ch & 0xF) << 7);
}

// =============================================================================
// Bit 6: Input voltage range
// =============================================================================
inline constexpr uint16_t RANGE_VREF  = 0x0000;  // 0 to Vref
inline constexpr uint16_t RANGE_2VREF = 0x0040;  // 0 to 2*Vref

// =============================================================================
// Bit 5: Power-down control
// =============================================================================
inline constexpr uint16_t POWER_NORMAL = 0x0000;
inline constexpr uint16_t POWER_DOWN   = 0x0020;

// =============================================================================
// Bit 4: Data descriptor output selection
// =============================================================================
inline constexpr uint16_t OUTPUT_CHAN_ADDR = 0x0000;  // DO[15:12] = channel addr
inline constexpr uint16_t OUTPUT_GPIO_DATA = 0x0010;  // DO[15:12] = GPIO data

// =============================================================================
// Bits [3:0]: GPIO output state
// =============================================================================
/** @ingroup ads7952_registers */
namespace GPIO {
  inline constexpr uint16_t PIN0_HIGH = 0x0001;
  inline constexpr uint16_t PIN1_HIGH = 0x0002;
  inline constexpr uint16_t PIN2_HIGH = 0x0004;
  inline constexpr uint16_t PIN3_HIGH = 0x0008;
  inline constexpr uint16_t ALL_LOW   = 0x0000;
} // namespace GPIO

// =============================================================================
// Auto-1 Programming — Channel enable mask (Frame 2, bits [11:0])
// Datasheet Table 3
// =============================================================================
/**
 * @ingroup ads7952_registers
 * @brief Build the Auto-1 enable bit for a channel.
 * @param ch Channel index.
 * @return Bit mask with the selected channel bit set.
 */
inline constexpr uint16_t Auto1ChannelBit(uint8_t ch) noexcept {
  return static_cast<uint16_t>(1U << (ch & 0xF));
}

// =============================================================================
// Auto-2 Programming — Last channel (bits [9:6])
// Datasheet Table 5
// =============================================================================
/**
 * @ingroup ads7952_registers
 * @brief Encode Auto-2 last-channel field DI[9:6].
 * @param ch Last channel index.
 * @return Encoded last-channel field for AUTO_2_PROG command.
 */
inline constexpr uint16_t Auto2LastChannel(uint8_t ch) noexcept {
  return static_cast<uint16_t>((ch & 0xF) << 6);
}

// =============================================================================
// GPIO Programming Register Layout (single frame, mode=0100)
// Datasheet Table 11
// =============================================================================
/** @ingroup ads7952_registers */
namespace GPIOProg {
  // Bit 9: Reset all registers
  inline constexpr uint16_t RESET_ALL_REGS   = 0x0200;
  inline constexpr uint16_t NORMAL_OPERATION  = 0x0000;
  // Bit 8: GPIO3 function
  inline constexpr uint16_t GPIO3_PWRDOWN_IN = 0x0100;
  inline constexpr uint16_t GPIO3_AS_GPIO    = 0x0000;
  // Bit 7: GPIO2 function
  inline constexpr uint16_t GPIO2_RANGE_IN   = 0x0080;
  inline constexpr uint16_t GPIO2_AS_GPIO    = 0x0000;
  // Bits [6:4]: GPIO0 & GPIO1 alarm assignment
  inline constexpr uint16_t GPIO01_GPIO              = 0x0000;
  inline constexpr uint16_t GPIO0_HI_LO_ALARM        = 0x0010;
  inline constexpr uint16_t GPIO0_HI_ALARM            = 0x0020;
  inline constexpr uint16_t GPIO1_HI_ALARM            = 0x0040;
  inline constexpr uint16_t GPIO1_LO_GPIO0_HI_ALARM   = 0x0060;
  // Bits [3:0]: GPIO direction (1=output)
  inline constexpr uint16_t DIR_GPIO0_OUT = 0x0001;
  inline constexpr uint16_t DIR_GPIO1_OUT = 0x0002;
  inline constexpr uint16_t DIR_GPIO2_OUT = 0x0004;
  inline constexpr uint16_t DIR_GPIO3_OUT = 0x0008;
} // namespace GPIOProg

// =============================================================================
// Alarm Programming Register Layout (Frame 2 of alarm sequence)
// Datasheet Table 13
// =============================================================================
/** @ingroup ads7952_registers */
namespace Alarm {
  // Bits [15:14]: Channel within group (0-3)
  /**
   * @brief Encode alarm channel index within a 4-channel group.
   * @param ch_in_group Channel offset in group [0..3].
   * @return Encoded channel-in-group bits [15:14].
   */
  inline constexpr uint16_t ChannelInGroup(uint8_t ch_in_group) noexcept {
    return static_cast<uint16_t>((ch_in_group & 0x3) << 14);
  }
  // Bit 13: Alarm register select
  inline constexpr uint16_t HIGH_REGISTER    = 0x2000;
  inline constexpr uint16_t LOW_REGISTER     = 0x0000;
  // Bit 12: Continue/exit alarm programming
  inline constexpr uint16_t EXIT_NEXT_FRAME  = 0x1000;
  inline constexpr uint16_t CONTINUE_PROG    = 0x0000;
  // Bits [9:0]: 10-bit threshold (top 10 of 12-bit ADC value)
  /**
   * @brief Convert 12-bit ADC threshold to 10-bit alarm register format.
   * @param adc_12bit Threshold in 12-bit ADC count units.
   * @return Threshold bits [9:0] expected by alarm register.
   */
  inline constexpr uint16_t Threshold12To10(uint16_t adc_12bit) noexcept {
    return (adc_12bit >> 2) & 0x03FF;
  }

  // Alarm group mode for a given global channel number (0-11)
  /**
   * @brief Resolve alarm programming mode code for a global channel.
   * @param channel Global channel index.
   * @return One of ALARM_GROUP_0..ALARM_GROUP_3 mode constants.
   */
  inline constexpr uint16_t GroupMode(uint8_t channel) noexcept {
    uint8_t group = channel / 4;
    constexpr uint16_t modes[] = {
      Mode::ALARM_GROUP_0, Mode::ALARM_GROUP_1,
      Mode::ALARM_GROUP_2, Mode::ALARM_GROUP_3
    };
    return (group < 4) ? modes[group] : Mode::ALARM_GROUP_0;
  }
} // namespace Alarm

// =============================================================================
// Response Frame Parsing — DO[15:12] channel, DO[11:0] data
// =============================================================================
/** @ingroup ads7952_registers */
namespace Response {
  /**
   * @brief Extract channel descriptor from response frame DO[15:12].
   * @param frame 16-bit ADC response frame.
   * @return Reported channel index.
   */
  inline constexpr uint8_t GetChannel(uint16_t frame) noexcept {
    return static_cast<uint8_t>((frame >> 12) & 0x0F);
  }
  /**
   * @brief Extract conversion data from response frame DO[11:0].
   * @param frame 16-bit ADC response frame.
   * @return Raw 12-bit conversion value.
   */
  inline constexpr uint16_t GetData(uint16_t frame) noexcept {
    return frame & 0x0FFF;
  }
} // namespace Response

// =============================================================================
// Device Constants (ADS7952 specific)
// =============================================================================
inline constexpr uint8_t  NUM_CHANNELS    = 12;   ///< Number of ADC input channels.
inline constexpr uint8_t  RESOLUTION_BITS = 12;   ///< ADC resolution in bits.
inline constexpr uint16_t MAX_COUNT       = 4095; ///< Maximum 12-bit conversion value.
inline constexpr uint8_t  NUM_GPIO_PINS   = 4;    ///< Number of GPIO pins on ADS7952.
inline constexpr uint8_t  CHANNELS_PER_ALARM_GROUP = 4; ///< Channels per alarm group.
inline constexpr uint8_t  NUM_ALARM_GROUPS = 3;   ///< Valid alarm groups for CH0..CH11.
inline constexpr uint8_t  INVALID_CONVERSIONS_AFTER_POWERUP = 1; ///< Frames to discard after power-up.

} // namespace reg
} // namespace ads7952
