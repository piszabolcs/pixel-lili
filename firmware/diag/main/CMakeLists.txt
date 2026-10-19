# the diag's own sources + the pocket-tank ports it reuses unchanged (display, PMIC, ES8311 codec)
set(PT ${CMAKE_CURRENT_LIST_DIR}/../../main)
set(COMMON ${CMAKE_CURRENT_LIST_DIR}/../../../common)
idf_component_register(
    SRCS diag_main.c gfx.c font5x9.c mic_es7210.c whistle.c imu_qmi.c rtc_pcf.c powerlog.c
         ${PT}/display_port_sh8601.c ${PT}/battery_port_axp2101.c ${PT}/codec_port.c
    INCLUDE_DIRS "." ${PT} ${COMMON}
    REQUIRES esp_driver_spi esp_driver_i2c esp_driver_i2s esp_driver_gpio esp_driver_usb_serial_jtag esp_lcd nvs_flash esp_timer esp_psram
)
# the square's world: display_port_sh8601.c keys its 480 x 480 path on it (common/tank.h gives TANK_W/H)
target_compile_definitions(${COMPONENT_LIB} PRIVATE TANK_SQUARE)
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -ffast-math)
