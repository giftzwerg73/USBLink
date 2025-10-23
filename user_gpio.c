// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 Marcus Schuster <ms@nixmail.com>
 */


#include "pico/cyw43_arch.h"
#include <hardware/irq.h>
#include <hardware/structs/sio.h>
#include <hardware/uart.h>
#include <hardware/watchdog.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <string.h>

#include "uart_bridge.h"
#include "user_gpio.h"


// read usb power
inline bool get_vusb(void)
{
    // return gpio_get(24);
    return cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN);
}

// turn onboard led on or off
inline void set_onboard_led(bool led_on)
{
    // gpio_put(PICO_DEFAULT_LED_PIN, led_on);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
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

// initialise gpio
void init_gpio(void)
{
    // init gpio
    // gpio_init(PICO_DEFAULT_LED_PIN);
    // gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_init(LED_PIN_BLUE);
    gpio_init(LED_PIN_RED);
    gpio_init(SW_PIN);
    gpio_init(ESC_PWR_PIN);
    // set direction
    gpio_set_dir(LED_PIN_BLUE, GPIO_OUT);
    gpio_set_dir(LED_PIN_RED, GPIO_OUT);
    gpio_set_dir(SW_PIN, GPIO_IN);
    gpio_set_dir(ESC_PWR_PIN, GPIO_IN);
    // set leds blue and red off green on
    set_blue_led(0);
    set_red_led(0);
}

// get state of button 1=presses 0=relesed 255=unknown
uint8_t get_button_state(void)
{
    static int32_t sample_cnt = 0;
    uint8_t ret;

    ret = bt_undev;
    if (get_button())
    {
        if (sample_cnt < 50)
        {
            sample_cnt++;
        }
        else
        {
            ret = bt_up;
        }
    }
    else
    {
        if (sample_cnt > -50)
        {
            sample_cnt--;
        }
        else
        {
            ret = bt_down;
        }
    }
    return ret;
}

// sleep x times 10ms and feed wathdog
void sleep_x10ms(uint32_t wait)
{
    uint32_t x;

    for (x = 0; x < wait; x++)
    {
        watchdog_update();
        sleep_ms(10);
    }
}

// check button at start up to define mode
uint8_t opmode_select(void)
{
    const uint32_t looptime = 10;
    uint8_t state;
    uint8_t opmode;
    uint8_t bt_evnt;
    uint32_t blink_on, blink_off, blink_cnt, ret_cnt;

    set_blue_led(0);
    set_red_led(0);
    opmode = opmode_esc;
    state = 0;
    while (1)
    {
        watchdog_update();
        bt_evnt = check_button_event();
        switch (state)
        {
            case 0:// first check until button settled up or down
                if (bt_evnt == bt_up)
                {// -> esc mode
                    set_blue_led(0);
                    set_red_led(0);
                    opmode = opmode_esc;
                    return opmode;
                }
                else if (bt_evnt == bt_down)
                {// -> selection mode
                    set_blue_led(1);
                    state = 1;
                }
                break;
            case 1:// wait for button release no matter how long
                if (bt_evnt == bt_evtup || bt_evnt == bt_evtup_short || bt_evnt == bt_evtup_long)
                {
                    blink_on = 200 * 1000 / looptime;
                    blink_off = 200 * 1000 / looptime;
                    blink_cnt = 0;
                    opmode = opmode_rec;
                    state = 2;
                }
                break;
            case 2:
                // blink blue led according to opmode
                blink_cnt++;
                if (blink_cnt == blink_on)
                {
                    set_blue_led(1);
                }
                if (blink_cnt >= blink_on + blink_off)
                {
                    set_blue_led(0);
                    blink_cnt = 0;
                }

                if (bt_evnt == bt_evtup_short)
                {
                    if (opmode == opmode_esc)
                    {
                        blink_on = 200 * 1000 / looptime;
                        blink_off = 200 * 1000 / looptime;
                        blink_cnt = 0;
                        opmode = opmode_rec;
                    }
                    else if (opmode == opmode_rec)
                    {
                        blink_on = 100 * 1000 / looptime;
                        blink_off = 100 * 1000 / looptime;
                        blink_cnt = 0;
                        opmode = opmode_servo;
                    }
                    else if (opmode == opmode_servo)
                    {
                        blink_on = 400 * 1000 / looptime;
                        blink_off = 400 * 1000 / looptime;
                        blink_cnt = 0;
                        opmode = opmode_esc;
                    }
                }
                else if (bt_evnt == bt_evtup_long)
                {
                    blink_on = 250 * 1000 / looptime;
                    blink_off = 250 * 1000 / looptime;
                    blink_cnt = 0;
                    ret_cnt = 5;
                    set_blue_led(0);
                    set_red_led(0);
                    state = 3;
                }
                break;
            case 3:
                blink_cnt++;
                if (blink_cnt == blink_on)
                {
                    set_blue_led(0);
                    set_red_led(1);
                }
                if (blink_cnt == blink_on + blink_off)
                {
                    set_blue_led(1);
                    set_red_led(0);
                    blink_cnt = 0;
                    ret_cnt--;
                }

                if (ret_cnt == 0)
                {
                    set_blue_led(0);
                    set_red_led(0);
                    return opmode;
                }
                break;
            default:
                break;
        }

        sleep_us(looptime);
    }
}

// check esc power gpio
bool check_escpwr(void)
{
    if (get_escpower() == 1 && get_escpower() == 1 && get_escpower() == 1)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// check esc power gpio
bool check_usbpwr(void)
{
    if (get_vusb() == 1 && get_vusb() == 1 && get_vusb() == 1)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// check for button events
uint8_t check_button_event(void)
{
    static uint8_t state = 0;
    static uint32_t time_cnt;
    uint8_t ret;
    bool button_buf[3];
    uint8_t button_state;

    button_state = get_button_state();
    switch (state)
    {
        case 0:// init, wait until stable
            ret = bt_undev;
            if (button_state == bt_up)
            {
                ret = bt_up;
                state = 1;
            }
            else if (button_state == bt_down)
            {
                ret = bt_down;
                time_cnt = 0;
                state = 2;
            }
            break;
        case 1:// button not pressed
            ret = bt_up;
            if (button_state == bt_down)
            {
                ret = bt_evtdown;
                time_cnt = 0;
                state = 2;
            }
            break;
        case 2:// button pressed
            ret = bt_down;
            if (time_cnt < 10000 * 100)
                time_cnt++;

            if (time_cnt == 2500 * 100)
            {
                set_onboard_led(0);
            }
            else if (time_cnt == 2600 * 100)
            {
                set_onboard_led(1);
            }

            if (button_state == bt_up)
            {
                if (time_cnt > 100 * 100 && time_cnt < 1000 * 100)
                {
                    ret = bt_evtup_short;
                }
                else if (time_cnt >= 2500 * 100)
                {
                    set_onboard_led(1);
                    ret = bt_evtup_long;
                }
                else
                {
                    ret = bt_evtup;
                }
                state = 1;
            }
            break;
        default:
            time_cnt = 0;
            state = 0;
            ret = bt_undev;
            break;
    }

    return ret;
}

void trigger_reset(void)
{
    uint32_t blink_cnt;

    for (blink_cnt = 0; blink_cnt < 23; blink_cnt++)
    {
        set_blue_led(0);
        sleep_x10ms(5);
        set_blue_led(1);
        sleep_x10ms(5);
    }
    set_blue_led(0);
    watchdog_update();
    while (1)
        blink_cnt++;
}
