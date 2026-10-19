# Pixel Lili diagnostic firmware (M1.4-M1.8) for the Waveshare ESP32-S3-Touch-AMOLED-2.16.
# Build (WSL):  idf.py -B ~/build/lili-diag build        (tools/build_diag.sh does it and copies the artefacts)
# Flash (Win):  py -3.12 tools/flash_216.py diag          (docs/STATUS.md)
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lili_diag)
