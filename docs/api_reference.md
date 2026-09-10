---
layout: default
title: "📖 API Reference"
description: "Complete API reference for the HF-ADS7952 driver"
parent: "📚 Documentation"
nav_order: 7
permalink: /docs/api_reference/
---

# 📖 API Reference

Complete API documentation for the ADS7952 driver library.

---

## Namespace

All symbols reside in the `ads7952` namespace. Configuration constants are in `ADS7952_CFG`. Register-level constants are in `ads7952::reg`.

---

## Class: `ADS7952<SpiType>`

Main driver class. Template parameter `SpiType` must be derived from `SpiInterface<SpiType>`.

### Constructor

```cpp
explicit ADS7952(SpiType& spi,
                 float vref = ADS7952_CFG::DEFAULT_VREF,    // 2.5f
                 float va   = ADS7952_CFG::DEFAULT_VA) noexcept; // 5.0f
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `spi` | `SpiType&` | — | Reference to CRTP SPI interface instance |
| `vref` | `float` | 2.5 | External reference voltage (REFP) in volts |
| `va` | `float` | 5.0 | Analog supply voltage (VA) in volts |

The constructor computes `two_vref_ = min(2 × vref, va)` and sets `active_vref_ = vref`. Copy/move constructors are deleted.

---

### Initialization

#### `EnsureInitialized()`

```cpp
bool EnsureInitialized(bool force = false) noexcept;
```

Ensure the driver is initialized — **idempotent**, safe to call repeatedly.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `force` | `bool` | `false` | If `true`, re-runs the full initialization sequence even if already initialized. Use after a device reset or power cycle. |

**First call sequence:**
1. Discards first (invalid) conversion after power-up
2. Programs Auto-1 channel mask with `DEFAULT_AUTO1_MASK`
3. Programs Auto-2 last channel with `DEFAULT_AUTO2_LAST_CH`
4. Enters Auto-1 mode

**Subsequent calls:** Returns `true` immediately without SPI traffic (unless `force` is `true`).

**Returns:** `true` if the driver is initialized after the call.

#### `IsInitialized()`

```cpp
bool IsInitialized() const noexcept;
```

Returns `true` if the driver has been successfully initialized.

---

### Channel Reading

#### `ReadChannel()`

```cpp
ReadResult ReadChannel(uint8_t channel) noexcept;
```

Read a single ADC channel. Internally switches to Manual mode and clocks the
SLAS605C Figure 51 pipeline until the response carries the requested channel:

1. **Frame N:** MANUAL mode + channel select → response is stale (discarded)
2. **Frame N+1:** CONTINUE → still the channel selected two frames earlier (discarded)
3. **Frame N+2:** CONTINUE → DO15:12 equals the requested channel → result

The driver keys on the 4-bit channel address in DO15:12, not on frame
position; it gives up with `Error::Timeout` after
`ADS7952_CFG::MANUAL_READ_MAX_FRAMES` CONTINUE frames.

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `channel` | `uint8_t` | 0–11 | Channel number to read |

**Returns:** `ReadResult` (see [Structures](#readresult)).

**Errors:**
- `Error::NotInitialized` — `EnsureInitialized()` not yet called
- `Error::InvalidChannel` — channel ≥ 12

---

#### `ReadAllChannels()`

```cpp
ChannelReadings ReadAllChannels() noexcept;
```

Read all channels in the current Auto-1 sequence. Enters Auto-1 mode with counter reset and reads until all programmed channels are received or timeout.

The number of SPI frames is `popcount(auto1_mask) + READ_ALL_MAX_EXTRA_FRAMES`.

**Returns:** `ChannelReadings` (see [Structures](#channelreadings)).

**Errors:**
- `Error::NotInitialized` — `EnsureInitialized()` not yet called
- `Error::Timeout` — not all channels received within frame budget

---

### Voltage Conversion

#### `CountToVoltage()` (instance)

```cpp
float CountToVoltage(uint16_t count) const noexcept;
```

Convert raw 12-bit count to voltage using the currently active reference (`active_vref_`). The active reference reflects the current range setting (Vref or 2×Vref).

Formula: $V = \frac{\text{count} \times V_{\text{active}}}{4095}$

#### `CountToVoltage()` (static)

```cpp
static constexpr float CountToVoltage(uint16_t count, float vref) noexcept;
```

Convert raw count to voltage with an explicit reference. Can be called without a driver instance.

| Parameter | Type | Description |
|-----------|------|--------------|
| `count` | `uint16_t` | Raw ADC count (0–4095) |
| `vref` | `float` | Reference voltage in volts |
| **Returns** | `float` | Voltage in volts |

#### `VoltageToCount()` (instance)

```cpp
uint16_t VoltageToCount(float voltage) const noexcept;
```

Convert a voltage to a 12-bit ADC count using the **currently active** reference voltage (affected by the Range setting). Result is clamped to 0–4095.

Formula: $\text{count} = \text{clamp}\left(\frac{\text{voltage} \times 4095}{V_{\text{active}}},\, 0,\, 4095\right)$

| Parameter | Type | Description |
|-----------|------|--------------|
| `voltage` | `float` | Target voltage in volts |
| **Returns** | `uint16_t` | 12-bit ADC count (0–4095) |

> **Free function alternative:** `ads7952::VoltageToCount(voltage, vref)` is available in `ads7952_types.hpp` for use without a driver instance (e.g. at compile time with `constexpr`).

---

### Mode Control

#### `EnterManualMode()`

```cpp
bool EnterManualMode(uint8_t channel = 0) noexcept;
```

Enter Manual mode, selecting the given channel for the next conversion. Returns `false` if channel ≥ 12.

#### `EnterAuto1Mode()`

```cpp
bool EnterAuto1Mode(bool reset_counter = true) noexcept;
```

Enter Auto-1 mode. If `reset_counter` is `true`, resets the channel counter to the start of the sequence.

#### `EnterAuto2Mode()`

```cpp
bool EnterAuto2Mode(bool reset_counter = true) noexcept;
```

Enter Auto-2 mode. If `reset_counter` is `true`, resets the counter to channel 0.

#### `GetMode()`

```cpp
Mode GetMode() const noexcept;
```

Returns the current operating mode.

---

### Programming

#### `ProgramAuto1Channels()`

```cpp
bool ProgramAuto1Channels(uint16_t channel_mask) noexcept;
```

Program the Auto-1 channel sequence. Uses a 2-frame SPI sequence:
1. **Frame 1:** AUTO_1_PROG mode command
2. **Frame 2:** Channel enable mask in bits [11:0]

**Bit ordering:** bit N enables channel N (bit 0 = CH0, bit 11 = CH11, LSB-first).

| Parameter | Type | Description |
|-----------|------|-------------|
| `channel_mask` | `uint16_t` | Bitmask — bit N enables channel N (bits [11:0] used) |

Use the helper functions and predefined constants instead of raw hex:

```cpp
adc.ProgramAuto1Channels(ads7952::ChannelMask(0, 2, 4));    // Arbitrary set
adc.ProgramAuto1Channels(ads7952::kAllChannels);              // All 12
adc.ProgramAuto1Channels(ads7952::kEvenChannels);             // Even only
adc.ProgramAuto1Channels(ads7952::ChannelRangeMask(0, 5));    // CH0–CH5
```

#### `ProgramAuto2LastChannel()`

```cpp
bool ProgramAuto2LastChannel(uint8_t last_channel) noexcept;
```

Program the last channel for Auto-2 sequential scan (CH0 through `last_channel`). Returns `false` if `last_channel` ≥ 12.

#### `ProgramGPIO()`

```cpp
bool ProgramGPIO(const GPIOConfig& config) noexcept;
```

Program GPIO pin functions and direction. Sends a single GPIO_PROG mode frame. See [GPIOConfig](#gpioconfig) for configuration options.

#### `ProgramAlarm()`

```cpp
bool ProgramAlarm(uint8_t channel, AlarmBound bound,
                  uint16_t threshold_12bit) noexcept;
