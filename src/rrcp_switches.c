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
#include <stdio.h>
#include <string.h>
#include "rrcp_io.h"
#include "rrcp_config.h"
#include "rrcp_switches.h"

const uint32_t chipname_n = 5;

char* chipnames [5] = {
    "unknown",
    "rtl8326",
    "rtl8316b",
    "rtl8318",
    "rtl8324"
};

uint32_t switchtype;

const uint32_t switchtype_n = 10;

struct switchtype_t switchtypes[10] = {
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
	"rtl8318",
	"unknown",
	"rtl8318",
	rtl8318,
	18,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,0,0,0,0,0,0,0,0,0}
    },
    {
	"generic",
	"rtl8324",
	"unknown",
	"rtl8324",
	rtl8324,
	24,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,0,0,0}
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
	"des1024d_b1",
	"B1",
	"rtl8326",
	rtl8326,
	24,
	{2,1,4,3,6,5,8,7,10,9,12,11,14,13,16,15,18,17,20,19,22,21,24,23,0,0,0}
    },
    {
	"dlink",
	"des1024d_c1",
	"C1",
	"rtl8324",
	rtl8324,
	24,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,0,0,0}
    },
    {
	"compex",
	"ps2216",
	"unknown",
	"rtl8316b",
	rtl8316b,
	16,
	{16,13,11,9,7,5,3,1,15,14,12,10,8,6,4,2,0,0,0,0,0,0,0,0,0,0,0}
    },
    {
	"ovislink",
	"fsh2402gt",
	"unknown",
	"rtl8326",
	rtl8326,
	26,
	{2,4,6,8,10,12,14,16,18,20,22,24,1,3,5,7,9,11,13,15,17,19,21,23,25,26,0}
    },
    {
	"zyxel",
	"es116p",
	"unknown",
	"rtl8316b",
	rtl8316b,
	16,
	{2,4,6,8,10,12,14,16,1,3,5,7,9,11,13,15,0,0,0,0,0,0,0,0,0,0,0}
    }
};

uint16_t rrcp_switch_autodetect_chip(void){
    uint16_t saved_reg;
    uint16_t detected_chiptype=unknown;
    int i,errcnt=0;
    uint8_t test1[6];
    uint8_t test2[4]={0x0,0x55,0xaa,0xff};

   /*
     step 1: detect rtl8316b with EEPROM
   */
    for(i=0;i<6;i++){
      if ((errcnt=eeprom_read(0x12+i,&test1[i]))!=0){break;}
    }
    if (errcnt==0){
      if (memcmp(&dest_mac[0],&test1[0],6)==0) {
       // here it is possible to note, that the device is equipped with EEPROM
       return(rtl8316b);
      }
    }
    /*
      step 2: if step 1 fail, detect rtl8316b without EEPROM
    */
    saved_reg=rtl83xx_readreg16(0x0218);
    for(i=0;i<4;i++){
      rtl83xx_setreg16(0x0218,test2[i]);
      if (rtl83xx_readreg16(0x0218) != test2[i]) {
        errcnt++; 
        break;
      }
    }
    rtl83xx_setreg16(0x0218,saved_reg);

    if (errcnt) {
        detected_chiptype=rtl8326;
    }else{
	detected_chiptype=rtl8316b;
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

////////////////////////
int rrcp_autodetectchip_try_to_write_eeprom (uint16_t addr1, uint16_t addr2)
{
    uint8_t tmp11=0, tmp12=0, tmp21=0, tmp22=0;

    eeprom_read(addr1,&tmp11);
    eeprom_read(addr2,&tmp12);
    eeprom_write(addr1,tmp11+0x055);
    eeprom_write(addr2,tmp11+0x0aa);
    tmp21=tmp11;
    tmp22=tmp11;
    eeprom_read(addr1,&tmp21);
    eeprom_read(addr2,&tmp22);
    eeprom_write(addr1,tmp11);
    eeprom_write(addr2,tmp12);
    return ((tmp21==tmp11+0x055)&&(tmp22==tmp11+0x0aa));
}

uint8_t rrcp_autodetectswitch_port_count(void){
    uint16_t r;
    uint16_t port_count;

    if (phy_read(8,3,&r)==0){
	if (r==0xffff){
	    port_count=16;
	}else{
	    port_count=24;
	}
    }else{
	port_count=26;
    }

    return port_count;
}

uint16_t rrcp_autodetect_switch_chip_eeprom(uint8_t *switch_type, uint8_t *chip_type, t_eeprom_type *eeprom_type){
    uint16_t saved_reg;
    uint16_t detected_switchtype=-1;
    uint16_t detected_chiptype=unknown;
    t_eeprom_type detected_eeprom=EEPROM_NONE;
    int i,errcnt=0;
    uint8_t test1[6];
    uint8_t test2[4]={0x0,0x55,0xaa,0xff};
    uint8_t port_count;

    // step 1: detect number of ports
    port_count=rrcp_autodetectswitch_port_count();    

    // step 2: detect EEPROM presence and size
    for(i=0;i<6;i++){
	if ((errcnt=eeprom_read(0x12+i,&test1[i]))!=0){break;}
    }
    if (errcnt==0){
	if (rrcp_autodetectchip_try_to_write_eeprom(0x07e,0x07f)){
	    detected_eeprom=EEPROM_2401;
	    if (rrcp_autodetectchip_try_to_write_eeprom(0x07f,0x0ff)){
		detected_eeprom=EEPROM_2402;
		if (rrcp_autodetectchip_try_to_write_eeprom(0x0ff,0x01ff)){
		    detected_eeprom=EEPROM_2404;
		    if (rrcp_autodetectchip_try_to_write_eeprom(0x01ff,0x03ff)){
			detected_eeprom=EEPROM_2408;
			if (rrcp_autodetectchip_try_to_write_eeprom(0x03ff,0x07ff)){
			    detected_eeprom=EEPROM_2416;
			}
		    }
		}
	    }
	}else{
	    detected_eeprom=EEPROM_WRITEPOTECTED;
	}
    }

    // step 3: check for registers, absent on rtl8326
    saved_reg=rtl83xx_readreg16(0x0218);
    for(i=0;i<4;i++){
	rtl83xx_setreg16(0x0218,test2[i]);
	if (rtl83xx_readreg16(0x0218) != test2[i]) {
	    errcnt++; 
	    break;
	}
    }
    rtl83xx_setreg16(0x0218,saved_reg);

    // make final decision
    if (errcnt) {
        detected_chiptype=rtl8326;
	detected_switchtype=0; // generic rtl8326
    }else if (port_count==16){
	detected_chiptype=rtl8316b;
	detected_switchtype=1; // generic rtl8316b
    }else if (port_count==18){
        detected_chiptype=rtl8318;
        detected_switchtype=2; // generic rtl8318
    }else{
        detected_chiptype=rtl8324;
        detected_switchtype=3; // generic rtl8324
    }

    *switch_type=detected_switchtype;
    *chip_type=detected_chiptype;
    *eeprom_type=detected_eeprom;
    return 0;
}
