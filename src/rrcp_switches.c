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

#include "rrcp_io.h"
#include "rrcp_switches.h"

const uint32_t chipname_n = 3;

char* chipnames [3] = {
    "unknown",
    "rtl8316b",
    "rtl8326"
};

uint32_t switchtype;

const uint32_t switchtype_n = 5;

struct switchtype_t switchtypes[5] = {
    {
	"generic",
	"rtl8316b",
	"unknown",
	"rtl8316b",
	rtl8316b,
	16,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,0,0,0,0,0,0,0,0,0,0,0}
    },
    {
	"generic",
	"rtl8326",
	"unknown",
	"rtl8326",
	rtl8326,
	26,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,0}
    },
    {
	"dlink",
	"des1016d",
	"D1",
	"rtl8316b",
	rtl8316b,
	16,
	{2,1,4,3,6,5,8,7,10,9,12,11,14,13,16,15,0,0,0,0,0,0,0,0,0,0,0}
    },
    {
	"dlink",
	"des1024d",
	"unknown",
	"rtl8326",
	rtl8326,
	24,
	{2,1,4,3,6,5,8,7,10,9,12,11,14,13,16,15,18,17,20,19,22,21,24,23,0,0,0}
    },
    {
	"compex",
	"ps2216",
	"unknown",
	"rtl8316b",
	rtl8316b,
	16,
	{16,13,11,9,7,5,3,1,15,14,12,10,8,6,4,2,0,0,0,0,0,0,0,0,0,0,0}
    }
};

uint16_t rrcp_switch_autodetect_chip(void){
    uint16_t saved_reg,tmp1,tmp2,tmp3;
    uint16_t detected_chiptype=unknown;
    
    //first step - try to write mirroring control register (present on rtl8316b, absent on rtl8326(s))
    saved_reg=rtl83xx_readreg16(0x0219);
    rtl83xx_setreg16(0x0219,0x0000);
    tmp1=rtl83xx_readreg16(0x0219);
    rtl83xx_setreg16(0x0219,0x55aa);
    tmp2=rtl83xx_readreg16(0x0219);
    rtl83xx_setreg16(0x0219,0xffff);
    tmp3=rtl83xx_readreg16(0x0219);
    rtl83xx_setreg16(0x0219,saved_reg);

    if (tmp1==0x0000 && tmp2==0x55aa && tmp3==0xffff){
        detected_chiptype=rtl8316b;
    }else{
	detected_chiptype=rtl8326;
    }
    return(detected_chiptype);
}

uint16_t rrcp_switch_autodetect(void){
    uint16_t detected_chiptype;

    detected_chiptype=rrcp_switch_autodetect_chip();

    if(detected_chiptype==rtl8316b){
	return 0; // generic rtl8316b
    }else{
	return 1; // generic rtl8326
    }
}
