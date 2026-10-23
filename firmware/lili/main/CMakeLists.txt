# the world's own sources + the pocket-tank port it reuses unchanged (the display)
set(PT ${CMAKE_CURRENT_LIST_DIR}/../../main)
set(LILI ${CMAKE_CURRENT_LIST_DIR}/../../../common/lili)
idf_component_register(
    SRCS lili_main.c
         ${LILI}/lili_world.c ${LILI}/lili_demo.c ${LILI}/render8.c ${LILI}/palette.c ${LILI}/palette_gen.c ${LILI}/pak.c ${LILI}/mask.c ${LILI}/maps_gen.c ${LILI}/events.c
         ${PT}/display_port_sh8601.c
    INCLUDE_DIRS "." ${PT} ${CMAKE_CURRENT_LIST_DIR}/../../../common ${LILI}
    REQUIRES esp_driver_spi esp_driver_i2c esp_driver_gpio esp_lcd nvs_flash esp_timer esp_psram esp_hw_support
)
# the painted world's pack (tools/pack_assets.py) inside the app image: read in place from flash, no partition
# (1.5 MB of the 2.5 MB app slot; the model partition at 0x290000 stays untouched)
target_add_binary_data(${COMPONENT_TARGET} "${CMAKE_CURRENT_LIST_DIR}/../../../assets/build/assets.pak" BINARY)
target_compile_definitions(${COMPONENT_LIB} PRIVATE TANK_SQUARE)
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -ffast-math)
