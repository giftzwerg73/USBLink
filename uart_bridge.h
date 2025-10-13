// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 Marcus Schuster <ms@nixmail.com>
 */

#if !defined(_UART_BRIDGE_H_)
#define _UART_BRIDGE_H_

#include <tusb.h>

#define BUFFER_SIZE 2560

#define DEF_BIT_RATE 19200
#define DEF_STOP_BITS 1
#define DEF_PARITY 0
#define DEF_DATA_BITS 8

typedef struct
{
    uart_inst_t *const inst;
    uint irq;
    void *irq_fn;
    uint8_t tx_pin;
    uint8_t rx_pin;
} uart_id_t;

typedef struct
{
    cdc_line_coding_t usb_lc;
    cdc_line_coding_t uart_lc;
    mutex_t lc_mtx;
    uint8_t uart_buffer[BUFFER_SIZE];
    uint32_t uart_pos;
    mutex_t uart_mtx;
    uint8_t usb_buffer[BUFFER_SIZE];
    uint32_t usb_pos;
    mutex_t usb_mtx;
} uart_data_t;

void init_uart_data_usb(void);
void init_uart_hw_usb(void);
void update_uart_cfg_usb(void);
void write_data_usb2uart(void);
void usb_cdc_process(void);
void print_usb(uint8_t *msg);
void putc_usb(uint8_t data);
void read_usb(uint8_t *msg);

#endif /* _UART_BRIDGE_H_ */