```

Program a high or low alarm threshold using a **raw 12-bit ADC count**. Uses a 3-frame SPI sequence:
1. **Frame 1:** Enter alarm group programming mode (group = channel / 4)
2. **Frame 2:** Alarm data with channel-in-group, hi/lo select, threshold, and exit flag
3. **Frame 3:** CONTINUE to complete exit

| Parameter | Type | Description |
|-----------|------|--------------|
| `channel` | `uint8_t` | Channel number (0–11) |
| `bound` | `AlarmBound` | `Low` or `High` threshold |
| `threshold_12bit` | `uint16_t` | 12-bit ADC count (0–4095); internally right-shifted by 2 to produce 10-bit hardware threshold |

```cpp
adc.ProgramAlarm(4, ads7952::AlarmBound::Low,  500);
adc.ProgramAlarm(4, ads7952::AlarmBound::High, 3500);
```

Returns `false` if `channel` ≥ 12.

#### `ProgramAlarmVoltage()`

```cpp
bool ProgramAlarmVoltage(uint8_t channel, AlarmBound bound,
                         float voltage) noexcept;
```

Program a high or low alarm threshold using a **voltage in volts**. Converts the voltage to a 12-bit count using the currently active reference (`active_vref_`), then delegates to `ProgramAlarm()`.

> **⚠️ Range invalidation:** The hardware alarm register stores a **raw count**, not a voltage. If you call `SetRange()` after programming voltage-based alarms, the stored count will correspond to a different physical voltage and the alarm will fire at the wrong level. **Re-call `ProgramAlarmVoltage()` after any `SetRange()` call** to re-sync the hardware registers. The new call will automatically use the updated `active_vref_`.
>
> Count-based alarms (`ProgramAlarm()`) are not affected by this — the user is responsible for knowing what physical voltage a given count maps to at the active reference.

| Parameter | Type | Description |
|-----------|------|--------------|
| `channel` | `uint8_t` | Channel number (0–11) |
| `bound` | `AlarmBound` | `Low` or `High` threshold |
| `voltage` | `float` | Threshold in volts (clamped to valid range) |

```cpp
adc.ProgramAlarmVoltage(0, ads7952::AlarmBound::Low,  0.5f);
adc.ProgramAlarmVoltage(0, ads7952::AlarmBound::High, 2.0f);

