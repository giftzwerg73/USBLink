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


static const uint32_t looptime = 10;
static const uint32_t pwrupdlytime = 3 * 1000 / looptime;
static const uint32_t pwmupdatetime = 200 * 1000 / looptime;
static const uint32_t recvupdatetime = 100 * 1000 / looptime;
static const uint32_t msgupdatetime = 1000 * 1000 / looptime;


// main esc programmer
void run_esc_app(void)
{
    bool escpower;
    uint32_t escpower_cnt;

    escpower_cnt = 0;
    while (1)
    {
        watchdog_update();
        update_uart_cfg();
        uart_write_bytes();
        escpower = ceck_escpwr();

        if (escpower == 0)
        {
            escpower_cnt++;
            if (escpower_cnt > msgupdatetime)
            {
                dbg_print_usb("Switch power on\n");
                escpower_cnt = 0;
            }
        }

        sleep_us(looptime);
        if (check_button_event() == bt_evtup_long)
        {
            if (escpower == 0)
            {
                dbg_print_usb("Going down esc progrmmer\n");
            }
            trigger_reset();
        }
    }
}


// main reciever tester
void run_receiver_tester_app(void)
{
    static uint32_t state;
    bool escpower;
    uint32_t escpower_cnt;
    uint32_t pulse;
    uint8_t stdin_buf[BUFFER_SIZE];
    uint32_t stdin_buf_pos;
    uint8_t print_buf[BUFFER_SIZE];
    uint32_t print_buf_pos;

    escpower_cnt = 0;
    state = 0;
    memset(stdin_buf, 0, sizeof(stdin_buf));
    while (1)
    {
        watchdog_update();
        update_uart_cfg();
        dbg_read_usb(stdin_buf);
        escpower = ceck_escpwr();

        switch (state)
        {
            case 0:
                gpio_init(SERV_CH1_PIN);
                gpio_set_dir(SERV_CH1_PIN, GPIO_OUT);
                gpio_put(SERV_CH1_PIN, 0);
                rc_init_input(RECV_CH1_PIN, true);
                escpower_cnt = 0;
                state = 1;
                break;
            case 1:
                if (escpower)
                {
                    escpower_cnt = 0;
                    state = 2;
                }
                else
                {
                    escpower_cnt++;
                    if (escpower_cnt > msgupdatetime)
                    {
                        dbg_print_usb("Switch power on\n");
                        escpower_cnt = 0;
                    }
                }
                break;
            case 2:
                if (escpower)
                {
                    escpower_cnt++;
                    if (escpower_cnt > pwrupdlytime)
                    {
                        rc_reset_input_pulse_width(RECV_CH1_PIN);
                        dbg_print_usb("Start reading pulses\n");
                        escpower_cnt = 0;
                        state = 3;
                    }
                }
                else
                {
                    dbg_print_usb("Power is off\n");
                    escpower_cnt = 0;
                    state = 4;
                }
                break;
            case 3:
                if (escpower)
                {
                    escpower_cnt++;
                    if (escpower_cnt > recvupdatetime)
                    {
                        // Read input from RC receiver - that is pulse width on input pin.
                        pulse = rc_get_input_pulse_width(RECV_CH1_PIN);
                        sprintf(print_buf, "Pulse ch1 = %lu\n", pulse);
                        dbg_print_usb(print_buf);
                        escpower_cnt = 0;
                    }
                }
                else
                {
                    dbg_print_usb("Power is off\n");
                    escpower_cnt = 0;
                    state = 4;
                }
                break;
            case 4:
                if (escpower)
                {
                    dbg_print_usb("Power is on again\n");
                    escpower_cnt = 0;
                    state = 2;
                }
                break;
            default:
                break;
        }

        sleep_us(looptime);
        if (check_button_event() == bt_evtup_long)
        {
            dbg_print_usb("Going down receiver test\n");
            trigger_reset();
        }
    }
}


