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

#include "uart_bridge.h"
#include "user_gpio.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

// pin definition
#define ESC_PWR_PIN 2
#define SW_PIN 15
#define LED_PIN_BLUE 16
#define LED_PIN_RED 17

// input variable
static uint8_t btn[3];
static uint8_t vusb[3];
static uint8_t escpwr[3];

// read usb power
inline bool get_vusb(void) 
{
#if defined(PICO_DEFAULT_LED_PIN)
    // Just read GPIO24
    return gpio_get(24);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to read GPIO02
    return cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN);
#endif
}

// read esc power
inline bool get_escpower(void) 
{ 
	return gpio_get(ESC_PWR_PIN); 
}

// read button
inline bool get_button(void) 
{ 
	return gpio_get(SW_PIN); 
}

// turn onboard led on or off
inline void set_onboard_led(bool led_on) 
{
#if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to set the GPIO on or off
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}

// set blue led on or off
inline void set_blue_led(bool led_on) 
{ 
	gpio_put(LED_PIN_BLUE, led_on); 
}

// set red led on or off
inline void set_red_led(bool led_on) 
{ 
	gpio_put(LED_PIN_RED, led_on); 
}

// toggle blue led
inline void toggle_blue_led(void) 
{ 
	gpio_put(LED_PIN_BLUE, !gpio_get_out_level(LED_PIN_BLUE)); 
}

// toggle red led
inline void toggle_red_led(void) 
{ 
	gpio_put(LED_PIN_RED, !gpio_get_out_level(LED_PIN_RED)); 
}

// initialise onboard led
static inline int onboard_led_init(void) 
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

// initialise gpio
void init_gpio(void) 
{
    int rc;

    // init gpio
    rc = onboard_led_init();
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
    // set leds
    set_blue_led(1);
    set_red_led(1);
    set_onboard_led(0);
    // read inputs
    vusb[0] = vusb[1] = vusb[2] = get_vusb();
    escpwr[0] = escpwr[1] = get_escpower();
    escpwr[2] = 255;
    btn[0] = btn[1] = get_button();
    btn[2] = 255;
}

// sample button
void sample_button(void) 
{
    btn[0] = (uint8_t)get_button();
    if (btn[0] == 0 && btn[1] == 1) 
    {
        btn[1] = btn[0];
        dbg_print_usb(1, "Button pressed\n");
        btn[2] = 1;
    } 
    else if (btn[0] == 1 && btn[1] == 0) 
    {
        btn[1] = btn[0];
        dbg_print_usb(1, "Button released\n");
        btn[2] = 0;
    }
}

// sample esc power
void sample_escpwr(void) 
{
    escpwr[0] = (uint8_t)get_escpower();
    escpwr[2] = escpwr[0];
    if (escpwr[0] == 0 && escpwr[1] == 1) 
    {
        escpwr[1] = escpwr[0];
        dbg_print_usb(1, "ESC_POWER_DOWN\n");
        escpwr[2] = 0;
    } 
    else if (escpwr[0] == 1 && escpwr[1] == 0) 
    {
        escpwr[1] = escpwr[0];
        dbg_print_usb(1, "ESC_POWER_UP\n");
        escpwr[2] = 1;
    }
}

// get state of button 1=presses 0=relesed 255=unknown
uint8_t get_button_state(void) 
{ 
	return btn[2]; 
}

// get state of esc power 1=power-up 0=power-down 255=unknown
uint8_t get_escpwr_state(void) 
{ 
	return escpwr[2]; 
}