// After a range change, re-sync alarm thresholds:
adc.SetRange(ads7952::Range::TwoVref);
adc.ProgramAlarmVoltage(0, ads7952::AlarmBound::Low,  0.5f);  // uses new active_vref_
adc.ProgramAlarmVoltage(0, ads7952::AlarmBound::High, 2.0f);  // uses new active_vref_
```

Returns `false` if `channel` ≥ 12.

---

### Range & Power

#### `SetRange()`

```cpp
bool SetRange(Range range) noexcept;
```

Set the input voltage range and update the active reference voltage. Sends a CONTINUE frame to latch the new range bit.

| Setting | Active Reference |
|---------|-----------------|
| `Range::Vref` | `vref_` |
| `Range::TwoVref` | `min(2 × vref_, va_)` |

> **⚠️ Alarm threshold invalidation:** Alarm thresholds in hardware are stored as raw counts. After `SetRange()`, the same physical voltage produces a different count, so existing alarm thresholds will fire at different voltages. If any alarms were programmed via `ProgramAlarmVoltage()`, re-call it after `SetRange()` to correct the hardware registers. Count-based alarms (`ProgramAlarm()`) are unaffected by this caveat but must be manually recalculated if a voltage-accurate threshold is required.

#### `GetRange()`

```cpp
Range GetRange() const noexcept;
```

#### `SetPowerDown()`

```cpp
bool SetPowerDown(PowerDown pd) noexcept;
```

Enter or exit power-down mode. Sends a CONTINUE frame to latch the new power-down bit.

#### `SetGPIOOutputs()`

```cpp
void SetGPIOOutputs(uint8_t gpio_state) noexcept;
```

Set GPIO output pin levels. **Bit ordering:** bit N controls GPIO pin N (bit 0 = GPIO0, bit 3 = GPIO3). 1 = high, 0 = low. The GPIO state is included in every subsequent SPI frame via `commonControlBits()`.

Use the `ads7952::gpio::` constants:

```cpp
adc.SetGPIOOutputs(ads7952::gpio::kGPIO2 | ads7952::gpio::kGPIO3);
adc.SetGPIOOutputs(ads7952::gpio::kNone);  // All low
```

---

### Diagnostics

| Method | Returns | Description |
|--------|---------|-------------|
| `GetVref()` | `float` | Configured reference voltage (Vref) |
| `GetActiveVref()` | `float` | Active reference (affected by range setting) |
| `GetAuto1ChannelMask()` | `uint16_t` | Programmed Auto-1 channel bitmask (bit N = CH N) |
| `GetAuto2LastChannel()` | `uint8_t` | Programmed Auto-2 last channel |

### Version

| Method | Returns | Description |
|--------|---------|-------------|
| `GetDriverVersionMajor()` | `uint8_t` | Major version (1) |
| `GetDriverVersionMinor()` | `uint8_t` | Minor version (0) |
| `GetDriverVersionPatch()` | `uint8_t` | Patch version (0) |

All version methods are `static constexpr`.

---

## Class: `SpiInterface<Derived>`

CRTP base class for SPI hardware abstraction. Your platform-specific class must inherit from this and provide the `transfer()` method.

### Required Method (in derived class)

```cpp
protected:
    void transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length);
