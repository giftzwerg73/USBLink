// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 Marcus Schuster <ms@nixmail.com>
 */

#if !defined(_USER_GPIO_H_)
#define _USER_GPIO_H_

// pin definition
#define ESC_PWR_PIN 2
#define SW_PIN 15
#define LED_PIN_BLUE 16
#define LED_PIN_RED 17
// pins for receiver and servo mode
#define RECV_CH1_PIN 13
#define SERV_CH1_PIN 12

void init_gpio(void);
void set_onboard_led(bool led_on);
void set_blue_led(bool led_on);
void set_red_led(bool led_on);
void toggle_blue_led(void);
void toggle_red_led(void);
bool get_vusb(void);
bool get_escpower(void);
bool get_button(void);
void sample_button(void);
void sample_escpwr(void);
uint8_t get_button_state(void);
uint8_t get_escpwr_state(void);
uint8_t opmode_select(void);

#endif /* _USER_GPIO_H_ */
