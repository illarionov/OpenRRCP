/*
    This file is part of OpenRRCP

    Copyright (c) 2009 Alexey Illarionov <littlesavage@rambler.ru>

    OpenRRCP is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    OpenRRCP is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenRRCP; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include <8052.h>
#include "fsd1600.h"

static bit loopback_detection_enabled = 0;
static uint32_t loopback_disabled_ports = 0;
static unsigned long loopback_left_to_recover = 0;

static bit port_can_be_loopback_disabled(uint8_t port) {
   return !(((uint16_t)eeprom_config[EEPROM_PORT_DISABLE_LOOP_DETECT_ADDR])
      &((uint16_t)1<<port));
}

void init_loopback_detection() {

   loopback_detection_enabled=0;

#ifdef LOOPBACK_DETECTION_JUMPER
      loopback_detection_enabled=LOOPBACK_DETECTION_JUMPER;
#else
      loopback_detection_enabled =
	 (eeprom_config[EEPROM_OPENRRCP_CONTROL_ADDR]
	  &OPENRRCP_CONTROL_LOOP_DETECT);
#endif

   if (loopback_detection_enabled) {
      uint16_t tmp;
      tmp = read_16bit_data(0x200);
      /* Loop detect function */
      if ((tmp&0x04) == 0)
	 write_16bit_data(0x200, tmp|0x04);
   }

   loopback_disabled_ports=0;
}

void loopback_detection()
{
   bit loop_detected;
   uint8_t i;

   if (!loopback_detection_enabled)
      return;

   loop_detected = read_16bit_data(0x102)&0x44;

   if (loop_detected) {
      uint32_t port_status;
      port_status = read_32bit_data(0x0101);

      for (i=0;i<16;i++) {
	 if ((port_status & ((uint32_t)1<<i))
	       && (port_can_be_loopback_disabled(i))) {
	    port_disable(i);
	    phy_port_power_down(i);
	    loopback_disabled_ports |= ((uint32_t)1<<i);
	    loopback_left_to_recover = (unsigned long)LOOPBACK_RECOVER_TIME_S*1000/POLLING_INTERVAL_MS-1;
	 }
      }
   }else {
      /* loop not detected */
      if ((loopback_disabled_ports != 0)
	    && (--loopback_left_to_recover==0)) {
	 i=0;
	 for (; (loopback_disabled_ports & ((uint32_t)1<<i))==0; )
	    i++;
	 /* turn on port */
	 phy_port_power_up(i);
	 port_enable(i);
	 loopback_disabled_ports &= ~((uint32_t)1<<i);
	 /* try next port on next iteration */
	 loopback_left_to_recover = 1;
      }
   }
}


