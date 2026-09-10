/**
 * @file ads7952.ipp
 * @brief Template implementation for the ADS7952 driver
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 * @ingroup ads7952_core
 *
 * Implements all ADS7952 driver methods using direct imperative SPI frame
 * sequences. Each operation sends the required frames inline and returns
 * success/failure — no internal state machine.
 *
 * SPI Pipeline Model (ADS7952 datasheet SLAS605C, Figure 51):
 *   A channel written in frame N is parsed at the CS rising edge that ends
 *   frame N, the MUX switches on the 2nd SCLK of frame N+1, the input is
 *   acquired during frame N+1 and sampled on the CS falling edge that opens
 *   frame N+2. The conversion for that channel is therefore clocked out in
 *   frame N+2. Frame N+1 still returns the channel that was selected two
 *   frames earlier. Every response carries its own 4-bit channel address in
 *   DO15:12; results are keyed on that address, never on frame position:
 *     Frame N:    MANUAL | channel  → stale (discard)
 *     Frame N+1:  CONTINUE          → previous channel (discard)
 *     Frame N+2:  CONTINUE          → DO15:12 == channel → result
 *   Auto-1 / Auto-2 follow the same N+2 rule for the first channel of a
 *   sequence; ReadAllChannels keys on the address for the same reason.
 *
 *   Acquisition is clocked by SCLK (14th rising edge → next CS falling
 *   edge), so the SCLK rate and the CS gap set how long the hold cap has
 *   to settle to the new channel. Run the bus at the rate the front end
 *   was characterised at (Airseal / Moonshine: 1 MHz) and keep the 16
 *   clocks of one frame contiguous.
 */
#pragma once

