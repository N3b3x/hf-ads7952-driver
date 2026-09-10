---
layout: default
title: "🐛 Troubleshooting"
description: "Common issues and solutions for the HF-ADS7952 driver"
parent: "📚 Documentation"
nav_order: 9
permalink: /docs/troubleshooting/
---

# 🐛 Troubleshooting

Common issues and their solutions when working with the HF-ADS7952 driver.

---

## 📋 Error Codes Quick Reference

The driver returns `Error` enum values from most operations:

| Error Code | Meaning | Common Cause |
|-----------|---------|--------------|
| `Error::None` | Success | — |
| `Error::NotInitialized` | `EnsureInitialized()` not called | Forgot to call `adc.EnsureInitialized()` before reading |
| `Error::InvalidChannel` | Channel ≥ 12 | Passing channel number 12 or higher |
| `Error::SpiError` | SPI transfer failed | `transfer()` returned non-zero |
| `Error::ProgrammingFailed` | Multi-frame programming failed | SPI error during `ProgramAlarm()` or `ProgramAuto1Channels()` |
| `Error::ModeChangeFailed` | Mode change failed | SPI error during `EnterAuto1Mode()` etc. |
| `Error::Timeout` | Operation timed out | — |

Always check return values:

```cpp
auto result = adc.ReadChannel(0);
if (!result.ok()) {
    printf("Error code: %u\n", static_cast<uint8_t>(result.error));
}
```

---

## SPI Communication Issues

### All readings return 0 counts

**Possible causes:**
- SPI MISO line not connected or broken
- Chip select (CS) not toggling correctly — the ADS7952 requires CS to frame each 16-bit transfer
- Device not powered (AVDD/DVDD rail down)

**Solutions:**
1. Verify wiring with a multimeter or logic analyzer
2. Check that CS goes low for exactly one 16-bit transfer, then returns high
3. Verify AVDD supply voltage (2.7–5.25 V) and DVDD
4. Use a logic analyzer to capture MOSI/MISO — valid MISO data starts as `4'b[channel_addr] + 12'b[data]`

---

### All readings return 0xFFF (4095 counts)

**Possible causes:**
- SPI bus not initialized
- MISO line pulled high (no device responding)
- Wrong SPI mode — ADS7952 requires **Mode 0** (CPOL=0, CPHA=0)
- Input voltage above the selected range

**Solutions:**
1. Ensure SPI bus is initialized before calling `adc.EnsureInitialized()`
2. Check physical connections to SDO (MISO) pin
3. Verify SPI Mode 0 in your platform config
4. Check `Range` setting — if `Vref`, input must be 0–Vref

---

### Inconsistent or noisy readings

**Possible causes:**
- Poor analog reference quality
- Missing or insufficient decoupling capacitors
- Long SPI traces picking up noise
- Input signal exceeds ADC range
- Sampling during input transitions

**Solutions:**
1. Use a precision voltage reference (e.g., REF5025) with proper decoupling
2. Add 100 nF ceramic + 10 µF bulk caps close to AVDD and REFP pins
3. Keep SPI traces short, use ground plane under analog signals
4. Ensure analog inputs stay within 0 to Vref (or 0 to 2×Vref if `Range::TwoVref`)
5. Consider averaging multiple readings for higher effective resolution

---

### First reading after power-up is wrong

**Expected behavior.** The ADS7952 datasheet (SLAS605C, Section 7.3) states that the first conversion result after power-up is invalid. The driver's `EnsureInitialized()` method discards this frame automatically.

**If you still see bad data:** Ensure you are calling `adc.EnsureInitialized()` before any `ReadChannel()` or `ReadAllChannels()` calls.

---

## SPI Pipeline Issues

### Channel address in response doesn't match request

This is normal. The ADS7952 uses a **pipelined** SPI protocol: a channel
selected in frame N is sampled on the CS edge that opens frame N+2, so its
conversion result is returned in frame **N+2** (SLAS605C Figure 51). Frame
N+1 still returns the channel selected two frames earlier.

The driver keys every result on the DO15:12 channel address and clocks
CONTINUE frames until the requested address appears (bounded by
`ADS7952_CFG::MANUAL_READ_MAX_FRAMES`).

If you are using raw SPI (bypassing the driver), you must handle this pipeline yourself.

### Every channel returns the same count

Look at the raw words before touching spans or clocks. Two very different
faults print the same converted bank:

| DO15:12 (address) | DO11:0 (data) | Meaning |
|---|---|---|
| advances 0..11 | identical | Digital side fine; sample-and-hold never charged to the new channel. Acquisition runs from the 14th SCLK rising edge to the next CS falling edge, so lower SCLK / widen the CS gap / check source impedance. |
| **stuck** (never equals the selected channel) | identical | The device is not receiving SDI (or is unpowered). A manual select of CH11 must come back with address 11 by frame N+2. If it never does, MISO is returning a pattern that is not this device's — check the SDI wire, the board rails, then MISO. |

The old two-frame `ReadChannel` did not check the address and would report
the stuck word as every channel; the driver now keys on the address and
returns `Error::Timeout` instead. Seen 2026-09-09 on a Portenta Mid +
Moonshine sensor board: every frame `0x0E83` with address 0 regardless of the
channel selected, and the AT25040 on the same SDI returned a different
"identity" on every read — a dead SDI/harness, not a driver or SCLK-rate fault.

---

### `ReadAllChannels()` missing channels

If `ChannelReadings.hasChannel(ch)` returns false for channels you expect:

