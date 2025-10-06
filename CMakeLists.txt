# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.17)

set(PICO_BOARD pico_w)
set(FAMILY pico_w)

include(pico-sdk/pico_sdk_init.cmake)

project(USBLink VERSION 1.0.0)

pico_sdk_init()

add_executable(USBLink main.c user_gpio.c uart_bridge.c usb_descriptors.c)

target_include_directories(USBLink PUBLIC
	./
	pico-sdk/lib/tinyusb/src)

target_link_libraries(USBLink
	hardware_flash
	pico_multicore
	pico_stdlib
	tinyusb_device
    pico_cyw43_arch_none)
    
pico_add_extra_outputs(USBLink)
