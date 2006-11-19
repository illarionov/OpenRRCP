/*
    This file is part of openrrcp

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

    ---

    You can send your updates, patches and suggestions on this software
    to it's original author, Andrew Chernyak (nording@yandex.ru)
    This would be appreciated, however not required.
*/

//#include <sys/types.h>
#include <stdint.h>

extern char ifname[128];
extern unsigned char my_mac[6];
extern unsigned char dest_mac[6];
extern unsigned char mac_bcast[6];
#ifdef __linux__
extern unsigned int if_nametoindex (__const char *__ifname) __THROW;
#endif
// takes logical port number as printed on switch' case (1,2,3...)
// returns physical port number (0,1,2...) or -1 if this device has no such logical port
int map_port_number_from_logical_to_physical(int port);

int map_port_number_from_physical_to_logical(int port);

void rtl83xx_prepare();

//send to wire
//ssize_t sock_send(void *ptr, int size);

//recieve from wire, returns length
//int sock_rec(void *ptr, int size, int waittick);

void rtl83xx_scan(int verbose);

uint32_t rtl83xx_readreg32(uint16_t regno);

uint16_t rtl83xx_readreg16(uint16_t regno);

void rtl83xx_setreg16(uint16_t regno, uint32_t regval);

void rtl83xx_setreg16reg16(uint16_t regno, uint16_t regval);

int do_write_eeprom(uint16_t addr,uint16_t data);

unsigned long rtl83xx_ping(void);