```

See [Platform Integration](platform_integration.md) for complete implementation examples.

---

## Enumerations

### `Mode`

```cpp
enum class Mode : uint8_t { Manual, Auto1, Auto2 };
```

| Value | Description |
|-------|-------------|
| `Manual` | Host selects channel each frame |
| `Auto1` | Device sequences through programmed channel bitmask |
| `Auto2` | Device sequences channels 0 through last_channel |

### `Range`

```cpp
enum class Range : uint8_t { Vref, TwoVref };
```

| Value | Description |
|-------|-------------|
| `Vref` | Input range: 0 to Vref |
| `TwoVref` | Input range: 0 to min(2×Vref, VA) |

### `PowerDown`

```cpp
enum class PowerDown : uint8_t { Normal, PowerDown };
```

| Value | Description |
|-------|-------------|
| `Normal` | Normal operation |
| `PowerDown` | Device enters power-down |

### `AlarmBound`

```cpp
enum class AlarmBound : uint8_t { Low = 0, High = 1 };
```

### `Error`

```cpp
enum class Error : uint8_t {
    Ok = 0, NotInitialized, InvalidChannel, SpiError,
    ProgrammingFailed, ModeChangeFailed, Timeout
};
```

| Value | Description |
|-------|-------------|
| `Ok` | Operation succeeded |
| `NotInitialized` | `EnsureInitialized()` not yet called |
| `InvalidChannel` | Channel number ≥ 12 |
| `SpiError` | SPI transfer failure |
| `ProgrammingFailed` | Device programming did not complete |
| `ModeChangeFailed` | Mode transition did not complete |
| `Timeout` | Operation timed out waiting for data |

### `GPIO01AlarmMode`

```cpp
enum class GPIO01AlarmMode : uint8_t {
    GPIO = 0,
    GPIO0_HighAndLowAlarm = 1,
    GPIO0_HighAlarm = 2,
    GPIO1_HighAlarm = 4,
    GPIO1_LowAlarm_GPIO0_HighAlarm = 6
};
```

| Value | GPIO0 Function | GPIO1 Function |
|-------|---------------|----------------|
| `GPIO` | General I/O | General I/O |
| `GPIO0_HighAndLowAlarm` | Combined hi/lo alarm output | General I/O |
| `GPIO0_HighAlarm` | High alarm output | General I/O |
| `GPIO1_HighAlarm` | General I/O | High alarm output |
| `GPIO1_LowAlarm_GPIO0_HighAlarm` | High alarm output | Low alarm output |

---

## Structures

### `ReadResult`

Returned by `ReadChannel()`.

```cpp
struct ReadResult {
    uint16_t count   = 0;         // Raw 12-bit ADC count (0–4095)
    float    voltage = 0.0f;      // Converted voltage using active reference
    uint8_t  channel = 0;         // Channel from response (verification)
    Error    error   = Error::Ok; // Error status

    bool ok() const noexcept;     // Returns true if error == Error::Ok
};
```

### `ChannelReadings`

Returned by `ReadAllChannels()`.

```cpp
struct ChannelReadings {
    static constexpr uint8_t MAX_CHANNELS = 12;

    uint16_t count[12]   = {};     // Raw counts per channel
    float    voltage[12] = {};     // Converted voltages per channel
    uint16_t valid_mask  = 0;      // Bit N = channel N has valid data (bit 0 = CH0)
    Error    error       = Error::Ok;

