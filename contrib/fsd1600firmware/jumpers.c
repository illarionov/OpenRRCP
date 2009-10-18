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

void init_standart_jumpers()
{
      uint16_t tmp, orig;
/* 3 - VLAN topology type selection
 *     ON -  15VLANs(port#1-15) with 1 overlapping port(#16)
 *     OFF - 14VLANs(port#1-14) with 2 overlapping ports
 * 4 - Port based VLAN
 *     ON - enable VLAN
 *     OFF - no VLAN
 */
#if defined(PORT_BASED_VLAN_JUMPER) && defined(VLAN_TOPOLOGY_JUMPER)
      if (PORT_BASED_VLAN_JUMPER) {
	 uint8_t i;
	 uint16_t mask;

	 tmp = read_16bit_data(0x30b);
	 write_16bit_data(0x30b, (tmp & 0xee) | 0x01);

	 write_16bit_data(0x037d, 0);
	 write_16bit_data(0x031d+3*15, 0xffff);
	 write_16bit_data(0x031d+3*15+1, 0);
	 write_16bit_data(0x031d+3*15+2, 0);

	 mask = 1;
	 for (i=0; i < 15; i++) {
	    if (i % 2 == 0)
	       write_16bit_data(0x030c+i/2, i | ((i+1)<<8));
	    write_16bit_data(0x031d+3*i,
		  mask | (VLAN_TOPOLOGY_JUMPER ? 0x8000 : 0xc000));
	    write_16bit_data(0x031d+3*i+1, 0);
	    write_16bit_data(0x031d+3*i+2, 0);
	    mask <<= 1;
	 }
	 if (VLAN_TOPOLOGY_JUMPER==0)
	    write_16bit_data(0x031d+3*14, 0xffff);
      }
#endif

/* 5 - Port trunk 0 (port 1,2,3,4)
 * 6 - Port trunk 1 (port 5,6,7,8)
 * 7 - Port trunk 2 (port 9,10,11,12)
 * 8 - Port trunk 3 (port 13,14,15,16)
 *     ON - enable trunk group
 *     OFF - disable
 */
#if defined(PORT_TRUNK0_JUMPER) || defined(PORT_TRUNK1_JUMPER) || defined(PORT_TRUNK2_JUMPER) || defined(PORT_TRUNK3_JUMPER)
      orig = tmp = read_16bit_data(0x0307);
      tmp &= 0xffe1;

#ifdef PORT_TRUNK0_JUMPER
      if (PORT_TRUNK0_JUMPER)
	 tmp |= 0x02;
#endif

#ifdef PORT_TRUNK1_JUMPER
      if (PORT_TRUNK1_JUMPER)
	 tmp |= 0x04;
#endif

#ifdef PORT_TRUNK2_JUMPER
      if (PORT_TRUNK2_JUMPER)
	 tmp |= 0x08;
#endif

#ifdef PORT_TRUNK3_JUMPER
      if (PORT_TRUNK3_JUMPER)
	 tmp |= 0x10;
#endif
      if (tmp != orig)
	 write_16bit_data(0x0307, tmp);

      /*
      if ( tmp & 0x1e) {
	 uint16_t tmp2;
	 tmp2 = read_16bit_data(0x30b);
	 write_16bit_data(0x30b, tmp2 & 0xee);
      }
      */
#endif

/* 9 -  TCP/IP(Diffserv) bases QOS
 * 10 - 801Q VLAN Tag Priority Based QoS
 *      ON  - Enable
 *      OFF - Disable QoS
 */
#if defined(DIFFSERV_PRIORITY_BASED_QOS_JUMPER) || defined(VLAN_TAG_PRIORITY_BASED_QOS_JUMPER)
      tmp = read_16bit_data(0x0400);

#ifdef DIFFSERV_PRIORITY_BASED_QOS_JUMPER
      if (DIFFSERV_PRIORITY_BASED_QOS_JUMPER)
	 tmp |= 0x01;
      else
	 tmp &= 0xfe;
#endif

#ifdef VLAN_TAG_PRIORITY_BASED_QOS_JUMPER
      if (DIFFSERV_PRIORITY_BASED_QOS_JUMPER)
	 tmp |= 0x02;
      else
	 tmp &= 0xfd;
#endif

      write_16bit_data(0x0400, tmp);
#endif

/* 11,12 - Port Based Priority QoS
 *     OFF, OFF - Disable
 *     OFF, ON  - port 1-2 High priority ports
 *     ON,  OFF - port 1-4 High Priority ports
 *     ON,  ON  - port 1-8 High Priority ports
 */
#if defined(PORT_BASED_PRIORITY_QOS_JUMPER1) && defined(PORT_BASED_PRIORITY_QOS_JUMPER2)
      {
	 uint8_t mask;
	 if (!PORT_BASED_PRIORITY_QOS_JUMPER1
	       && !PORT_BASED_PRIORITY_QOS_JUMPER2)
	    mask=0;
	 else if (!PORT_BASED_PRIORITY_QOS_JUMPER1
	       && PORT_BASED_PRIORITY_QOS_JUMPER2)
	    mask=0x03;
	 else if (PORT_BASED_PRIORITY_QOS_JUMPER1
	       && !PORT_BASED_PRIORITY_QOS_JUMPER2)
	    mask=0x0f;
	 else
	    mask=0xff;
	 write_16bit_data(0x0401, mask);
      }
#endif

}

