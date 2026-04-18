/**
 * @file esp32_ads7952_bus.hpp
 * @brief ESP32 SPI transport implementation for ADS7952 driver (header-only)
 *
 * @details
 * Provides an ESP-IDF transport adapter implementing
 * ads7952::SpiInterface<Esp32Ads7952SpiBus>, intended for use by all ESP32
 * examples and tests in this directory.
 *
 * @ingroup ads7952_examples_support
 */

#pragma once

#include "../../../inc/ads7952_spi_interface.hpp"
#include "esp32_ads7952_test_config.hpp"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

/**
 * @ingroup ads7952_examples_support
 * @class Esp32Ads7952SpiBus
 * @brief ESP32 SPI transport implementation for ADS7952 driver.
 *
 * Implements the CRTP SPI contract expected by ads7952::ADS7952 using ESP-IDF's
 * SPI master driver with configurable host, pins, and timing parameters.
 */
class Esp32Ads7952SpiBus : public ads7952::SpiInterface<Esp32Ads7952SpiBus> {
public:
  /**
   * @ingroup ads7952_examples_support
   * @brief SPI configuration structure.
   *
   * @note For standard usage, prefer CreateEsp32Ads7952SpiBus(), which pulls
   *       values from esp32_ads7952_test_config.hpp.
   */
  struct SPIConfig {
    spi_host_device_t host;      ///< SPI host (e.g., SPI2_HOST)
    gpio_num_t miso_pin;         ///< MISO pin (Master In Slave Out)
    gpio_num_t mosi_pin;         ///< MOSI pin (Master Out Slave In)
    gpio_num_t sclk_pin;         ///< SCLK pin (SPI Clock)
    gpio_num_t cs_pin;           ///< CS pin (Chip Select, active low)
    uint32_t frequency;          ///< SPI frequency in Hz (max 20MHz for ADS7952)
    uint8_t mode;                ///< SPI mode (must be 0: CPOL=0, CPHA=0 for ADS7952)
    uint8_t queue_size;          ///< Transaction queue size
    uint8_t cs_ena_pretrans;     ///< CS asserted N clock cycles before transaction
    uint8_t cs_ena_posttrans;    ///< CS held N clock cycles after transaction
  };

  /**
   * @brief Construct the SPI bus wrapper from a concrete configuration.
   * @param config SPI host/pin/timing configuration to use.
   */
  explicit Esp32Ads7952SpiBus(const SPIConfig &config) : config_(config) {}

  /**
   * @brief Destructor that guarantees SPI resources are released.
   */
  ~Esp32Ads7952SpiBus() { deinitialize(); }

  /**
   * @brief Perform a full-duplex SPI data transfer
   * @param tx Pointer to data to transmit (len bytes). If nullptr, zeros are sent.
   * @param rx Pointer to buffer for received data (len bytes). If nullptr,
   *           received data is ignored.
   * @param len Number of bytes to transfer.
   */
  void transfer(const uint8_t *tx, uint8_t *rx, std::size_t len) {
    if (!initialized_ || spi_device_ == nullptr) {
      ESP_LOGE(TAG, "SPI bus not initialized");
      return;
    }

    spi_transaction_t trans = {};
    trans.length = len * 8; // Length in bits
    trans.tx_buffer = tx;
    trans.rx_buffer = rx;

    esp_err_t ret = spi_device_transmit(spi_device_, &trans);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "SPI transfer failed: %s", esp_err_to_name(ret));
    }

#if ESP32_ADS7952_ENABLE_DETAILED_SPI_LOGGING
    if (tx && rx) {
      ESP_LOGD(TAG, "SPI TX/RX [%d bytes]: TX=0x%02X%02X RX=0x%02X%02X",
               (int)len,
               len >= 1 ? tx[0] : 0, len >= 2 ? tx[1] : 0,
               len >= 1 ? rx[0] : 0, len >= 2 ? rx[1] : 0);
    }
