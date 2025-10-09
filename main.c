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
    const uint32_t looptime = 10;
    const uint32_t pwrupdlytime = 3 * 1000 / looptime;
    const uint32_t pwmupdatetime = 200 * 1000 / looptime;
    static uint32_t state;
    uint32_t pulse;
    uint32_t angle;
    bool update_angle;
    bool update_print_angle;
    bool escpower;
    uint32_t escpower_cnt;
    uint8_t opmode;
    uint8_t print_buf[BUFFER_SIZE];
    uint32_t print_buf_pos;
    uint8_t stdin_buf[BUFFER_SIZE];
    uint32_t stdin_buf_pos;
    uint32_t x;
    rc_servo Servo1;


    // init gpio but not uart pins
    init_gpio();

    if (watchdog_enable_caused_reboot())
    {
        for (x = 0; x < 3; x++)
        {
            set_onboard_led(0);
            sleep_ms(50);
            set_onboard_led(1);
            sleep_ms(450);
        }
        set_onboard_led(1);
    }
    // start watchdog
    watchdog_enable(500, 1);

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

            sleep_us(looptime);
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
        usbd_serial_init();

        init_uart_data();

        // start core 1
        multicore_launch_core1(core1_entry);

        escpower = 0;
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
                        if (escpower_cnt > pwmupdatetime)
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
                        if (escpower_cnt > pwmupdatetime)
                        {
                            // Read input from RC receiver - that is pulse width on input pin.
                            pulse = rc_get_input_pulse_width(RECV_CH1_PIN);
                            sprintf(print_buf, "Pulse ch1= %lu\n", pulse);
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
        return 0;
    }
    // servo mode
    else if (opmode == opmode_servo)
    {
        usbd_serial_init();

        init_uart_data();

        // start core 1
        multicore_launch_core1(core1_entry);

        angle = 90;
        update_angle = 0;
        escpower = 0;
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
                        if (escpower_cnt > pwmupdatetime)
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
                            sprintf(print_buf, "Pulse ch1= %lu\n", pulse);
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
        return 0;
    }
    // should never get here
    else
    {
        return 0;
    }
}
