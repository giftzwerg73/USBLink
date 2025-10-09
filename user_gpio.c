// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 Marcus Schuster <ms@nixmail.com>
 */


#include <hardware/irq.h>
#include <hardware/structs/sio.h>
#include <hardware/uart.h>
#include <hardware/watchdog.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <string.h>

#include "uart_bridge.h"
#include "user_gpio.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

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
    // set leds blue and red off green on
    set_blue_led(0);
    set_red_led(0);
    set_onboard_led(1);
    // read inputs
    vusb[0] = vusb[1] = vusb[2] = get_vusb();
    escpwr[0] = escpwr[1] = get_escpower();
    escpwr[2] = 255;
    btn[0] = btn[1] = get_button();
    btn[2] = 255;
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
    uint8_t print_buf[128];
    uint32_t blink_on, blink_off, blink_cnt, ret_cnt;

    set_blue_led(0);
    set_red_led(0);
    opmode = opmode_esc;
    while (1)
    {// first check until button settled up or down
        watchdog_update();
        if (get_button() == 1 && get_button() == 1 && get_button() == 1)
        {// button up
            opmode = opmode_esc;
            return opmode;
        }
        else if (get_button() == 0 && get_button() == 0 && get_button() == 0)
        {//  button down -> selection mode
            set_blue_led(1);
            state = 0;
            while (1)
            {
                watchdog_update();
                switch (state)
                {
                    case 0:// wait for button release
                        if (get_button() == 1 && get_button() == 1 && get_button() == 1)
                        {
                            blink_on = 250 * 1000 / looptime;
                            blink_off = 250 * 1000 / looptime;
                            blink_cnt = 0;
                            opmode = opmode_rec;
                            state = 1;
                        }
                        break;
                    case 1:
                        // blink blue led according to opmode
                        blink_cnt++;
                        if (blink_cnt == blink_on)
                        {
                            set_blue_led(1);
                        }
                        if (blink_cnt == blink_on + blink_off)
                        {
                            set_blue_led(0);
                            blink_cnt = 0;
                        }
                        bt_evnt = check_button_event();
                        if (bt_evnt == bt_evtup_short)
                        {
                            if (opmode == opmode_esc)
                            {
                                blink_on = 250 * 1000 / looptime;
                                blink_off = 250 * 1000 / looptime;
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
                                blink_on = 500 * 1000 / looptime;
                                blink_off = 500 * 1000 / looptime;
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
                            state = 2;
                        }
                        break;
                    case 2:
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
    }
}

// check esc power gpio
bool ceck_escpwr(void)
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

// check for button events
uint8_t check_button_event(void)
{
    static uint8_t state = 0;
    static uint32_t time_cnt;
    uint8_t ret;
    bool button_buf[3];
    uint8_t button_state;

    button_buf[0] = get_button();
    button_buf[1] = get_button();
    button_buf[2] = get_button();
    if (button_buf[0] == 1 && button_buf[1] == 1 && button_buf[2] == 1)
        button_state = 0;
    else if (button_buf[0] == 0 && button_buf[1] == 0 && button_buf[2] == 0)
        button_state = 1;
    else
        button_state = 255;

    switch (state)
    {
        case 0:// init, wait until not pressed
            ret = bt_undev;
            if (button_state == 0)
            {
                ret = bt_up;
                time_cnt = 0;
                state = 1;
            }
            break;
        case 1:// button not pressed
            ret = bt_up;
            if (button_state == 1)
            {
                ret = bt_evtdown;
                state = 2;
            }
            break;
        case 2:// button down event
            ret = bt_down;
            state = 3;
            break;
        case 3:// button pressed
            time_cnt++;
            ret = bt_down;
            if (button_state == 0)
            {
                ret = bt_evtup;
                if (time_cnt > 150 * 100 && time_cnt < 1000 * 100)
                {
                    time_cnt = 0;
                    ret = bt_evtup_short;
                }
                else if (time_cnt > 3000 * 100)
                {
                    time_cnt = 0;
                    ret = bt_evtup_long;
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

/*
void test_code_opmode(void)
{
    // test code opmode()
    uint8_t state;
    uint8_t bt_evnt;

    usbd_serial_init();
    init_uart_data();
    // start core 1
    multicore_launch_core1(core1_entry);

    set_blue_led(0);
    set_red_led(0);
    opmode = opmode_esc;
    state = 0;
    sleep_ms(30);
    while (1)
    {
        update_uart_cfg();

        watchdog_update();
        bt_evnt = check_button_event();
        switch (state)
        {
            case 0:// power up
                if (bt_evnt == bt_up)// not pressed
                {
                    set_blue_led(0);
                    set_red_led(0);
                    opmode = opmode_esc;
                    // return opmode;
                }
                else if (bt_evnt == bt_evtup)// button released
                {
                    state = 1;
                    dbg_print_usb("goto select mode\n");
                }
                break;
            case 1:
                if (bt_evnt == bt_evtup_short)
                {
                    if (opmode == opmode_esc)
                    {
                        set_blue_led(0);
                        set_red_led(1);
                        opmode = opmode_rec;
                        dbg_print_usb("select opmode_rec\n");
                    }
                    else if (opmode == opmode_rec)
                    {
                        set_blue_led(1);
                        set_red_led(1);
                        opmode = opmode_servo;
                        dbg_print_usb("select opmode_servo\n");
                    }
                    else if (opmode == opmode_servo)
                    {
                        set_blue_led(0);
                        set_red_led(0);
                        opmode = opmode_esc;
                        dbg_print_usb("select opmode_esc\n");
                    }
                }
                else if (bt_evnt == bt_evtup_long)
                {
                    // return opmode;
                    dbg_print_usb("return selected opmode\n");
                }
                break;
            default:
                break;
        }

        sleep_us(10);
    }
    // end test code opmode
}
*/
