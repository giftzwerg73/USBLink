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
#include "user_gpio.h"
#include "uart_bridge.h"
#include "usb_descriptors.h"


// main program core 1
void core1_entry(void)
{
	int itf;
	int con[CFG_TUD_CDC][2];
	
	for (itf=0;itf<CFG_TUD_CDC;itf++) 
	{
	    con[itf][0] = 0;
	    con[itf][1] = con[itf][0]; 
	    if(itf == 0)
	    {
	        set_onboard_led(con[itf][0]); 
		}
    }
	
	tusb_init();

	while (1) 
	{
		tud_task();

		for (itf=0;itf<CFG_TUD_CDC;itf++) 
		{
			if (tud_cdc_n_connected(itf)) 
			{
			    con[itf][0] = 1;
				usb_cdc_process(itf);
			}
			else
			{
				con[itf][0] = 0;
			}
			
			if(con[itf][0] != con[itf][1])
            {   
				con[itf][1] = con[itf][0];
			    if(itf == 0)
	            {
			        set_onboard_led(con[itf][0]); 
			    }
		    } 
		}
	}
}


// main program core 0
int main(void)
{
	int itf;
	
	init_gpio();

	usbd_serial_init();

	for (itf=0;itf<CFG_TUD_CDC;itf++)
	{
		init_uart_data(itf);
	}
   
	multicore_launch_core1(core1_entry);

	while (1) {
		for (itf = 0; itf < CFG_TUD_CDC; itf++) 
		{
			update_uart_cfg(itf);
			uart_write_bytes(itf);
		}
	}

	return 0;
}
