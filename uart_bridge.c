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

#include "uart_bridge.h"
#include "user_gpio.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#if !defined(MIN)
#define MIN(a, b) ((a > b) ? b : a)
#endif /* MIN */

// prototypes
void uart0_irq_fn(void);

const uart_id_t UART_ID = {

    .inst = uart0,
    .irq = UART0_IRQ,
    .irq_fn = &uart0_irq_fn,
    .tx_pin = 12,
    .rx_pin = 13,

};

uart_data_t UART_DATA;

static inline uint databits_usb2uart(uint8_t data_bits)
{
    switch (data_bits)
    {
        case 5:
            return 5;
        case 6:
            return 6;
        case 7:
            return 7;
        default:
            return 8;
    }
}

static inline uart_parity_t parity_usb2uart(uint8_t usb_parity)
{
    switch (usb_parity)
    {
        case 1:
            return UART_PARITY_ODD;
        case 2:
            return UART_PARITY_EVEN;
        default:
            return UART_PARITY_NONE;
    }
}

static inline uint stopbits_usb2uart(uint8_t stop_bits)
{
    switch (stop_bits)
    {
        case 2:
            return 2;
        default:
            return 1;
    }
}

void update_uart_cfg(void)
{
    const uart_id_t *ui = &UART_ID;
    uart_data_t *ud = &UART_DATA;

    mutex_enter_blocking(&ud->lc_mtx);

    if (ud->usb_lc.bit_rate != ud->uart_lc.bit_rate)
    {
        uart_set_baudrate(ui->inst, ud->usb_lc.bit_rate);
        ud->uart_lc.bit_rate = ud->usb_lc.bit_rate;
    }

    if ((ud->usb_lc.stop_bits != ud->uart_lc.stop_bits) ||
        (ud->usb_lc.parity != ud->uart_lc.parity) ||
        (ud->usb_lc.data_bits != ud->uart_lc.data_bits))
    {
        uart_set_format(ui->inst, databits_usb2uart(ud->usb_lc.data_bits),
                        stopbits_usb2uart(ud->usb_lc.stop_bits),
                        parity_usb2uart(ud->usb_lc.parity));
        ud->uart_lc.data_bits = ud->usb_lc.data_bits;
        ud->uart_lc.parity = ud->usb_lc.parity;
        ud->uart_lc.stop_bits = ud->usb_lc.stop_bits;
    }

    mutex_exit(&ud->lc_mtx);
}

void usb_read_bytes(void)
{
    uart_data_t *ud = &UART_DATA;
    uint32_t len;

    len = tud_cdc_n_available(0);

    if (len && mutex_try_enter(&ud->usb_mtx, NULL))
    {
        len = MIN(len, BUFFER_SIZE - ud->usb_pos);
        if (len)
        {
            uint32_t count;

            count = tud_cdc_n_read(0, &ud->usb_buffer[ud->usb_pos], len);
            ud->usb_pos += count;
        }

        mutex_exit(&ud->usb_mtx);
    }
}

void usb_write_bytes(void)
{
    uart_data_t *ud = &UART_DATA;

    if (ud->uart_pos && mutex_try_enter(&ud->uart_mtx, NULL))
    {
        uint32_t count;

        count = tud_cdc_n_write(0, ud->uart_buffer, ud->uart_pos);
        if (count < ud->uart_pos)
        {
            memmove(ud->uart_buffer, &ud->uart_buffer[count], ud->uart_pos - count);
        }
        ud->uart_pos -= count;

        mutex_exit(&ud->uart_mtx);

        if (count)
        {
            tud_cdc_n_write_flush(0);
        }
    }
}

// rea/write usb data
void usb_cdc_process(void)
{
    uart_data_t *ud = &UART_DATA;

    mutex_enter_blocking(&ud->lc_mtx);
    tud_cdc_n_get_line_coding(0, &ud->usb_lc);
    mutex_exit(&ud->lc_mtx);

    usb_read_bytes();
    usb_write_bytes();
}

inline void dbg_print_usb(uint8_t *msg)
{

    uart_data_t *ud = &UART_DATA;

    mutex_enter_blocking(&ud->uart_mtx);

    while (*msg && (ud->uart_pos < BUFFER_SIZE))
    {
        ud->uart_buffer[ud->uart_pos] = *msg++;
        ud->uart_pos++;
    }

    mutex_exit(&ud->uart_mtx);
}