namespace ads7952 {

/**
 * @addtogroup ads7952_core
 * @{
 */

// =============================================================================
// Construction
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::ADS7952(SpiType&,float,float,Range) */
ADS7952<SpiType>::ADS7952(SpiType &spi, float vref, float va, Range initial_range) noexcept
    : spi_(spi),
      vref_((std::max)(ADS7952_CFG::MIN_VREF, (std::min)(vref, ADS7952_CFG::MAX_VREF))),
      va_((std::max)(ADS7952_CFG::MIN_VA, (std::min)(va, ADS7952_CFG::MAX_VA))),
      two_vref_(2.0f * vref_),
  active_vref_((initial_range == Range::TwoVref) ? two_vref_ : vref_),
  range_(initial_range) {}

// =============================================================================
// Runtime Calibration
// =============================================================================

template <typename SpiType>
void ADS7952<SpiType>::SetVref(float vref) noexcept {
  vref_ = (std::max)(ADS7952_CFG::MIN_VREF, (std::min)(vref, ADS7952_CFG::MAX_VREF));
  two_vref_ = 2.0f * vref_;
  // Update active_vref_ based on current range setting
  active_vref_ = (range_ == Range::TwoVref) ? two_vref_ : vref_;
}

template <typename SpiType>
void ADS7952<SpiType>::SetVA(float va) noexcept {
  va_ = (std::max)(ADS7952_CFG::MIN_VA, (std::min)(va, ADS7952_CFG::MAX_VA));
}

// =============================================================================
// Initialization
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::EnsureInitialized(bool) */
bool ADS7952<SpiType>::EnsureInitialized(bool force) noexcept {
  if (initialized_ && !force) {
    return true;
  }
  initialized_ = false;
  return Initialize();
}

template <typename SpiType>
bool ADS7952<SpiType>::Initialize() noexcept {
  // Datasheet: first conversion after power-up is invalid — discard it
  spiTransfer16(reg::Mode::CONTINUE);

  // Program Auto-1 with default channel mask
  if (!ProgramAuto1Channels(auto1_mask_)) {
    return false;
  }

  // Program Auto-2 with default last channel
  if (!ProgramAuto2LastChannel(auto2_last_ch_)) {
    return false;
  }

  // Enter Auto-1 mode as default operating mode
  if (!EnterAuto1Mode(true)) {
    return false;
  }

  initialized_ = true;
  return true;
}

// =============================================================================
// Channel Reading
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::ReadChannel(uint8_t) */
ReadResult ADS7952<SpiType>::ReadChannel(uint8_t channel) noexcept {
  ReadResult result{};

  if (!initialized_) {
    result.error = Error::NotInitialized;
    return result;
  }
  if (channel >= reg::NUM_CHANNELS) {
    result.error = Error::InvalidChannel;
    return result;
  }

  const uint16_t ctrl = commonControlBits();

  // Frame N: MANUAL mode with the target channel. SLAS605C Figure 51: the
  // command written in frame N is parsed on the rising edge of CS ending
  // frame N, the MUX switches on the 2nd SCLK of frame N+1, the input is
  // acquired through frame N+1 and sampled on the CS falling edge that
  // opens frame N+2 — so the conversion for this channel is clocked out in
  // frame N+2, not N+1. Frame N+1 still carries the channel selected two
  // frames ago. Rather than count frames, clock CONTINUE until the 4-bit
  // channel address in DO15:12 is the one requested (the Airseal/Moonshine
  // port's ProcessFrame contract); anything else is attributed to the wrong
  // tap.
  uint16_t cmd = reg::Mode::MANUAL | reg::PROGRAM_RETAIN
               | reg::ChannelSelect(channel) | ctrl;
  spiTransfer16(cmd);
  mode_ = Mode::Manual;

  const uint16_t cont = reg::Mode::CONTINUE | ctrl;
  for (uint8_t i = 0; i < ADS7952_CFG::MANUAL_READ_MAX_FRAMES; ++i) {
    const uint16_t resp = spiTransfer16(cont);
    const uint8_t  ch   = reg::Response::GetChannel(resp);
    if (ch == channel) {
      result.channel = ch;
      result.count   = reg::Response::GetData(resp);
      result.voltage = CountToVoltage(result.count);
      result.error   = Error::Ok;
      return result;
    }
  }

  result.channel = channel;
  result.error   = Error::Timeout;
  return result;
}

template <typename SpiType>
/** @copydoc ADS7952::ReadAllChannels() */
ChannelReadings ADS7952<SpiType>::ReadAllChannels() noexcept {
  ChannelReadings result{};

  if (!initialized_) {
    result.error = Error::NotInitialized;
    return result;
  }

  const uint16_t ctrl = commonControlBits();
  const uint16_t target_mask = auto1_mask_ & 0x0FFF;
  const uint8_t  num_enabled = popcount16(target_mask);

  if (num_enabled == 0) {
    result.error = Error::Ok;
    return result;
  }

  // Enter Auto-1 with the channel counter reset. Table 2: in Auto-1 the
  // DI11 programming bit gates DI10..DI00 as a block, so RESET_COUNTER
  // (DI10) is ignored unless DI11 = PROGRAM_ENABLE — the earlier
  // PROGRAM_RETAIN | RESET_COUNTER frame never reset the sequence. The
  // range / power / GPIO bits in `ctrl` are what the device already runs
  // with, so re-programming them here is a no-op. Response from this frame
  // is stale — discard.
  uint16_t cmd = reg::Mode::AUTO_1 | reg::PROGRAM_ENABLE
               | reg::RESET_COUNTER | ctrl;
  spiTransfer16(cmd);
  mode_ = Mode::Auto1;

  // Read until all programmed channels are received (with safety margin).
  const uint16_t cont = reg::Mode::CONTINUE | ctrl;
  const uint8_t  max_frames =
      num_enabled + ADS7952_CFG::READ_ALL_MAX_EXTRA_FRAMES;

  for (uint8_t i = 0; i < max_frames; ++i) {
    uint16_t resp = spiTransfer16(cont);
    uint8_t  ch   = reg::Response::GetChannel(resp);
    uint16_t data = reg::Response::GetData(resp);

    if (ch < ChannelReadings::MAX_CHANNELS && (target_mask & (1U << ch))) {
      result.count[ch]   = data;
      result.voltage[ch] = CountToVoltage(data);
      result.valid_mask |= static_cast<uint16_t>(1U << ch);
    }

    if (result.valid_mask == target_mask) {
      result.error = Error::Ok;
      return result;
    }
  }

  // Not all channels arrived within the allowed frame budget.
  result.error = Error::Timeout;
  return result;
}

// =============================================================================
// Voltage Conversion
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::CountToVoltage(uint16_t) const */
float ADS7952<SpiType>::CountToVoltage(uint16_t count) const noexcept {
  return (static_cast<float>(count) * active_vref_)
       / static_cast<float>(reg::MAX_COUNT);
}

template <typename SpiType>
/** @copydoc ADS7952::VoltageToCount(float) const */
uint16_t ADS7952<SpiType>::VoltageToCount(float voltage) const noexcept {
  if (voltage <= 0.0f) return 0;
  float raw = (voltage / active_vref_) * static_cast<float>(reg::MAX_COUNT);
  if (raw > static_cast<float>(reg::MAX_COUNT)) return reg::MAX_COUNT;
  return static_cast<uint16_t>(raw);
}

// =============================================================================
// Mode Control
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::EnterManualMode(uint8_t) */
bool ADS7952<SpiType>::EnterManualMode(uint8_t channel) noexcept {
  if (channel >= reg::NUM_CHANNELS) return false;

  uint16_t cmd = reg::Mode::MANUAL | reg::PROGRAM_ENABLE
               | reg::ChannelSelect(channel) | commonControlBits();
  spiTransfer16(cmd);
  mode_ = Mode::Manual;
  return true;
}

template <typename SpiType>
/** @copydoc ADS7952::EnterAuto1Mode(bool) */
bool ADS7952<SpiType>::EnterAuto1Mode(bool reset_counter) noexcept {
  uint16_t cmd = reg::Mode::AUTO_1 | reg::PROGRAM_ENABLE
               | (reset_counter ? reg::RESET_COUNTER : reg::NO_RESET_COUNTER)
               | commonControlBits();
  spiTransfer16(cmd);
  mode_ = Mode::Auto1;
  return true;
}

template <typename SpiType>
/** @copydoc ADS7952::EnterAuto2Mode(bool) */
bool ADS7952<SpiType>::EnterAuto2Mode(bool reset_counter) noexcept {
  uint16_t cmd = reg::Mode::AUTO_2 | reg::PROGRAM_ENABLE
               | (reset_counter ? reg::RESET_COUNTER : reg::NO_RESET_COUNTER)
               | commonControlBits();
  spiTransfer16(cmd);
  mode_ = Mode::Auto2;
  return true;
}

// =============================================================================
// Programming — Auto-1 Channel Mask
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::ProgramAuto1Channels(uint16_t) */
bool ADS7952<SpiType>::ProgramAuto1Channels(uint16_t channel_mask) noexcept {
  // Frame 1: Enter Auto-1 programming mode
  spiTransfer16(reg::Mode::AUTO_1_PROG);

  // Frame 2: Channel enable mask in bits [11:0]
  // (bits [15:12] are "don't care" per datasheet Table 3)
  spiTransfer16(channel_mask & 0x0FFF);

  auto1_mask_ = channel_mask & 0x0FFF;
  return true;
}

// =============================================================================
// Programming — Auto-2 Last Channel
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::ProgramAuto2LastChannel(uint8_t) */
bool ADS7952<SpiType>::ProgramAuto2LastChannel(uint8_t last_channel) noexcept {
  if (last_channel >= reg::NUM_CHANNELS) return false;

  // Single frame: mode + last channel address in bits [9:6]
  spiTransfer16(reg::Mode::AUTO_2_PROG | reg::Auto2LastChannel(last_channel));

  auto2_last_ch_ = last_channel;
  return true;
}

// =============================================================================
// Programming — GPIO Configuration
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::ProgramGPIO(const GPIOConfig&) */
bool ADS7952<SpiType>::ProgramGPIO(const GPIOConfig &config) noexcept {
  uint16_t frame = reg::Mode::GPIO_PROG;

  // Reset control (bit 9)
  if (config.reset_all_registers) {
    frame |= reg::GPIOProg::RESET_ALL_REGS;
  }

  // GPIO3 function (bit 8)
  if (config.gpio3_as_powerdown_input) {
    frame |= reg::GPIOProg::GPIO3_PWRDOWN_IN;
  }

  // GPIO2 function (bit 7)
  if (config.gpio2_as_range_input) {
    frame |= reg::GPIOProg::GPIO2_RANGE_IN;
  }

  // GPIO0/1 alarm mode (bits [6:4])
  switch (config.alarm_mode) {
    case GPIO01AlarmMode::GPIO:
      break;
    case GPIO01AlarmMode::GPIO0_HighAndLowAlarm:
      frame |= reg::GPIOProg::GPIO0_HI_LO_ALARM;
      break;
    case GPIO01AlarmMode::GPIO0_HighAlarm:
      frame |= reg::GPIOProg::GPIO0_HI_ALARM;
      break;
    case GPIO01AlarmMode::GPIO1_HighAlarm:
      frame |= reg::GPIOProg::GPIO1_HI_ALARM;
      break;
    case GPIO01AlarmMode::GPIO1_LowAlarm_GPIO0_HighAlarm:
      frame |= reg::GPIOProg::GPIO1_LO_GPIO0_HI_ALARM;
      break;
  }

  // Direction mask (bits [3:0])
  frame |= static_cast<uint16_t>(config.direction_mask & 0x0F);

  spiTransfer16(frame);
  return true;
}

// =============================================================================
// Programming — Alarm Thresholds
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::ProgramAlarm(uint8_t,AlarmBound,uint16_t) */
bool ADS7952<SpiType>::ProgramAlarm(uint8_t channel, AlarmBound bound,
                                    uint16_t threshold_12bit) noexcept {
  if (channel >= reg::NUM_CHANNELS) return false;

  // Determine alarm group (3 groups × 4 channels)
  const uint8_t group      = channel / reg::CHANNELS_PER_ALARM_GROUP;
  const uint8_t ch_in_grp  = channel % reg::CHANNELS_PER_ALARM_GROUP;

  // Select the correct alarm group mode code
  uint16_t group_mode;
  switch (group) {
    case 0: group_mode = reg::Mode::ALARM_GROUP_0; break;
    case 1: group_mode = reg::Mode::ALARM_GROUP_1; break;
    case 2: group_mode = reg::Mode::ALARM_GROUP_2; break;
    default: return false;
  }

  // Frame 1: Enter alarm group programming mode
  spiTransfer16(group_mode);

  // Frame 2: Alarm data — channel in group, hi/lo select, exit, threshold
  uint16_t alarm_frame = reg::Alarm::ChannelInGroup(ch_in_grp);

  if (bound == AlarmBound::High) {
    alarm_frame |= reg::Alarm::HIGH_REGISTER;
  }

  // Exit programming after this alarm (single-alarm API)
  alarm_frame |= reg::Alarm::EXIT_NEXT_FRAME;
  alarm_frame |= reg::Alarm::Threshold12To10(threshold_12bit);

  spiTransfer16(alarm_frame);

  // Frame 3: Any frame to complete the exit (continue mode)
  spiTransfer16(reg::Mode::CONTINUE | commonControlBits());

  return true;
}

template <typename SpiType>
/** @copydoc ADS7952::ProgramAlarmVoltage(uint8_t,AlarmBound,float) */
bool ADS7952<SpiType>::ProgramAlarmVoltage(uint8_t channel, AlarmBound bound,
                                           float voltage) noexcept {
  return ProgramAlarm(channel, bound, VoltageToCount(voltage));
}

// =============================================================================
// Range & Power
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::SetRange(Range) */
bool ADS7952<SpiType>::SetRange(Range range) noexcept {
  range_ = range;

  // Update active voltage reference
  active_vref_ = (range == Range::TwoVref) ? two_vref_ : vref_;

  // Send a mode frame with the updated range bit so the device latches it
  uint16_t cmd = reg::Mode::CONTINUE | commonControlBits();
  spiTransfer16(cmd);
  return true;
}

template <typename SpiType>
/** @copydoc ADS7952::SetPowerDown(PowerDown) */
bool ADS7952<SpiType>::SetPowerDown(PowerDown pd) noexcept {
  power_down_ = pd;

  // Send a frame so the device latches the new power-down setting
  uint16_t cmd = reg::Mode::CONTINUE | commonControlBits();
  spiTransfer16(cmd);
  return true;
}

// =============================================================================
// GPIO Output Control
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::SetGPIOOutputs(uint8_t) */
void ADS7952<SpiType>::SetGPIOOutputs(uint8_t gpio_state) noexcept {
  gpio_output_state_ = gpio_state & 0x0F;

  // The GPIO output bits [3:0] are sent with every mode control frame.
  // Send a continue frame to latch the new output levels.
  spiTransfer16(reg::Mode::CONTINUE | commonControlBits());
}

// =============================================================================
// Raw frame trace
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::RawManualFrames(uint8_t, uint16_t*, uint8_t) */
uint8_t ADS7952<SpiType>::RawManualFrames(uint8_t channel, uint16_t* out,
                                          uint8_t n) noexcept {
  if (!initialized_ || out == nullptr || n == 0 ||
      channel >= reg::NUM_CHANNELS) {
    return 0;
  }
  const uint16_t ctrl = commonControlBits();
  out[0] = spiTransfer16(reg::Mode::MANUAL | reg::PROGRAM_RETAIN
                         | reg::ChannelSelect(channel) | ctrl);
  const uint16_t cont = reg::Mode::CONTINUE | ctrl;
  for (uint8_t i = 1; i < n; ++i) {
    out[i] = spiTransfer16(cont);
  }
  mode_ = Mode::Manual;
  return n;
}

// =============================================================================
// Internal Helpers
// =============================================================================

template <typename SpiType>
/** @copydoc ADS7952::spiTransfer16(uint16_t) */
uint16_t ADS7952<SpiType>::spiTransfer16(uint16_t command) noexcept {
  uint8_t tx[2] = {
      static_cast<uint8_t>((command >> 8) & 0xFF),  // MSB first
      static_cast<uint8_t>(command & 0xFF)};
  uint8_t rx[2] = {0, 0};

  spi_.transfer(tx, rx, 2);

  return (static_cast<uint16_t>(rx[0]) << 8) | rx[1];
}

template <typename SpiType>
/** @copydoc ADS7952::commonControlBits() const */
uint16_t ADS7952<SpiType>::commonControlBits() const noexcept {
  uint16_t bits = 0;

  if (range_ == Range::TwoVref) {
    bits |= reg::RANGE_2VREF;
  }

  if (power_down_ == PowerDown::PowerDown) {
    bits |= reg::POWER_DOWN;
  }

  bits |= static_cast<uint16_t>(gpio_output_state_ & 0x0F);

  return bits;
}

template <typename SpiType>
/** @copydoc ADS7952::popcount16(uint16_t) */
constexpr uint8_t ADS7952<SpiType>::popcount16(uint16_t x) noexcept {
  x = x - ((x >> 1) & 0x5555);
  x = (x & 0x3333) + ((x >> 2) & 0x3333);
  return static_cast<uint8_t>((((x + (x >> 4)) & 0x0F0F) * 0x0101) >> 8);
}

} // namespace ads7952

/** @} */ // end of ads7952_core
