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


// main program core 1
void core1_entry(void)
{
    bool con;

    tusb_init();

    while (1)
    {
        tud_task();

        con = 0;
        if (tud_cdc_n_connected(0))
        {
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
    if (cyw43_arch_init())
    {
        return -1;
    }
    // signal if watchdog hit
    if (watchdog_enable_caused_reboot())
    {
        for (x = 0; x < 3; x++)
        {
            set_onboard_led(0);
            sleep_ms(50);
            set_onboard_led(1);
            sleep_ms(450);
        }
    }
    // sign of live
    set_onboard_led(1);
    // start watchdog
    watchdog_enable(500, 1);
    // check for push button to select operation mode
    opmode = opmode_select();
    // feed watchdog
    watchdog_update();
    // esc programmer mode
    if (opmode == opmode_esc)
    {
        usbd_serial_init();
        init_uart_data();
        init_uart_hw();
        // start core 1
        multicore_launch_core1(core1_entry);
        // run esc programmer
        run_esc_app();
    }
    // reciever test mode
    else if (opmode == opmode_rec)
    {
        usbd_serial_init();
        init_uart_data();
        // start core 1
        multicore_launch_core1(core1_entry);
        // run receiver tester
        run_receiver_tester_app();
    }
    // servo mode
    else if (opmode == opmode_servo)
    {
        usbd_serial_init();
        init_uart_data();
        // start core 1
        multicore_launch_core1(core1_entry);
        // run servo tester
        run_servo_tester_app();
    }
    // should never get here
    return 0;
}
