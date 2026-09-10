/**
 * @file ads7952_config.hpp
 * @brief Compile-time configuration options for the ADS7952 driver
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 *
 * Provides sensible defaults that can be overridden via Kconfig (ESP-IDF),
 * CMake definitions, or direct preprocessor defines.
 *
 * @ingroup ads7952_driver
 */
#pragma once
#include <cstdint>

#include "ads7952_types.hpp"

namespace ADS7952_CFG {

/**
 * @defgroup ads7952_config Compile-Time Configuration
 * @ingroup ads7952_driver
 * @brief Build-time defaults and tuning knobs for the ADS7952 driver.
 *
 * These constants define startup behavior (mode, range, channel sequencing),
 * electrical assumptions (Vref/VA), and bounded runtime loop behavior.
 * Values can be overridden from the build system using CONFIG_ADS7952_* macros.
 * @{
 */

// ---- Operating mode default ------------------------------------------------
#ifdef CONFIG_ADS7952_MODE_AUTO1
inline constexpr ads7952::Mode DEFAULT_MODE = ads7952::Mode::Auto1;
#elif defined(CONFIG_ADS7952_MODE_AUTO2)
inline constexpr ads7952::Mode DEFAULT_MODE = ads7952::Mode::Auto2;
#else
inline constexpr ads7952::Mode DEFAULT_MODE = ads7952::Mode::Manual;
#endif

// ---- Input range default ---------------------------------------------------
#ifdef CONFIG_ADS7952_RANGE_2VREF
inline constexpr ads7952::Range DEFAULT_RANGE = ads7952::Range::TwoVref;
#else
inline constexpr ads7952::Range DEFAULT_RANGE = ads7952::Range::Vref;
#endif

// ---- Device constants ------------------------------------------------------
inline constexpr uint8_t  NUM_CHANNELS    = 12;
inline constexpr uint8_t  RESOLUTION_BITS = 12;
inline constexpr uint16_t MAX_COUNT       = (1U << RESOLUTION_BITS) - 1;

// ---- Voltage reference defaults --------------------------------------------
#ifdef CONFIG_ADS7952_VREF_MV
inline constexpr float DEFAULT_VREF = CONFIG_ADS7952_VREF_MV / 1000.0f;
#else
inline constexpr float DEFAULT_VREF = 2.5f;   // REF5025 typical
#endif

#ifdef CONFIG_ADS7952_VA_MV
inline constexpr float DEFAULT_VA = CONFIG_ADS7952_VA_MV / 1000.0f;
#else
inline constexpr float DEFAULT_VA = 5.0f;     // Typical VA supply
#endif

// ---- Voltage reference limits (per ADS7952 datasheet) ---------------------
inline constexpr float MIN_VREF = 1.0f;   // Minimum Vref (datasheet limit)
inline constexpr float MAX_VREF = 2.5f;   // Maximum Vref (datasheet limit)
inline constexpr float MIN_VA   = 2.7f;   // Minimum VA (datasheet absolute min)
inline constexpr float MAX_VA   = 5.5f;   // Maximum VA (datasheet absolute max)

// ---- Auto-1 default channel mask (all 12 channels) ------------------------
#ifdef CONFIG_ADS7952_AUTO1_CHANNEL_MASK
inline constexpr uint16_t DEFAULT_AUTO1_MASK = CONFIG_ADS7952_AUTO1_CHANNEL_MASK;
#else
inline constexpr uint16_t DEFAULT_AUTO1_MASK = 0x0FFF;
#endif

// ---- Auto-2 default last channel -------------------------------------------
#ifdef CONFIG_ADS7952_AUTO2_LAST_CH
inline constexpr uint8_t DEFAULT_AUTO2_LAST_CH = CONFIG_ADS7952_AUTO2_LAST_CH;
#else
inline constexpr uint8_t DEFAULT_AUTO2_LAST_CH = 11;
#endif

// ---- Safety margin frames for ReadAllChannels auto-read loop ---------------
#ifdef CONFIG_ADS7952_READ_ALL_MAX_EXTRA
inline constexpr uint8_t READ_ALL_MAX_EXTRA_FRAMES = CONFIG_ADS7952_READ_ALL_MAX_EXTRA;
#else
inline constexpr uint8_t READ_ALL_MAX_EXTRA_FRAMES = 4;
#endif

// ---- Manual-mode read: CONTINUE frames allowed before the requested channel
//      address appears in DO15:12. SLAS605C Figure 51: select in frame N,
//      data in frame N+2, so the minimum is 2; one spare for a mode change
//      that was still in flight.
#ifdef CONFIG_ADS7952_MANUAL_READ_MAX_FRAMES
inline constexpr uint8_t MANUAL_READ_MAX_FRAMES = CONFIG_ADS7952_MANUAL_READ_MAX_FRAMES;
#else
inline constexpr uint8_t MANUAL_READ_MAX_FRAMES = 4;
#endif

// ---- Max retries for mode change / programming operations ------------------
#ifdef CONFIG_ADS7952_MAX_RETRIES
inline constexpr uint8_t MAX_RETRIES = CONFIG_ADS7952_MAX_RETRIES;
#else
inline constexpr uint8_t MAX_RETRIES = 3;
#endif

/** @} */
} // namespace ADS7952_CFG