#endif
  }

  /**
   * @brief Initialize the SPI bus and device
   * @return true if successful, false otherwise
   */
  bool initialize() {
    if (initialized_) {
      ESP_LOGW(TAG, "SPI bus already initialized");
      return true;
    }

    // Configure SPI bus
    spi_bus_config_t bus_cfg = {};
    bus_cfg.miso_io_num = config_.miso_pin;
    bus_cfg.mosi_io_num = config_.mosi_pin;
    bus_cfg.sclk_io_num = config_.sclk_pin;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 64;

    esp_err_t ret = spi_bus_initialize(config_.host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
      return false;
    }
    bus_initialized_ = true;

    // Configure SPI device
    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = config_.frequency;
    dev_cfg.mode = config_.mode;
    dev_cfg.spics_io_num = config_.cs_pin;
    dev_cfg.queue_size = config_.queue_size;
    dev_cfg.cs_ena_pretrans = config_.cs_ena_pretrans;
    dev_cfg.cs_ena_posttrans = config_.cs_ena_posttrans;

    ret = spi_bus_add_device(config_.host, &dev_cfg, &spi_device_);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
      spi_bus_free(config_.host);
      bus_initialized_ = false;
      return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "SPI bus initialized: freq=%luHz, mode=%u, CS=GPIO%d",
             config_.frequency, config_.mode, config_.cs_pin);
    return true;
  }

  /**
   * @brief Deinitialize the SPI bus and free resources
   */
  void deinitialize() {
    if (spi_device_) {
      spi_bus_remove_device(spi_device_);
      spi_device_ = nullptr;
    }
    if (bus_initialized_) {
      spi_bus_free(config_.host);
      bus_initialized_ = false;
    }
    initialized_ = false;
  }

  /**
   * @brief Check whether the SPI bus/device pair is ready for transfers.
   * @return true when initialize() completed successfully and not deinitialized.
   */
  bool isInitialized() const { return initialized_; }

private:
  static constexpr const char *TAG = "Esp32Ads7952SpiBus";

  SPIConfig config_;
  spi_device_handle_t spi_device_ = nullptr;
  bool initialized_ = false;
  bool bus_initialized_ = false;
};

/**
 * @ingroup ads7952_examples_support
 * @brief Factory function creating an Esp32Ads7952SpiBus from example config.
 *
 * Uses values from esp32_ads7952_test_config.hpp so all examples/tests share
 * one hardware configuration source.
 *
 * @return Configured SPI bus instance.
 */
inline std::unique_ptr<Esp32Ads7952SpiBus> CreateEsp32Ads7952SpiBus() {
  Esp32Ads7952SpiBus::SPIConfig config;
  config.host = static_cast<spi_host_device_t>(ADS7952_TestConfig::SPIParams::SPI_HOST_ID);
  config.miso_pin = static_cast<gpio_num_t>(ADS7952_TestConfig::SPIPins::MISO);
  config.mosi_pin = static_cast<gpio_num_t>(ADS7952_TestConfig::SPIPins::MOSI);
  config.sclk_pin = static_cast<gpio_num_t>(ADS7952_TestConfig::SPIPins::SCLK);
  config.cs_pin = static_cast<gpio_num_t>(ADS7952_TestConfig::SPIPins::CS);
  config.frequency = ADS7952_TestConfig::SPIParams::FREQUENCY;
  config.mode = ADS7952_TestConfig::SPIParams::MODE;
  config.queue_size = ADS7952_TestConfig::SPIParams::QUEUE_SIZE;
  config.cs_ena_pretrans = ADS7952_TestConfig::SPIParams::CS_ENA_PRETRANS;
  config.cs_ena_posttrans = ADS7952_TestConfig::SPIParams::CS_ENA_POSTTRANS;

  return std::make_unique<Esp32Ads7952SpiBus>(config);
}
