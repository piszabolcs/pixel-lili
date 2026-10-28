# the world's own sources + the pocket-tank port it reuses unchanged (the display)
set(PT ${CMAKE_CURRENT_LIST_DIR}/../../main)
set(LILI ${CMAKE_CURRENT_LIST_DIR}/../../../common/lili)
idf_component_register(
    SRCS lili_main.c touch_216.c body_216.c ${CMAKE_CURRENT_LIST_DIR}/../../diag/main/mic_es7210.c ${CMAKE_CURRENT_LIST_DIR}/../../diag/main/whistle.c ${CMAKE_CURRENT_LIST_DIR}/../../diag/main/imu_qmi.c ${CMAKE_CURRENT_LIST_DIR}/../../diag/main/rtc_pcf.c ${CMAKE_CURRENT_LIST_DIR}/../../../common/lili/motion.c ${CMAKE_CURRENT_LIST_DIR}/../../../common/lili/ui.c ${CMAKE_CURRENT_LIST_DIR}/../../../common/lili/font5x9.c ${PT}/battery_port_axp2101.c ${PT}/../components/tankcore/rtc_port_pcf85063.c
         ${LILI}/lili_world.c ${LILI}/lili_demo.c ${LILI}/nav.c ${LILI}/gesture.c ${LILI}/plan.c ${LILI}/toys.c ${LILI}/pose.c ${LILI}/schema_gen.c ${LILI}/life.c ${LILI}/save.c ${LILI}/brain.c ${LILI}/brain_rules_gen.c ${LILI}/brain_llm.c ${CMAKE_CURRENT_LIST_DIR}/../../../common/llm/q4_model.c ${LILI}/render8.c ${LILI}/palette.c ${LILI}/palette_gen.c ${LILI}/pak.c ${LILI}/mask.c ${LILI}/maps_gen.c ${LILI}/events.c
         ${PT}/display_port_sh8601.c
    INCLUDE_DIRS "." ${PT} ${CMAKE_CURRENT_LIST_DIR}/../../diag/main ${PT}/../components/tankcore ${CMAKE_CURRENT_LIST_DIR}/../../../common/llm ${CMAKE_CURRENT_LIST_DIR}/../../../common ${LILI}
    REQUIRES esp_driver_spi esp_driver_i2c esp_driver_i2s esp_driver_usb_serial_jtag esp_driver_gpio esp_lcd nvs_flash esp_timer esp_psram esp_hw_support esp_partition
)
# the painted world's pack (tools/pack_assets.py) is not in the app image any more (2026-10-27): with the M3.5
# animations it outgrows the 2.5 MB app slot, so it has its own "assets" partition (partitions.csv, spec 5.10)
target_compile_definitions(${COMPONENT_LIB} PRIVATE TANK_SQUARE)
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -ffast-math)
