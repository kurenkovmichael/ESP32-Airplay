#include "iot_board.h"

#include "dac.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings.h"

#if defined(CONFIG_DAC_ADAU1701) && defined(CONFIG_DAC_CONTROLS_VOLUME)
#include "dac_adau1701.h"
#endif

static const char TAG[] = "esp32wrover-adau1701";
static bool s_initialized;
static i2c_master_bus_handle_t s_i2c_bus;

const char *iot_board_get_info(void) {
  return BOARD_NAME;
}

bool iot_board_is_init(void) {
  return s_initialized;
}

board_res_handle_t iot_board_get_handle(int id) {
  return id == BOARD_I2C_DAC_ID ? (board_res_handle_t)s_i2c_bus : NULL;
}

esp_err_t iot_board_init(void) {
  if (s_initialized) {
    return ESP_OK;
  }

#if defined(CONFIG_DAC_ADAU1701) && defined(CONFIG_DAC_CONTROLS_VOLUME)
  // ADAU1701 owns this bus while self-booting from EEPROM. Do not install the
  // ESP32 master driver until the documented initialization window has ended.
  vTaskDelay(pdMS_TO_TICKS(CONFIG_ADAU1701_SELFBOOT_DELAY_MS));

  i2c_master_bus_config_t cfg = {
      .i2c_port = BOARD_I2C_PORT,
      .sda_io_num = BOARD_I2C_SDA_GPIO,
      .scl_io_num = BOARD_I2C_SCL_GPIO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  esp_err_t err = i2c_new_master_bus(&cfg, &s_i2c_bus);
  if (err != ESP_OK) {
    return err;
  }

  dac_register(&dac_adau1701_ops);
  err = dac_init(s_i2c_bus);
  if (err != ESP_OK) {
    i2c_del_master_bus(s_i2c_bus);
    s_i2c_bus = NULL;
    return err;
  }

  float volume_db;
  if (settings_get_volume(&volume_db) == ESP_OK) {
    dac_set_volume(volume_db);
  }
  ESP_LOGI(TAG, "ADAU1701 I2C volume enabled: SDA=%d SCL=%d",
           BOARD_I2C_SDA_GPIO, BOARD_I2C_SCL_GPIO);
#else
  ESP_LOGI(TAG, "ESP32 software volume enabled");
#endif

  s_initialized = true;
  return ESP_OK;
}

esp_err_t iot_board_deinit(void) {
  if (!s_initialized) {
    return ESP_OK;
  }
#if defined(CONFIG_DAC_ADAU1701) && defined(CONFIG_DAC_CONTROLS_VOLUME)
  dac_deinit();
  if (s_i2c_bus != NULL) {
    i2c_del_master_bus(s_i2c_bus);
    s_i2c_bus = NULL;
  }
#endif
  s_initialized = false;
  return ESP_OK;
}
