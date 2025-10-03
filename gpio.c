// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 Marcus Schuster <ms@nixmail.com>
 */

#include <hardware/irq.h>
#include <hardware/structs/sio.h>
#include <hardware/uart.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <string.h>
 
#define ESC_PWR_PIN 2
#define SW_PIN 15
#define LED_PIN_BLUE 16
#define LED_PIN_RED 17

uint8_t btn[2];
uint8_t vusb[2];
uint8_t escpwr[2];

// Perform initialisation
static int pico_led_init(void) 
{
#if defined(PICO_DEFAULT_LED_PIN)
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // For Pico W devices we need to initialise the driver etc
    return cyw43_arch_init();
#endif
}

// read usb power
bool pico_get_vusb(void) 
{
#if defined(PICO_DEFAULT_LED_PIN)
    // Just read GPIO24
    return gpio_get(24);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to read GPIO02
    return cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN);
#endif
}


// Turn onboard led on or off
void set_onboard_led(bool led_on) 
{
#if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to set the GPIO on or off
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}

// Toggle blue led
inline void toggle_blue_led()
{
	gpio_put(LED_PIN_BLUE, !gpio_get_out_level(LED_PIN_BLUE));
}

// Toggle blue led
inline void toggle_red_led()
{
	gpio_put(LED_PIN_RED, !gpio_get_out_level(LED_PIN_RED));
}


void init_gpio(void)
{
    int rc;
	
	// init gpio
	rc = pico_led_init();
    hard_assert(rc == PICO_OK);
    gpio_init(LED_PIN_BLUE);
	gpio_init(LED_PIN_RED);
	gpio_init(SW_PIN);
	gpio_init(ESC_PWR_PIN);
	// set direction
	gpio_set_dir(LED_PIN_BLUE, GPIO_OUT);
	gpio_set_dir(LED_PIN_RED, GPIO_OUT);
	gpio_set_dir(SW_PIN, GPIO_IN);
	gpio_set_dir(ESC_PWR_PIN, GPIO_IN);
	// set outputs
	gpio_put(LED_PIN_BLUE, 1);
	gpio_put(LED_PIN_RED, 1);
	pico_set_led(0);
	// read inputs 
	vusb[0] = vusb[1] = pico_get_vusb();
	btn[0] = btn[1] = gpio_get(SW_PIN);
	escpwr[0] = escpwr[1] = gpio_get(ESC_PWR_PIN);
}
