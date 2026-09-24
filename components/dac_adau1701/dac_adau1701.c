#include "dac_adau1701.h"

#include <math.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#define ADAU1701_I2C_ADDR       0x34
#define ADAU1701_I2C_SPEED_HZ   400000
#define ADAU1701_I2C_TIMEOUT_MS 100

#define ADAU1701_SAFELOAD_DATA0 0x0810
#define ADAU1701_SAFELOAD_ADDR0 0x0815
#define ADAU1701_CORE_CONTROL   0x081C
#define ADAU1701_CORE_IST       (1U << 5)

static const char TAG[] = "ADAU1701";
static i2c_master_dev_handle_t s_device;
static SemaphoreHandle_t s_mutex;

static esp_err_t adau1701_write(uint16_t reg, const uint8_t *data, size_t len) {
  if (s_device == NULL || len > 5) {
    return ESP_ERR_INVALID_STATE;
  }
  uint8_t buf[7] = {(uint8_t)(reg >> 8), (uint8_t)reg};
  for (size_t i = 0; i < len; i++) {
    buf[i + 2] = data[i];
  }
  return i2c_master_transmit(s_device, buf, len + 2, ADAU1701_I2C_TIMEOUT_MS);
}

static esp_err_t adau1701_read(uint16_t reg, uint8_t *data, size_t len) {
  if (s_device == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  const uint8_t addr[] = {(uint8_t)(reg >> 8), (uint8_t)reg};
  return i2c_master_transmit_receive(s_device, addr, sizeof(addr), data, len,
                                     ADAU1701_I2C_TIMEOUT_MS);
}

static esp_err_t adau1701_safeload(uint16_t param_addr, uint32_t value_5_23) {
  const uint8_t addr_data[] = {(uint8_t)(param_addr >> 8), (uint8_t)param_addr};
  // Safeload registers are 40 bits wide. A 28-bit parameter is represented as
  // the same zero-padded 32-bit 5.23 word used by Parameter RAM, preceded by
  // one zero byte.
  const uint8_t value_data[] = {
      0, (uint8_t)(value_5_23 >> 24), (uint8_t)(value_5_23 >> 16),
      (uint8_t)(value_5_23 >> 8), (uint8_t)value_5_23};
  uint8_t core[2];

  esp_err_t err =
      adau1701_write(ADAU1701_SAFELOAD_ADDR0, addr_data, sizeof(addr_data));
  if (err != ESP_OK) {
    return err;
  }
  err = adau1701_write(ADAU1701_SAFELOAD_DATA0, value_data, sizeof(value_data));
  if (err != ESP_OK) {
    return err;
  }
  err = adau1701_read(ADAU1701_CORE_CONTROL, core, sizeof(core));
  if (err != ESP_OK) {
    return err;
  }
  core[1] |= ADAU1701_CORE_IST;
  return adau1701_write(ADAU1701_CORE_CONTROL, core, sizeof(core));
}

static esp_err_t adau1701_init(void *i2c_bus) {
  i2c_master_bus_handle_t bus = (i2c_master_bus_handle_t)i2c_bus;
  if (bus == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_mutex == NULL) {
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
      return ESP_ERR_NO_MEM;
    }
  }

  esp_err_t err =
      i2c_master_probe(bus, ADAU1701_I2C_ADDR, ADAU1701_I2C_TIMEOUT_MS);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ADAU1701 not found at 7-bit address 0x%02X: %s",
             ADAU1701_I2C_ADDR, esp_err_to_name(err));
    return err;
  }
  i2c_device_config_t cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = ADAU1701_I2C_ADDR,
      .scl_speed_hz = ADAU1701_I2C_SPEED_HZ,
  };
  err = i2c_master_bus_add_device(bus, &cfg, &s_device);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "I2C control ready, volume parameter address=%d",
             CONFIG_ADAU1701_VOLUME_PARAM_ADDR);
  }
  return err;
}

static esp_err_t adau1701_deinit(void) {
  esp_err_t err = ESP_OK;
  if (s_device != NULL) {
    err = i2c_master_bus_rm_device(s_device);
    s_device = NULL;
  }
  if (s_mutex != NULL) {
    vSemaphoreDelete(s_mutex);
    s_mutex = NULL;
  }
  return err;
}

static void adau1701_set_volume(float volume_db) {
  if (s_device == NULL || s_mutex == NULL) {
    return;
  }

  // Match the ESP32 software-volume curve: AirPlay -30 dB is mute, 0 dB is
  // unity, with a squared linear ramp between them.
  float gain;
  if (volume_db <= -30.0f) {
    gain = 0.0f;
  } else if (volume_db >= 0.0f) {
    gain = 1.0f;
  } else {
    float normalized = (volume_db + 30.0f) / 30.0f;
    gain = normalized * normalized;
  }
  uint32_t value = (uint32_t)lroundf(gain * 8388608.0f);

  xSemaphoreTake(s_mutex, portMAX_DELAY);
  esp_err_t err = adau1701_safeload(CONFIG_ADAU1701_VOLUME_PARAM_ADDR, value);
  xSemaphoreGive(s_mutex);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Volume safeload failed: %s", esp_err_to_name(err));
  } else {
    ESP_LOGD(TAG, "Volume %.1f dB -> gain %.6f -> 0x%08lx", volume_db, gain,
             (unsigned long)value);
  }
}

const dac_ops_t dac_adau1701_ops = {
    .init = adau1701_init,
    .deinit = adau1701_deinit,
    .set_volume = adau1701_set_volume,
};