// main servo tester
void run_servo_tester_app(void)
{
    static uint32_t state;
    bool escpower;
    uint32_t escpower_cnt;
    uint32_t angle;
    bool update_angle;
    bool update_print_angle;
    uint32_t pulse;
    rc_servo Servo1;
    uint8_t stdin_buf[BUFFER_SIZE];
    uint32_t stdin_buf_pos;
    uint8_t print_buf[BUFFER_SIZE];
    uint32_t print_buf_pos;

    angle = 90;
    update_angle = 0;
    escpower_cnt = 0;
    state = 0;
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
                    update_angle = 1;
                }
                update_print_angle = 1;
            }
            else if (stdin_buf[stdin_buf_pos] == '-')
            {
                if (angle > 0)
                {
                    angle--;
                    update_angle = 1;
                }
                update_print_angle = 1;
            }
            else if (stdin_buf[stdin_buf_pos] == 'o')
            {
                angle = 180;
                update_angle = 1;
                update_print_angle = 1;
            }
            else if (stdin_buf[stdin_buf_pos] == 'k')
            {
                angle = 90;
                update_angle = 1;
                update_print_angle = 1;
            }
            else if (stdin_buf[stdin_buf_pos] == 'm')
            {
                angle = 0;
                update_angle = 1;
                update_print_angle = 1;
            }
            stdin_buf[stdin_buf_pos] = 0;
            stdin_buf_pos++;
        }

        if (update_print_angle)
        {
            update_print_angle = 0;
            sprintf(print_buf, "Set ch1 = %lu deg\n", angle);
            dbg_print_usb(print_buf);
        }

        switch (state)
        {
            case 0:
                if (escpower)
                {
                    escpower_cnt++;
                    if (escpower_cnt > msgupdatetime)
                    {
                        dbg_print_usb("Switch power off first\n");
                        escpower_cnt = 0;
                    }
                }
                else
                {
                    state = 1;
                }
                break;
            case 1:// init wait for power up
                if (escpower)
                {
                    dbg_print_usb("Init PWM\n");
                    Servo1 = rc_servo_init(SERV_CH1_PIN);
                    rc_init_input(RECV_CH1_PIN, true);
                    escpower_cnt = 0;
                    state = 2;
                }
                break;
            case 2:// power up delay
                if (escpower)
                {
                    escpower_cnt++;
                    if (escpower_cnt > pwrupdlytime)
                    {
                        rc_servo_start(&Servo1, angle);// set servo1 start degrees
                        sprintf(print_buf, "Start ch1 = %lu deg\n", angle);
                        dbg_print_usb(print_buf);
                        escpower_cnt = 0;
                        state = 3;
                    }
                }
                else
                {
                    dbg_print_usb("Power is off\n");
                    escpower_cnt = 0;
                    state = 4;
                }
                break;
            case 3:
                if (escpower)
                {
                    escpower_cnt++;
                    if (escpower_cnt == pwmupdatetime / 2)
                    {
                        // Read input from RC receiver - that is pulse width on input pin.
                        pulse = rc_get_input_pulse_width(RECV_CH1_PIN);
                        sprintf(print_buf, "Pulse ch1 = %lu\n", pulse);
                        dbg_print_usb(print_buf);
                    }

                    if (escpower_cnt > pwmupdatetime)
                    {
                        escpower_cnt = 0;
                        if (update_angle)
                        {
                            update_angle = 0;
                            rc_servo_set_angle(&Servo1, angle);
                            sprintf(print_buf, "Write ch1 = %lu deg\n", angle);
                            dbg_print_usb(print_buf);
                        }
                    }
                }
                else
                {
                    rc_servo_stop(&Servo1, true);
                    dbg_print_usb("Power is off\n");
                    escpower_cnt = 0;
                    state = 4;
                }
                break;
            case 4:
                if (escpower)
                {
                    dbg_print_usb("Restart without init PWM\n");
                    escpower_cnt = 0;
                    state = 2;
                }
                break;
            default:
                break;
        }

        sleep_us(looptime);
        if (check_button_event() == bt_evtup_long)
        {
            dbg_print_usb("Going down servo test\n");
            trigger_reset();
        }
    }
}
