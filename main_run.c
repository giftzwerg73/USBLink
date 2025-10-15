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
static const uint32_t recvupdatetime = 100 * 1000 / looptime;
static const uint32_t escpwrupdatetime = 100 * 1000 / looptime;
static const uint32_t msgupdatetime = 1000 * 1000 / looptime;

static rc_servo servo1;
static uint8_t stdin_buf[BUFFER_SIZE];
static uint32_t stdin_buf_pos = 0;
static uint8_t print_buf[BUFFER_SIZE];

static void check_escpower_led(void)
{
    static uint32_t escpower_cnt = 0;

    if (escpower_cnt++ > escpwrupdatetime)
    {
        set_blue_led(check_escpwr());
        escpower_cnt = 0;
    }
}

static void check_button_reset(void)
{
    if (check_button_event() == bt_evtup_long)
    {
        set_onboard_led(0);
        set_blue_led(0);
        trigger_reset();
    }
}

static char get_char_stdio(void)
{
    char c;

    // check for buffer overroll
    if (stdin_buf_pos >= BUFFER_SIZE)
    {
        stdin_buf_pos = 0;
    }

    c = stdin_buf[stdin_buf_pos];
    if (c == 0)
        stdin_buf_pos = 0;
    else
    {
        stdin_buf[stdin_buf_pos] = 0;
        stdin_buf_pos++;
    }
    return c;
}

static bool update_angle_from_stdio(uint32_t *val, char c)
{
    uint32_t val_edit;

    if (c != 0)
    {
        val_edit = *val;

        if (c == '+' && val_edit < 180)
            val_edit++;
        else if (c == '-' && val_edit > 0)
            val_edit--;
        else if (c == 'o')
            val_edit = 180;
        else if (c == 'k')
            val_edit = 90;
        else if (c == 'm')
            val_edit = 0;

        if (val_edit != *val)
        {
            *val = val_edit;
            return true;
        }
    }

    return false;
}

static bool averaging_toggle(uint8_t *mean_nr, char c)
{
    uint8_t nr;

    nr = *mean_nr;
    if (c == 'a')
    {
        if (nr != 1)
        {
            *mean_nr = 1;
            print_usb("Pulse averaging OFF\n");
            return true;
        }
        else
        {
            *mean_nr = 4;
            print_usb("Pulse averaging ON\n");
            return true;
        }
    }
    return false;
}

static bool make_mean(uint32_t *reslut, uint32_t newval, uint8_t mean_nr, const uint8_t mean_type)
{
#define MAX_DEPTH 32
    static uint8_t mean_cnt = 0;
    static uint32_t mean_buf[MAX_DEPTH] = { 0 };
    static uint64_t mean_sum = 0;
    uint32_t x;

    if (mean_nr > MAX_DEPTH)
        mean_nr = MAX_DEPTH;

    if (mean_cnt >= mean_nr)
        mean_cnt = 0;

    if (mean_type == 0)// average
    {
        mean_buf[mean_cnt] = newval;
        mean_cnt++;
        if (mean_cnt == mean_nr)
        {
            mean_sum = 0;
            for (x = 0; x < mean_nr; x++)
            {
                mean_sum = mean_sum + mean_buf[x];
            }
            *reslut = mean_sum / mean_nr;
            return true;
        }
    }
    else if (mean_type == 1)// gliding mean
    {
        mean_buf[mean_cnt] = newval;
        mean_cnt++;
        mean_sum = 0;
        for (x = 0; x < mean_nr; x++)
        {
            mean_sum = mean_sum + mean_buf[x];
        }
        *reslut = mean_sum / mean_nr;
        return true;
    }
    else// reset
    {
        mean_cnt = 0;
        mean_sum = 0;
        memset(mean_buf, 0, sizeof(mean_buf));
    }

    return false;
}

// main esc programmer
void run_esc_app(void)
{
    set_blue_led(0);

    while (1)
    {
        watchdog_update();
        update_cdc_uart_cfg();
        write_data_usb2uart();
        check_escpower_led();
        sleep_us(looptime);
        check_button_reset();
    }
}

