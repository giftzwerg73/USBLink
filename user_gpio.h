// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 Marcus Schuster <ms@nixmail.com>
 */

#if !defined(_USER_GPIO_H_)
#define _USER_GPIO_H_

void init_gpio(void);
void set_onboard_led(bool led_on);
void set_blue_led(bool led_on);
void set_red_led(bool led_on);
void toggle_blue_led(void);
void toggle_red_led(void);
bool get_vusb(void);
bool get_escpower(void); 
bool get_button(void);
void setup_sample_button(void);

#endif /* _USER_GPIO_H_ */