inline void dbg_putc_usb(uint8_t data)
{
    uart_data_t *ud = &UART_DATA;

    mutex_enter_blocking(&ud->uart_mtx);

    ud->uart_buffer[ud->uart_pos] = data;
    ud->uart_pos++;

    mutex_exit(&ud->uart_mtx);
}

static inline void uart_read_bytes(void)
{
    uart_data_t *ud = &UART_DATA;
    const uart_id_t *ui = &UART_ID;

    if (uart_is_readable(ui->inst))
    {
        mutex_enter_blocking(&ud->uart_mtx);

        while (uart_is_readable(ui->inst) && (ud->uart_pos < BUFFER_SIZE))
        {
            ud->uart_buffer[ud->uart_pos] = uart_getc(ui->inst);
            ud->uart_pos++;
        }

        mutex_exit(&ud->uart_mtx);
    }
}

void uart0_irq_fn(void)
{
    uart_read_bytes();
}


inline void dbg_read_usb(uint8_t *msg)
{
    uart_data_t *ud = &UART_DATA;

    if (ud->usb_pos && mutex_try_enter(&ud->usb_mtx, NULL))
    {
        uint32_t count = 0;

        while (count < ud->usb_pos)
        {
            *msg++ = ud->usb_buffer[count];
            count++;
        }

        if (count < ud->usb_pos)
        {
            memmove(ud->usb_buffer, &ud->usb_buffer[count],
                    ud->usb_pos - count);
        }
        ud->usb_pos -= count;
        mutex_exit(&ud->usb_mtx);
    }
}

void uart_write_bytes(void)
{
    uart_data_t *ud = &UART_DATA;

    if (ud->usb_pos && mutex_try_enter(&ud->usb_mtx, NULL))
    {
        const uart_id_t *ui = &UART_ID;
        uint32_t count = 0;

        while (uart_is_writable(ui->inst) && count < ud->usb_pos)
        {
            uart_putc_raw(ui->inst, ud->usb_buffer[count]);
            count++;
        }

        if (count < ud->usb_pos)
        {
            memmove(ud->usb_buffer, &ud->usb_buffer[count],
                    ud->usb_pos - count);
        }
        ud->usb_pos -= count;
        mutex_exit(&ud->usb_mtx);
    }
}

void init_uart_hw(void)
{
    const uart_id_t *ui = &UART_ID;
    uart_data_t *ud = &UART_DATA;

    /* Pinmux */
    gpio_set_function(ui->tx_pin, GPIO_FUNC_UART);
    gpio_set_function(ui->rx_pin, GPIO_FUNC_UART);

    // enable pullup and invert rx and tx
    gpio_set_pulls(ui->rx_pin, true, false);
    gpio_set_outover(ui->tx_pin, GPIO_OVERRIDE_INVERT);
    gpio_set_inover(ui->rx_pin, GPIO_OVERRIDE_INVERT);

    /* UART start */
    uart_init(ui->inst, ud->usb_lc.bit_rate);
    uart_set_hw_flow(ui->inst, false, false);
    uart_set_format(ui->inst, databits_usb2uart(ud->usb_lc.data_bits),
                    stopbits_usb2uart(ud->usb_lc.stop_bits),
                    parity_usb2uart(ud->usb_lc.parity));
    uart_set_fifo_enabled(ui->inst, false);

    /* UART RX Interrupt */
    irq_set_exclusive_handler(ui->irq, ui->irq_fn);
    irq_set_enabled(ui->irq, true);
    uart_set_irq_enables(ui->inst, true, false);
}

void init_uart_data(void)
{
    const uart_id_t *ui = &UART_ID;
    uart_data_t *ud = &UART_DATA;

    /* USB CDC LC */
    ud->usb_lc.bit_rate = DEF_BIT_RATE;
    ud->usb_lc.data_bits = DEF_DATA_BITS;
    ud->usb_lc.parity = DEF_PARITY;
    ud->usb_lc.stop_bits = DEF_STOP_BITS;

    /* UART LC */
    ud->uart_lc.bit_rate = DEF_BIT_RATE;
    ud->uart_lc.data_bits = DEF_DATA_BITS;
    ud->uart_lc.parity = DEF_PARITY;
    ud->uart_lc.stop_bits = DEF_STOP_BITS;

    /* Buffer */
    ud->uart_pos = 0;
    ud->usb_pos = 0;

    /* Mutex */
    mutex_init(&ud->lc_mtx);
    mutex_init(&ud->uart_mtx);
    mutex_init(&ud->usb_mtx);
}
