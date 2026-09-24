set(srcs "")

if(CONFIG_DAC_ADAU1701)
    if(CONFIG_DAC_CONTROLS_VOLUME)
        if(CONFIG_ADAU1701_VOLUME_PARAM_ADDR LESS 0)
            message(FATAL_ERROR
                "CONFIG_ADAU1701_VOLUME_PARAM_ADDR must be set to 0..1023 "
                "when ADAU1701 hardware volume control is enabled")
        endif()
        list(APPEND srcs "dac_adau1701.c")
    endif()
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES dac
    PRIV_REQUIRES driver esp_driver_i2c
)
