---
layout: default
title: "🔌 Hardware Setup"
description: "Wiring, SPI configuration, power supply, and decoupling for the ADS7952"
parent: "📚 Documentation"
nav_order: 3
permalink: /docs/hardware_setup/
---

# 🔌 Hardware Setup

![ADS7952 12-channel SAR ADC: Auto-1 sequencing and 16-bit SPI frame layout](assets/ads7952-frame.svg)

This guide covers the physical connections between the ADS7952 and your microcontroller, power supply requirements, and SPI bus configuration.

---

## ADS7952 Pin Overview

The ADS7952 comes in a 32-pin TQFP package. Key pins for driver operation:

| Pin | Function | Description |
|-----|----------|-------------|
| SCLK | SPI Clock | Serial clock input (up to 20 MHz) |
| SDI (DIN) | SPI MOSI | Serial data input — 16-bit command frames |
| SDO (DOUT) | SPI MISO | Serial data output — 16-bit response frames |
| CS̄ | Chip Select | Active-low chip select |
| AVDD | Analog Supply | Analog power supply (VA): 2.7–5.25 V |
| DVDD | Digital Supply | Digital power supply: 2.7 V or AVDD |
| AGND | Analog Ground | Analog ground reference |
| DGND | Digital Ground | Digital ground reference |
| REFP | Reference+ | External positive voltage reference input |
| REFM | Reference− | Reference ground (connect to AGND) |
| CH0–CH11 | Analog Inputs | 12 single-ended analog input channels |
| GPIO0–GPIO3 | Digital I/O | 4 configurable GPIO / alarm output pins |

---

## SPI Connection

### ESP32-S3 Wiring (HardFOC-V1)

| ADS7952 Pin | ESP32-S3 Pin | Signal |
|-------------|-------------|--------|
| SCLK | GPIO 12 | SPI Clock |
| SDI (DIN) | GPIO 11 | MOSI |
| SDO (DOUT) | GPIO 13 | MISO |
| CS̄ | GPIO 10 | Chip Select |
| DVDD | 3.3 V | Digital power |
| DGND | GND | Digital ground |

> ⚠️ Pin assignments are project-specific. The above match the HardFOC-V1 board configuration.
> Edit `esp32_ads7952_test_config.hpp` to match your hardware.

### Generic ESP32 Wiring

| ADS7952 Pin | ESP32 Pin | Signal |
|-------------|-----------|--------|
| SCLK | GPIO 18 | SPI Clock |
| SDI (DIN) | GPIO 23 | MOSI |
| SDO (DOUT) | GPIO 19 | MISO |
| CS̄ | GPIO 5 | Chip Select |

---

## SPI Configuration

| Parameter | Value | Notes |
|-----------|-------|-------|
| **SPI Mode** | Mode 0 (CPOL=0, CPHA=0) | Data sampled on rising edge |
| **Max Clock Speed** | 20 MHz | Datasheet absolute max |
| **Frame Size** | 16 bits | Full-duplex; simultaneous TX/RX |
| **Bit Order** | MSB first | DI[15] transmitted first |
| **CS̄ Active** | Low | Must go low before SCLK activity |
| **CS̄ Framing** | Per-frame | CS̄ must toggle between each 16-bit frame |

> 💡 **CS̄ framing is critical.** Each 16-bit SPI frame must be bracketed by CS̄ going low then high. The ADS7952 uses the CS̄ falling edge to latch the command and the rising edge to start the conversion.

---

## Power Supply Requirements

### Voltage Rails

| Supply | Pin | Range | Typical | Description |
|--------|-----|-------|---------|-------------|
| **VA (AVDD)** | AVDD | 2.7–5.25 V | 5.0 V | Analog supply — determines max input voltage |
| **VD (DVDD)** | DVDD | 2.7 V or AVDD | 3.3 V | Digital supply |
| **REFP** | REFP | 0.1–VA V | 2.5 V | External reference — directly sets ADC accuracy |

### Reference Voltage

The ADS7952 requires an external voltage reference on the REFP pin. Recommended references:

