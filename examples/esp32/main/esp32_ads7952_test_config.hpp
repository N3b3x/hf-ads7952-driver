/**
 * @file esp32_ads7952_test_config.hpp
 * @brief Hardware configuration for ADS7952 driver on ESP32
 * @ingroup ads7952_examples_support
 *
 * @details
 * This file contains the hardware and runtime configuration used by the ESP32
 * transport and example applications. Modify these values to match your board.
 */

#pragma once

#include <cstdint>

//==============================================================================
// COMPILE-TIME CONFIGURATION FLAGS
//==============================================================================

/**
 * @brief Enable detailed SPI transaction logging.
 *
 * When enabled (set to 1), Esp32Ads7952SpiBus logs per-transfer TX/RX bytes.
 * Set to 0 to reduce runtime log volume.
 */
#ifndef ESP32_ADS7952_ENABLE_DETAILED_SPI_LOGGING
#define ESP32_ADS7952_ENABLE_DETAILED_SPI_LOGGING 1
#endif

namespace ADS7952_TestConfig {

/**
 * @brief SPI Configuration for ESP32
 *
 * These pins are used for SPI communication with the ADS7952.
 * Ensure your hardware matches these pin assignments or modify accordingly.
 */
struct SPIPins {
    static constexpr uint8_t MISO = 19;          ///< GPIO19 - SPI MISO (Master In Slave Out)
    static constexpr uint8_t MOSI = 23;          ///< GPIO23 - SPI MOSI (Master Out Slave In)
    static constexpr uint8_t SCLK = 18;          ///< GPIO18 - SPI Clock (SCLK)
    static constexpr uint8_t CS   = 5;           ///< GPIO5  - Chip Select (active low)
};

/**
 * @brief SPI Communication Parameters
 *
 * The ADS7952 supports SPI frequencies up to 20MHz.
 *
 * SPI Mode: CPOL=0, CPHA=0 (Mode 0)
 * Data format: 16-bit frames
 */
struct SPIParams {
    static constexpr uint32_t FREQUENCY = 4000000;    ///< 4MHz SPI frequency (conservative default)
    static constexpr uint8_t MODE = 0;                ///< SPI Mode 0 (CPOL=0, CPHA=0)
    static constexpr uint8_t QUEUE_SIZE = 1;          ///< Transaction queue size
    static constexpr uint8_t CS_ENA_PRETRANS = 1;     ///< CS asserted N clock cycles before transaction
    static constexpr uint8_t CS_ENA_POSTTRANS = 1;    ///< CS held N clock cycles after transaction
    /// SPI host: 2 = SPI2_HOST, 3 = SPI3_HOST
    static constexpr uint8_t SPI_HOST_ID = 2;
};

/**
 * @brief ADC Specifications
 *
 * ADS7952 is a 12-channel, 12-bit SAR ADC.
 */
struct ADCSpecs {
    static constexpr uint8_t  NUM_CHANNELS    = 12;       ///< Number of analog input channels
    static constexpr uint8_t  RESOLUTION_BITS = 12;       ///< ADC resolution in bits
    static constexpr uint16_t MAX_COUNT       = 4095;     ///< Maximum ADC count (2^12 - 1)
    static constexpr float    VREF            = 2.5f;     ///< Reference voltage (V)
};

} // namespace ADS7952_TestConfig
