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
#include <tusb.h>

#include "rc.h"
#include "uart_bridge.h"
#include "usb_descriptors.h"
#include "user_gpio.h"


// main program core 1
void core1_entry(void)
{
    int itf;
    int con;

    tusb_init();

    while (1)
    {
        tud_task();

        for (itf = 0; itf < CFG_TUD_CDC; itf++)
        {
            con = 0;
            if (tud_cdc_n_connected(itf))
            {
                con = 1;
                usb_cdc_process(itf);
            }

            if (itf == 0)
            {
                gpio_put(LED_PIN_RED, con);
            }
            else if (itf == 1)
            {
                gpio_put(LED_PIN_BLUE, con);
            }
        }
    }
}

// main program core 0
int main(void)
{
    int itf;
    uint32_t pulse;
    uint32_t angle;
    bool update_angle;
    uint8_t opmode;
    uint8_t print_buf[BUFFER_SIZE];
    uint32_t print_buf_pos;
    uint8_t stdin_buf[BUFFER_SIZE];
    uint32_t stdin_buf_pos;

    // init gpio but not uart pins
    init_gpio();

    // check for push button to select operation mode
    opmode = opmode_select();

    // esc programmer
    if (opmode == 0)
    {
        usbd_serial_init();

        for (itf = 0; itf < CFG_TUD_CDC; itf++)
        {
            init_uart_data(itf);
        }
        // start core 1
        multicore_launch_core1(core1_entry);

        while (1)
        {
            for (itf = 0; itf < CFG_TUD_CDC; itf++)
            {
                update_uart_cfg(itf);
                uart_write_bytes(itf);
            }
        }

        return 0;
    }
    // reciever mode
    else if (opmode == 1)
    {
        // set pin as input from RC receiver and start measuring pulses
        rc_init_input(RECV_CH1_PIN, true);

        usbd_serial_init();

        init_uart_data(1);

        // start core 1
        multicore_launch_core1(core1_entry);

        memset(stdin_buf, 0, sizeof(stdin_buf));
        while (1)
        {
            update_uart_cfg(1);
            dbg_read_usb(1, stdin_buf);

            // Read input from RC receiver - that is pulse width on input pin.
            pulse = rc_get_input_pulse_width(RECV_CH1_PIN);
            sprintf(print_buf, "Pulse ch1= %lu\n", pulse);
            dbg_print_usb(1, print_buf);

            // Test if a channel is active:
            rc_reset_input_pulse_width(RECV_CH1_PIN);
            sleep_ms(30);// RC period is 20 ms so in 30 there should be some pulse
            pulse = rc_get_input_pulse_width(RECV_CH1_PIN);
            if (pulse == 0)
            {
                sprintf(print_buf, "Channel 1 disconnected\n");
                dbg_print_usb(1, print_buf);
            }

            sleep_ms(400);
        }
        return 0;
    }
    // servo mode
    else if (opmode == 2)
    {
        // create servo object and start pwm
        rc_servo myServo1 = rc_servo_init(SERV_CH1_PIN);
        rc_servo_start(&myServo1, 90);// set servo1 90 degrees

        usbd_serial_init();

        init_uart_data(1);

        // start core 1
        multicore_launch_core1(core1_entry);

        angle = 90;
        update_angle = 1;
        memset(stdin_buf, 0, sizeof(stdin_buf));
        while (1)
        {
            update_uart_cfg(1);
            dbg_read_usb(1, stdin_buf);

            stdin_buf_pos = 0;
            while (stdin_buf[stdin_buf_pos] && stdin_buf_pos < sizeof(stdin_buf))
            {
                if (stdin_buf[stdin_buf_pos] == '+')
                {
                    if (angle < 180)
                    {
                        angle++;
                        update_angle = 1;
                    }
                }
                if (stdin_buf[stdin_buf_pos] == '-')
                {
                    if (angle > 0)
                    {
                        angle--;
                        update_angle = 1;
                    }
                }
                stdin_buf[stdin_buf_pos] = 0;
                stdin_buf_pos++;
            }
            
            if (update_angle)
            {
                update_angle = 0;
                sleep_ms(25);
                rc_servo_set_angle(&myServo1, angle);
                sprintf(print_buf, "Set ch1 = %lu deg\n", angle);
                dbg_print_usb(1, print_buf);
            }
        }
        return 0;
    }
    // should never get here
    else
    {
        return 0;
    }
}
