Raspberry Pi Pico USBLink
=========================

This program creates a USB to OneWire UART (UART0) bridge on the uniesc hardware. It also adds a servo and receiver tester.

Disclaimer
----------

This software is provided without warranty, according to the MIT License, and should therefore not be used where it may endanger life, financial stakes, or cause discomfort and inconvenience to others.

Raspberry Pi Pico Pinout
------------------------

| Raspberry Pi Pico GPIO | Function |
|:----------------------:|:--------:|
| GPIO12 (Pin 16)        | UART0 TX |
| GPIO13 (Pin 17)        | UART0 RX |

Signals TX and RX are connected through open-collector buffers (inverting) to the OneWire Signal.