1. **Check the Auto-1 mask:** Only channels in the programmed mask are sequenced. Call `ProgramAuto1Channels(0x0FFF)` for all 12 channels.
2. **Check `validMask`:** `ChannelReadings.validMask` is a bitmask of channels that were actually read. The number of SPI transfers equals `popcount(auto1_mask) + 1`.
3. **Ensure you called `EnterAuto1Mode(true)`:** The `true` argument resets the sequence pointer to the first channel.

---

## Build Issues

### CMake: Target `hf::ads7952` not found

```
CMake Error: ... does not define target "hf::ads7952"
```

**Solution:** Ensure you have added the library before linking:

```cmake
add_subdirectory(path/to/hf-ads7952-driver)
target_link_libraries(your_target PRIVATE hf::ads7952)
```

---

### Compiler error: C++20 required

```
error: 'cxx_std_20' is not a valid compile feature
```

**Solution:** Update your compiler to one that supports C++20:
- GCC 10+ (ESP-IDF v5.x uses GCC 13)
- Clang 10+
- MSVC 19.29+

---

### ESP-IDF: Component not found

```
CMake Error: ... could not find component hf_ads7952
```

**Solution:** Ensure the component wrapper exists:

```
your_project/
└── components/
    └── hf_ads7952/
        ├── CMakeLists.txt
        └── idf_component.yml
```

The `CMakeLists.txt` must include the build settings file:

```cmake
include(${COMPONENT_DIR}/../../cmake/hf_ads7952_build_settings.cmake)

idf_component_register(
    SRCS ${HF_ADS7952_SOURCE_FILES}
    INCLUDE_DIRS ${HF_ADS7952_PUBLIC_INCLUDE_DIRS}
    REQUIRES ${HF_ADS7952_IDF_REQUIRES}
)
```

---

### Windows: MAX_PATH build failure

```
fatal error: ... file name too long
```

If your workspace path is deeply nested (e.g., inside OneDrive), the full build path can exceed Windows' 260-character limit.

**Solutions:**
1. Clone the repository to a shorter base path (e.g., `C:\dev\`)
2. Enable Windows long path support: `reg add HKLM\SYSTEM\CurrentControlSet\Control\FileSystem /v LongPathsEnabled /t REG_DWORD /d 1`
3. Use `subst` to map a deep path to a drive letter

---

## Runtime Issues

### `EnsureInitialized()` returns false

**Possible causes:**
- SPI `transfer()` method returns non-zero (error)
- SPI bus not configured before driver initialization

**Debugging:**
1. Add logging inside your `transfer()` implementation to see the return code
2. Verify the SPI bus is fully initialized (clock, pins, device handle) before calling `EnsureInitialized()`
3. Check that CS is properly toggled per transfer

---

### Wrong channel data in Manual mode

The driver's `ReadChannel()` handles the two-frame pipeline automatically. If you still see incorrect data:

1. Verify you are reading `result.count` and `result.voltage`, not the raw SPI return
2. Check `result.channel` — it should match the requested channel
3. Ensure no other code is sending SPI traffic to the ADS7952 between driver calls

---

### Alarm thresholds not triggering

`ProgramAlarm()` is a **3-frame** SPI sequence. If any frame fails, the threshold is not written.

**Debugging:**
1. Check the return value: `auto err = adc.ProgramAlarm(ch, bound, threshold); if (err != Error::None) ...`
2. Verify the threshold value is a 12-bit count (0–4095), not a voltage
3. Convert voltage to counts: `threshold = (voltage / vref) * 4095`
4. GPIO0/GPIO1 must be configured for alarm output mode via `ProgramGPIO()` to see alarm signals on pins

---

### GPIO pins not responding

1. Confirm GPIO direction: set bit in `GPIOConfig.direction_mask` (1 = output) for each pin
2. GPIO0 and GPIO1 can be alarm outputs — check `GPIOConfig.alarm_mode`
3. Call `ProgramGPIO()` before `SetGPIOOutputs()`
4. GPIO3 doubles as the power-down input when configured — check `GPIOConfig.gpio3_is_powerdown`

---

## Debugging Techniques

### Logic Analyzer Checklist

| Signal | What to Check |
|--------|--------------|
| CS | Goes low for exactly 16 SCLK cycles per transfer |
| SCLK | Mode 0 — idle low, data sampled on rising edge |
| MOSI | DI[15:12] shows mode bits, DI[11] shows PROGRAM |
| MISO | DO[15:12] shows channel address, DO[11:0] shows 12-bit data |

### Voltage Conversion Sanity Check

```cpp
// Static conversion (no driver instance needed)
float v = ads7952::ADS7952<MySpi>::CountToVoltage(2048, 2.5f, ads7952::Range::Vref);
// Should be ~1.25 V (midscale)

float v2 = ads7952::ADS7952<MySpi>::CountToVoltage(2048, 2.5f, ads7952::Range::TwoVref);
// Should be ~2.50 V (midscale of 0–5 V range)
```

### ESP-IDF SPI Debugging

Enable SPI driver logging:

```
idf.py menuconfig
# → Component config → Log output → Default log verbosity: Debug
```

Or set per-tag in code:

```cpp
esp_log_level_set("spi_master", ESP_LOG_DEBUG);
```

---

## Getting Help

If your issue is not listed here:

1. Check the [ADS7952 datasheet](datasheet/) (SLAS605C) for device-specific behavior
2. Use a logic analyzer to capture and decode SPI traffic
3. Run the [Integration Test Suite](examples.md#4-integration-test-suite) to validate hardware
4. Open an issue on the [GitHub repository](https://github.com/N3b3x/hf-ads7952-driver/issues)

---

**Navigation**
⬅️ [Examples](examples.md)  ⬅️ [Documentation Home](index.md)
