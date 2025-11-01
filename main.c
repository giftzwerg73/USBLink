// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 Marcus Schuster <ms@nixmail.com>
 */

#include <hardware/irq.h>
#include <hardware/structs/sio.h>
#include <hardware/uart.h>
#include <hardware/watchdog.h>
#include <pico/cyw43_arch.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <string.h>
#include <tusb.h>

#include "main_run.h"
#include "uart_bridge.h"
#include "usb_descriptors.h"
#include "user_gpio.h"

const char version[] = "USBLink FW_V1.0";

// main program core 1
void core1_entry_usb(void)
{
	bool con;

	tusb_init();

	while (1) {
		tud_task();

		con = 0;
		if (tud_cdc_n_connected(0)) {
			con = 1;
			usb_cdc_process();
		}

		gpio_put(LED_PIN_RED, con);
	}
}

// main program core 0
int main(void)
{
	uint8_t opmode;
	uint32_t x;

	// init gpio but not uart pins
	init_gpio();
	// init rf
	if (cyw43_arch_init()) {
		return -1;
	}
	// signal if watchdog hit
	if (watchdog_enable_caused_reboot()) {
		for (x = 0; x < 3; x++) {
			set_onboard_led(0);
			sleep_ms(50);
			set_onboard_led(1);
			sleep_ms(450);
		}
	} else { // sign of live

		set_onboard_led(1);
	}
	// start watchdog
	watchdog_enable(500, 1);
	// check for push button to select operation mode
	opmode = opmode_select();
	// feed watchdog
	watchdog_update();
	// check if we get power from usb
	if (check_usbpwr() == 1) {
		if (opmode == opmode_esc) { // esc programmer mode
			usbd_serial_init();
			init_cdc_data();
			init_cdc_uart_hw();
			// start core 1
			multicore_launch_core1(core1_entry_usb);
			// run esc programmer
			run_esc_app();
		}

		else if (opmode == opmode_rec) { // reciever test mode
			usbd_serial_init();
			init_cdc_data();
			init_receiver_tester_hw();
			// start core 1
			multicore_launch_core1(core1_entry_usb);
			// run receiver tester
			run_receiver_tester_app();
		}

		else if (opmode == opmode_servo) { // servo mode
			usbd_serial_init();
			init_cdc_data();
			init_servo_tester_hw();
			// start core 1
			multicore_launch_core1(core1_entry_usb);
			// run servo tester
			run_servo_tester_app();
		}
	}

	else { // power from extern only -> only blink
		run_no_usb_app();
	}

	// should never get here
	return 0;
}
