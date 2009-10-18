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

void port_tester() {
   bit enabled;
   uint8_t i;

#ifdef PORT_TESTER_JUMPER
   enabled = PORT_TESTER_JUMPER;
#else
   enabled = (eeprom_config[EEPROM_OPENRRCP_CONTROL_ADDR]
	 &OPENRRCP_CONTROL_PORT_TESTER);
#endif

   if(!enabled)
     return;

   /* RTL8208 cable tester */

   phy_write(1, 24, 0x0780); /* stop all cable tests */

   for (i=0; i<16;i++) {
      uint8_t j;
      uint8_t port;
      bit port_timeout;

      /* Use Port 1 of PHY */
      port = i < 8 ? 1 : 9;
      port_timeout = 1;

      /* start cable test.*/
      phy_write(port, 29, 0x17d0);
      phy_write(port, 24, 0x0f80|((uint16_t)i<<13));
      /* wait up to 0.5 sec for results */
      for(j=0;j<40;j++) {
	 uint16_t tmp;
	 wait_ms(10);
	 phy_read(port, 30, &tmp);
	 if (tmp & 0x8000) {
	    port_timeout=0;
	    break;
	 }
      }
      phy_write(port, 24, 0x0780); /* stop all cable tests */
      if (port_timeout) {
	 /* port does not respond. Mark as broken */
	 port_disable(i);
	 phy_port_power_down(i);
      }
   }
}

