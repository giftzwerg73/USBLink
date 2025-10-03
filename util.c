// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 Marcus Schuster <ms@nixmail.com>
 */
 
#include <pico/stdlib.h>
#include <string.h>
#include <pico/time.h>


bool sample_button (struct repeating_timer *t)
{
        btn[0] = gpio_get(SW_PIN);
		if(btn[0] == 0 && btn[1] == 1)
		{
			btn[1] = btn[0];
			dbg_print_usb(1, "Button pressed\n");
		}
		else if(btn[0] == 1 && btn[1] == 0)
		{
			btn[1] = btn[0];
			dbg_print_usb(1, "Button released\n");
		}
  return true;
}

void setup() 
{
  struct repeating_timer timer;

  // NB: A negative period means "start to start" - i.e. regardless of how long the callback takes to run
  long usPeriod = -1000000/32000;
  add_repeating_timer_us(usPeriod, sample_button, NULL, &timer); 
}
