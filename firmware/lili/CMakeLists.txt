# Pixel Lili world firmware (M2.6) for the Waveshare ESP32-S3-Touch-AMOLED-2.16: the platform-free
# core in common/lili on the panel, drawn stripe by stripe (no frame buffer).
# Build (WSL):  tools/build_lili.sh      Flash (Win):  py -3.12 tools/flash_216.py lili
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lili_world)