| Reference IC | Output | Accuracy | Notes |
|-------------|--------|----------|-------|
| **REF5025** | 2.500 V | ±0.05% | TI precision reference (recommended) |
| **REF3025** | 2.500 V | ±0.2% | Lower cost option |
| **LM4040-2.5** | 2.500 V | ±0.1% | Shunt reference |

> ⚠️ **Reference quality directly impacts ADC accuracy.** A noisy or inaccurate reference will appear as measurement error. Use a precision reference with a low-ESR decoupling capacitor.

### Input Range

| Range Setting | Input Span | `SetRange()` | Active Reference |
|---------------|-----------|--------------|-----------------|
| **Vref** (default) | 0 to REFP | `Range::Vref` | Vref |
| **2×Vref** | 0 to 2×REFP | `Range::TwoVref` | min(2×Vref, VA) |

When using `Range::TwoVref`, the driver automatically clamps the active reference to `min(2 × vref, va)` to prevent exceeding the analog supply.

---

## Decoupling Recommendations

| Supply | Capacitor | Placement |
|--------|-----------|-----------|
| AVDD | 100 nF ceramic + 10 µF tantalum | As close to pin as possible |
| DVDD | 100 nF ceramic | As close to pin as possible |
| REFP | 100 nF ceramic + 10 µF tantalum | Directly at reference pin |
| REFM | Connect to AGND | — |

### Layout Guidelines

- Place all decoupling capacitors **on the same PCB layer** as the ADS7952, directly adjacent to the supply pins
- Use a solid ground plane under the ADC and reference circuit
- Route analog inputs away from digital/SPI traces
- Keep the REFP trace short and shielded from noise sources

---

## GPIO Pin Connections (Optional)

The ADS7952 has 4 GPIO pins that can be configured by the driver:

| GPIO | Default | Configurable As |
|------|---------|----------------|
| GPIO0 | Digital I/O | General I/O, High alarm output, Combined hi/lo alarm output |
| GPIO1 | Digital I/O | General I/O, High alarm output, Low alarm output |
| GPIO2 | Digital I/O | General I/O, External range input |
| GPIO3 | Digital I/O | General I/O, External power-down input |

If you don't need GPIO/alarm features, these pins can be left unconnected.

---

## ⚠️ Important Notes

1. **Level Shifting**: If your MCU runs at 3.3 V and DVDD is set to 5 V, use level shifters on SPI lines. If DVDD = 3.3 V, direct 3.3 V logic connections are safe.

2. **Analog Input Protection**: Inputs must stay within **AGND − 0.3 V** to **AVDD + 0.3 V**. Use clamping diodes or series resistors if the input source can exceed this range.

3. **First Conversion Invalid**: After power-up, the first ADC conversion is invalid per the datasheet. `EnsureInitialized()` handles this automatically.

4. **Ground Connections**: Connect AGND and DGND together at a **single point** near the device to avoid ground loops.

5. **SPI Bus Sharing**: The ADS7952 can share an SPI bus with other devices. Ensure proper CS̄ management — the driver does not manage CS̄ internally.

---

## Typical Schematic

```
                    ┌──────────────┐
       MOSI ───────┤ SDI     AVDD ├──── 5.0V + 100nF ∥ 10µF
       MISO ───────┤ SDO     DVDD ├──── 3.3V + 100nF
       SCLK ───────┤ SCLK    AGND ├──── GND
       CS   ───────┤ CS̄      DGND ├──── GND
                    │              │
    Analog 0 ──────┤ CH0     REFP ├──── 2.5V ref + 100nF ∥ 10µF
    Analog 1 ──────┤ CH1     REFM ├──── GND
       ...         │  ...         │
    Analog 11 ─────┤ CH11   GPIO0 ├──── (optional)
                    │       GPIO1 ├──── (optional)
                    │       GPIO2 ├──── (optional)
                    │       GPIO3 ├──── (optional)
                    └──────────────┘
```

---

**Navigation**
⬅️ [Quick Start](quickstart.md)  ➡️ [Platform Integration](platform_integration.md)
