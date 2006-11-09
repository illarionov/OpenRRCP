/*
    This file is part of openrrcp

    pcapsipdump is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    pcapsipdump is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    ---

    You can send your updates, patches and suggestions on this software
    to it's original author, Andrew Chernyak (nording@yandex.ru)
    This would be appreciated, however not required.
*/

extern char ifname[128];
extern unsigned char my_mac[6];
extern unsigned char dest_mac[6];
extern unsigned char mac_bcast[6];

extern unsigned int if_nametoindex (__const char *__ifname) __THROW;

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

unsigned long rtl83xx_readreg32(unsigned short regno);

unsigned short rtl83xx_readreg16(unsigned short regno);

void rtl83xx_setreg16(unsigned short regno, unsigned long regval);

void rtl83xx_setreg16reg16(unsigned short regno, unsigned short regval);
