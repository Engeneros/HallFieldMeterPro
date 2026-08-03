# SPDX-License-Identifier: Apache-2.0
#cmake_minimum_required(VERSION 3.20.0)
#find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
#project(blinky)
#target_sources(app PRIVATE src/main.c)


cmake_minimum_required(VERSION 3.20.0)
# Указываем плату по умолчанию для этого проекта
set(BOARD nucleo_f756zg)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(HallFM)
#target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE
    src/main.cpp
    src/board_devs.cpp
 #   src/leds/leds.c
    src/leds/gpo.cpp
    src/adc/ads1234.cpp
    src/adc/adc_sequencer.cpp
    src/time/time.cpp
    src/modbus/mbs.cpp
    src/console/console.cpp
    src/mem/mem.cpp
)
# Добавляем ПАПКИ к хидерам, а не сами файлы!
target_include_directories(app PRIVATE 
    src              # Здесь лежит board_devs.hpp
    src/leds         # Здесь лежат leds.h и gpo.hpp
    src/adc          # Здесь лежит adc.hpp
    src/time
    src/modbus
    src/console
    src/mem
)


