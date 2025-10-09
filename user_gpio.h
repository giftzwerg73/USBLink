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
// defines for button_events
#define bt_undev 0
#define bt_up 1
#define bt_evtdown 2
#define bt_down 3
#define bt_evtup 4
#define bt_evtup_short 5
#define bt_evtup_long 6
// define operating modes
#define opmode_undev 0
#define opmode_esc 1
#define opmode_rec 2
#define opmode_servo 3

void init_gpio(void);
void set_onboard_led(bool led_on);
void set_blue_led(bool led_on);
void set_red_led(bool led_on);
void toggle_blue_led(void);
void toggle_red_led(void);
bool get_vusb(void);
bool get_escpower(void);
bool get_button(void);
uint8_t get_button_state(void);
uint8_t get_escpwr_state(void);
uint8_t opmode_select(void);
bool ceck_escpwr(void);
uint8_t check_button_event(void);
void trigger_reset(void);
void sleep_x10ms(uint32_t wait);

#endif /* _USER_GPIO_H_ */