// main hw init for receiver
void init_receiver_tester_hw(void)
{
    gpio_init(SERV_CH1_PIN);
    gpio_set_dir(SERV_CH1_PIN, GPIO_OUT);
    gpio_put(SERV_CH1_PIN, 0);
    rc_init_input(RECV_CH1_PIN, true);
}

// main reciever tester
void run_receiver_tester_app(void)
{
    uint32_t recvupdate_cnt;
    uint32_t pulse;
    uint32_t pulse_old;
    uint32_t timecnt;
    uint8_t mean_nr;
    char c;

    set_blue_led(0);
    recvupdate_cnt = 0;
    pulse_old = 0xFFFFFFFF;
    timecnt = 0;
    mean_nr = 4;
    stdin_buf_pos = 0;
    memset(stdin_buf, 0, sizeof(stdin_buf));

    while (1)
    {
        watchdog_update();
        read_cdc_data(stdin_buf);
        c = get_char_stdio();

        if (averaging_toggle(&mean_nr, c))
            make_mean(&pulse, pulse, mean_nr, 2);

        timecnt++;
        if (recvupdate_cnt++ > recvupdatetime)
        {
            pulse = rc_get_input_pulse_width(RECV_CH1_PIN);
            if (make_mean(&pulse, pulse, mean_nr, 1))
            {
                if (pulse != pulse_old || timecnt >= msgupdatetime)
                {
                    sprintf(print_buf, "Pulse = %04lu us\n", pulse);
                    print_usb(print_buf);
                    pulse_old = pulse;
                    timecnt = 0;
                }
            }
            recvupdate_cnt = 0;
        }

        check_escpower_led();
        sleep_us(looptime);
        check_button_reset();
    }
}

// main hw init for receiver
void init_servo_tester_hw(void)
{
    // init hw and start pwm
    servo1 = rc_servo_init(SERV_CH1_PIN);
    rc_init_input(RECV_CH1_PIN, true);
    rc_servo_start(&servo1, 90);
}

// main servo tester
void run_servo_tester_app(void)
{
    uint32_t recvupdate_cnt;
    uint32_t pulse;
    uint32_t pulse_old;
    uint32_t angle;
    uint32_t timecnt;
    uint8_t mean_nr;
    char c;

    set_blue_led(0);
    recvupdate_cnt = 0;
    angle = 90;
    pulse_old = 0xFFFFFFFF;
    timecnt = 0;
    mean_nr = 4;
    stdin_buf_pos = 0;
    memset(stdin_buf, 0, sizeof(stdin_buf));

    while (1)
    {
        watchdog_update();
        read_cdc_data(stdin_buf);
        c = get_char_stdio();

        if (averaging_toggle(&mean_nr, c))
            make_mean(&pulse, pulse, mean_nr, 2);

        if (update_angle_from_stdio(&angle, c))
        {
            rc_servo_set_angle(&servo1, angle);
            sprintf(print_buf, "Update: Angle = %03lu deg\n", angle);
            print_usb(print_buf);
        }

        timecnt++;
        if (recvupdate_cnt++ > recvupdatetime)
        {
            pulse = rc_get_input_pulse_width(RECV_CH1_PIN);
            if (make_mean(&pulse, pulse, mean_nr, 1))
            {
                if (pulse != pulse_old || timecnt >= msgupdatetime)
                {
                    sprintf(print_buf, "Pulse = %04lu   Angle = %03lu\n", pulse, angle);
                    print_usb(print_buf);
                    pulse_old = pulse;
                    timecnt = 0;
                }
            }
            recvupdate_cnt = 0;
        }

        check_escpower_led();
        sleep_us(looptime);
        check_button_reset();
    }
}

// main no_usb -> just blink
void run_no_usb_app(void)
{
    uint32_t x;

    x = 0;
    while (1)
    {
        // feed wdt
        watchdog_update();
        // blink
        x++;
        if (x == 100 * 100)
        {
            set_onboard_led(0);
            set_blue_led(1);
            set_red_led(0);
        }
        else if (x >= 200 * 100)
        {
            set_onboard_led(1);
            set_blue_led(0);
            set_red_led(1);
            x = 0;
        }

        sleep_us(looptime);
        check_button_reset();
    }
}
