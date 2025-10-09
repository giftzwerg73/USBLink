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
#include <tusb.h>

#include "rc.h"
#include "uart_bridge.h"
#include "usb_descriptors.h"
#include "user_gpio.h"


// main program core 1
void core1_entry(void)
{
    int con;

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
    uint32_t pulse;
    uint32_t angle;
    bool update_angle;
    uint32_t update_angle_cnt;
    bool escpower;
    uint32_t escpower_cnt;
    uint32_t rec_update_cnt;
    uint8_t opmode;
    uint8_t print_buf[BUFFER_SIZE];
    uint32_t print_buf_pos;
    uint8_t stdin_buf[BUFFER_SIZE];
    uint32_t stdin_buf_pos;
    uint32_t x;

    // init gpio but not uart pins
    init_gpio();

    if (watchdog_enable_caused_reboot())
    {
        for (x = 0; x < 9; x++)
        {
            set_onboard_led(1);
            sleep_x10ms(5);
            set_onboard_led(0);
            sleep_x10ms(45);
        }
        set_onboard_led(1);
    }
    // start watchdog
    watchdog_enable(250, 1);

    // check for push button to select operation mode
    opmode = opmode_select();

    watchdog_update();
    // esc programmer
    if (opmode == opmode_esc)
    {
        usbd_serial_init();
        init_uart_data();
        init_uart_hw();
        // start core 1
        multicore_launch_core1(core1_entry);

        while (1)
        {
            watchdog_update();
            update_uart_cfg();
            uart_write_bytes();

            sleep_us(10);
            if (check_button_event() == bt_evtup_long)
            {
                dbg_print_usb("Going down esc progrmmer\n");
                trigger_reset();
            }
        }
        return 0;
    }
    // reciever mode
    else if (opmode == opmode_rec)
    {
        // set pin as input for RC receiver and start measuring pulses
        rc_init_input(RECV_CH1_PIN, true);
        usbd_serial_init();
        init_uart_data();
        // start core 1
        multicore_launch_core1(core1_entry);

        escpower = 0;
        escpower_cnt = 0;
        rec_update_cnt = 0;
        memset(stdin_buf, 0, sizeof(stdin_buf));

        while (1)
        {
            watchdog_update();
            update_uart_cfg();
            dbg_read_usb(stdin_buf);
            escpower = ceck_escpwr();

            escpower_cnt++;
            if (escpower_cnt >= 1000 * 100)
            {
                escpower_cnt = 0;
                if (escpower == 0)
                {
                    sprintf(print_buf, "Power receiver up\n");
                    dbg_print_usb(print_buf);
                }
            }

            rec_update_cnt++;
            if (rec_update_cnt >= 100 * 100)
            {
                rec_update_cnt = 0;
                if (escpower == 1)
                {
                    // Read input from RC receiver - that is pulse width on input pin.
                    pulse = rc_get_input_pulse_width(RECV_CH1_PIN);
                    sprintf(print_buf, "Pulse ch1= %lu\n", pulse);
                    dbg_print_usb(print_buf);

                    // Test if a channel is active:
                    rc_reset_input_pulse_width(RECV_CH1_PIN);
                    sleep_ms(30);// RC period is 20 ms so in 30 there should be some pulse
                    pulse = rc_get_input_pulse_width(RECV_CH1_PIN);
                    if (pulse == 0)
                    {
                        sprintf(print_buf, "Channel 1 disconnected\n");
                        dbg_print_usb(print_buf);
                    }
                }
            }

            sleep_us(10);
            if (check_button_event() == bt_evtup_long)
            {
                dbg_print_usb("Going down receiver test\n");
                trigger_reset();
            }
        }
        return 0;
    }
    // servo mode
    else if (opmode == opmode_servo)
    {
        // create servo object and start pwm
        rc_servo myServo1 = rc_servo_init(SERV_CH1_PIN);
        rc_servo_start(&myServo1, 90);// set servo1 90 degrees

        usbd_serial_init();

        init_uart_data();

        // start core 1
        multicore_launch_core1(core1_entry);

        angle = 90;
        update_angle = 0;
        update_angle_cnt = 0;
        escpower = 0;
        escpower_cnt = 0;
        memset(stdin_buf, 0, sizeof(stdin_buf));
        while (1)
        {
            watchdog_update();
            update_uart_cfg();
            dbg_read_usb(stdin_buf);
            escpower = ceck_escpwr();

            stdin_buf_pos = 0;
            while (stdin_buf[stdin_buf_pos] && stdin_buf_pos < sizeof(stdin_buf))
            {
                if (stdin_buf[stdin_buf_pos] == '+')
                {
                    if (angle < 180)
                    {
                        angle++;
                    }

                    if (escpower == 1)
                    {
                        update_angle = 1;
                    }
                    else
                    {
                        update_angle = 0;
                        sprintf(print_buf, "INC ch1 = %lu deg\n", angle);
                        dbg_print_usb(print_buf);
                    }
                }
                else if (stdin_buf[stdin_buf_pos] == '-')
                {
                    if (angle > 0)
                    {
                        angle--;
                    }

                    if (escpower == 1)
                    {
                        update_angle = 1;
                    }
                    else
                    {
                        update_angle = 0;
                        sprintf(print_buf, "DEC ch1 = %lu deg\n", angle);
                        dbg_print_usb(print_buf);
                    }
                }
                else if (stdin_buf[stdin_buf_pos] == 'o')
                {
                    angle = 180;

                    if (escpower == 1)
                    {
                        update_angle = 1;
                    }
                    else
                    {
                        update_angle = 0;
                        sprintf(print_buf, "Set ch1 = %lu deg\n", angle);
                        dbg_print_usb(print_buf);
                    }
                }
                else if (stdin_buf[stdin_buf_pos] == 'k')
                {
                    angle = 90;

                    if (escpower == 1)
                    {
                        update_angle = 1;
                    }
                    else
                    {
                        update_angle = 0;
                        sprintf(print_buf, "Set ch1 = %lu deg\n", angle);
                        dbg_print_usb(print_buf);
                    }
                }
                else if (stdin_buf[stdin_buf_pos] == 'm')
                {
                    angle = 0;

                    if (escpower == 1)
                    {
                        update_angle = 1;
                    }
                    else
                    {
                        update_angle = 0;
                        sprintf(print_buf, "Set ch1 = %lu deg\n", angle);
                        dbg_print_usb(print_buf);
                    }
                }
                stdin_buf[stdin_buf_pos] = 0;
                stdin_buf_pos++;
            }

            escpower_cnt++;
            if (escpower_cnt >= 1000 * 100)
            {
                escpower_cnt = 0;
                if (escpower == 0)
                {
                    sprintf(print_buf, "Power Servo up\n");
                    dbg_print_usb(print_buf);
                }
            }

            update_angle_cnt++;
            if (update_angle_cnt >= 25 * 100)
            {
                update_angle_cnt = 0;
                if (update_angle)
                {
                    update_angle = 0;
                    update_angle_cnt = 0;
                    rc_servo_set_angle(&myServo1, angle);
                    sprintf(print_buf, "Write ch1 = %lu deg\n", angle);
                    dbg_print_usb(print_buf);
                }
            }

            sleep_us(10);
            if (check_button_event() == bt_evtup_long)
            {
                dbg_print_usb("Going down servo test\n");
                trigger_reset();
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