    bool ok() const noexcept;                    // Returns true if error == Error::Ok
    bool hasChannel(uint8_t ch) const noexcept;  // Check if channel has valid data
    uint8_t validChannelCount() const noexcept;  // Count of valid channels
};
```

### `GPIOConfig`

Configuration for `ProgramGPIO()`.

```cpp
struct GPIOConfig {
    GPIO01AlarmMode alarm_mode         = GPIO01AlarmMode::GPIO;
    bool gpio2_as_range_input          = false;  // GPIO2 controls RANGE externally
    bool gpio3_as_powerdown_input      = false;  // GPIO3 controls power-down externally
    uint8_t direction_mask             = 0;      // Bit N = GPIO pin N (1=output, 0=input)
    bool reset_all_registers           = false;  // Reset all programming registers
};
```

Use `ads7952::gpio::` constants for `direction_mask`:

```cpp
ads7952::GPIOConfig cfg{};
cfg.direction_mask = ads7952::gpio::kGPIO2 | ads7952::gpio::kGPIO3;
```

---

## Configuration Namespace: `ADS7952_CFG`

| Constant | Type | Default | Description |
|----------|------|---------|-------------|
| `DEFAULT_MODE` | `Mode` | `Manual` | Default operating mode |
| `DEFAULT_RANGE` | `Range` | `Vref` | Default input range |
| `DEFAULT_VREF` | `float` | `2.5f` | Reference voltage (V) |
| `DEFAULT_VA` | `float` | `5.0f` | Analog supply voltage (V) |
| `DEFAULT_AUTO1_MASK` | `uint16_t` | `0x0FFF` | Auto-1 channel mask |
| `DEFAULT_AUTO2_LAST_CH` | `uint8_t` | `11` | Auto-2 last channel |
| `NUM_CHANNELS` | `uint8_t` | `12` | Number of ADC channels |
| `RESOLUTION_BITS` | `uint8_t` | `12` | ADC resolution |
| `MAX_COUNT` | `uint16_t` | `4095` | Maximum ADC count |
| `READ_ALL_MAX_EXTRA_FRAMES` | `uint8_t` | `4` | Extra frames for ReadAllChannels |
| `MAX_RETRIES` | `uint8_t` | `3` | Max retries for operations |

All constants can be overridden via `CONFIG_ADS7952_*` preprocessor defines. See [Configuration](configuration.md).

---

## Register Namespace: `ads7952::reg`

Low-level register constants in `ads7952_registers.hpp`. Normally not used directly — the driver handles all frame construction internally.

| Sub-namespace | Description |
|---------------|-------------|
| `reg::Mode` | SPI mode select constants (CONTINUE, MANUAL, AUTO_1, etc.) |
| `reg::GPIO` | GPIO output pin constants (PIN0_HIGH through PIN3_HIGH) |
| `reg::GPIOProg` | GPIO programming register bits |
| `reg::Alarm` | Alarm programming register bits and helpers |
| `reg::Response` | Response frame parsing (`GetChannel()`, `GetData()`) |

Key helper functions:
- `reg::ChannelSelect(ch)` — encode channel in manual mode bits
- `reg::Auto1ChannelBit(ch)` — single channel bit for Auto-1 mask
- `reg::Auto2LastChannel(ch)` — encode last channel for Auto-2
- `reg::Alarm::Threshold12To10(adc_12bit)` — convert 12-bit threshold to 10-bit register value

---

## Free Functions & Constants

### Channel Mask Helpers

```cpp
// Build mask from arbitrary channel list (variadic, compile-time safe)
template <typename... Channels>
constexpr uint16_t ChannelMask(Channels... channels) noexcept;

// Build mask for contiguous range [first, last] inclusive
constexpr uint16_t ChannelRangeMask(uint8_t first, uint8_t last) noexcept;
```

### Predefined Channel Masks

| Constant | Value | Channels |
|----------|-------|----------|
| `kAllChannels` | `0x0FFF` | CH0–CH11 |
| `kEvenChannels` | `0x0555` | CH0, CH2, CH4, CH6, CH8, CH10 |
| `kOddChannels` | `0x0AAA` | CH1, CH3, CH5, CH7, CH9, CH11 |
| `kFirstFour` | `0x000F` | CH0–CH3 |
| `kSecondFour` | `0x00F0` | CH4–CH7 |
| `kThirdFour` | `0x0F00` | CH8–CH11 |

All masks follow the convention: **bit N = channel N** (bit 0 = CH0, LSB-first).

### GPIO Pin Constants (`ads7952::gpio::`)

| Constant | Value | Description |
|----------|-------|-------------|
| `kGPIO0` | `0x01` | GPIO pin 0 (bit 0) |
| `kGPIO1` | `0x02` | GPIO pin 1 (bit 1) |
| `kGPIO2` | `0x04` | GPIO pin 2 (bit 2) |
| `kGPIO3` | `0x08` | GPIO pin 3 (bit 3) |
| `kAll` | `0x0F` | All 4 GPIO pins |
| `kNone` | `0x00` | No GPIO pins |

Usable with both `SetGPIOOutputs()` and `GPIOConfig::direction_mask`.

### Voltage-Count Conversion

```cpp
constexpr uint16_t VoltageToCount(float voltage, float vref) noexcept;
```

Converts a voltage to a 12-bit ADC count (0–4095), clamped. Useful for alarm thresholds:

```cpp
uint16_t thresh = ads7952::VoltageToCount(1.5f, 2.5f);  // → 2457
adc.ProgramAlarm(0, ads7952::AlarmBound::High, thresh);
```

---

**Navigation**
⬅️ [CMake Integration](cmake_integration.md)  ➡️ [Examples](examples.md)
