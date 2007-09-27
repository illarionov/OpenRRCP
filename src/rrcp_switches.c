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

const uint32_t chipname_n = 7;

char* chipnames [7] = {
    "unknown",
    "rtl8326",
    "rtl8316b",
    "rtl8318",
    "rtl8324",
    "rtl8316bp",
    "rtl8324p"
};

uint32_t switchtype;

/* 
  Internal register vs. EEPROM configuration mapping   
  Format of the table: <internal register>, <EEPROM address (low)>, <quantity of registers in a chain>
  Last value should be -1,0,0
*/

int reg2eeprom_rtl8326_generic[]={ // rtl8326 - compatibility only
    -1,0,0
};

int reg2eeprom_rtl8316b_generic[]={ // generic rtl8316b
    0x0200,0x00c,3,
    0x0300,0x022,1,
    0x0307,0x024,2,
    0x030b,0x028,1,
    0x0400,0x02e,3,
    0x0607,0x038,1,
    0x060a,0x03a,8,
    -1,0,0
};

int reg2eeprom_rtl8324_generic[]={ // generic rtl8324
    0x0200,0x00c,3,
    0x0300,0x022,1,
    0x0307,0x024,2,
    0x030b,0x028,1,
    0x0400,0x02e,3,
    0x0607,0x038,1,
    0x060a,0x03a,12,
    -1,0,0
};

int reg2eeprom_ps2216_6d[]={ // Compex PS2216B 6D with CPU ( VendorId 0x11f67003 & 0x11f67004)
    0x0200,0x00c,3,
    0x0209,0x06a,14,
    0x0219,0x19e,6,
    0x0300,0x022,1,
    0x0307,0x024,2,
    0x030b,0x028,1,
    0x030c,0x092,113,
    0x037d,0x19a,2,
    0x0400,0x02e,3,
    0x0607,0x038,1,
    0x0608,0x17c,2,
    0x060a,0x03a,8,
    -1,0,0
};

int reg2eeprom_rtl8316bp_generic[]={ // generic rtl8316bp
    0x0200,0x00c,3,
    0x0209,0x070,1,
    0x020a,0x080,8,
    0x0219,0x072,1,
    0x021b,0x076,1,
    0x021d,0x07a,1,
    0x0300,0x022,1,
    0x0307,0x024,3,
    0x030b,0x028,1,
    0x030c,0x098,8,
    0x0319,0x0b2,2,
    0x031d,0x0ba,97,
    0x0400,0x02e,3,
    0x0607,0x038,1,
    0x0608,0x068,1,
    0x060a,0x03a,8,
    -1,0,0
};

int reg2eeprom_rtl8324p_generic[]={ // generic rtl8324p
    0x0200,0x00c,3,
    0x0209,0x070,1,
    0x020a,0x080,12,
    0x0219,0x072,1,
    0x021b,0x076,4,
    0x0300,0x022,1,
    0x0307,0x024,3,
    0x030b,0x028,1,
    0x030c,0x098,12,
    0x0319,0x0b2,3,
    0x031d,0x0ba,98,
    0x0400,0x02e,3,
    0x0607,0x038,1,
    0x0608,0x068,2,
    0x060a,0x03a,12,
    -1,0,0
};

/* 
  Default values   
  Format of the table: <internal register>, <default value>, <quantity of registers in a chain>
  If the register here is not specified, its value is equal 0
  Last value should be -1,0,0
*/

int defregval_rtl8326_generic[]={   // generic rtl8326
     0x0300,0x4,1,
     0x0400,0x10,1,
     0x0607,0x10,1,
     0x060a,0xafaf,12,
     0x0616,0xbfbf,1
    -1,0,0
};

int defregval_rtl8316b_generic[]={ // generic rtl8316b & rtl8316bp 
     0x0209,0x2379,1,
     0x0300,0x4,1,
     0x0307,0x8200,1,
     0x030c,0x100,1,
     0x030d,0x302,1,
     0x030e,0x504,1,
     0x030f,0x706,1,
     0x0310,0x908,1,
     0x0311,0x0b0a,1,
     0x0312,0x0d0c,1,
     0x0313,0x0f0e,1,
     0x0319,0x0ffff,2,
     0x031d,0x0c001,1,
     0x0320,0x0c002,1,
     0x0323,0x0c004,1,
     0x0326,0x0c008,1,
     0x0329,0x0c010,1,
     0x032c,0x0c020,1,
     0x032f,0x0c040,1,
     0x0332,0x0c080,1,
     0x0335,0x0c100,1,
     0x0338,0x0c200,1,
     0x033b,0x0c400,1,
     0x033e,0x0c800,1,
     0x0341,0x0d000,1,
     0x0344,0x0e000,1,
     0x0347,0x0ffff,1,
     0x034a,0x0ffff,1,
     0x0400,0x10,1,
     0x0607,0x10,1,
     0x060a,0xafaf,8,
     -1,0,0
};

int defregval_rtl8324_generic[]={   // generic rtl8324 & rtl8324p
     0x0209,0x2379,1,
     0x0300,0x4,1,
     0x0307,0x8200,1,
     0x030c,0x100,1,
     0x030d,0x302,1,
     0x030e,0x504,1,
     0x030f,0x706,1,
     0x0310,0x908,1,
     0x0311,0x0b0a,1,
     0x0312,0x0d0c,1,
     0x0313,0x0f0e,1,
     0x0314,0x1110,1,
     0x0315,0x1312,1,
     0x0316,0x1514,1,
     0x0317,0x1716,1,
     0x0319,0x0ffff,3,
     0x031d,0x00001,1,
     0x031e,0x000c0,1,
     0x0320,0x00002,1,
     0x0321,0x000c0,1,
     0x0323,0x00004,1,
     0x0324,0x000c0,1,
     0x0326,0x00008,1,
     0x0327,0x000c0,1,
     0x0329,0x00010,1,
     0x032a,0x000c0,1,
     0x032c,0x00020,1,
     0x032d,0x000c0,1,
     0x032f,0x00040,1,
     0x0330,0x000c0,1,
     0x0332,0x00080,1,
     0x0333,0x000c0,1,
     0x0335,0x00100,1,
     0x0336,0x000c0,1,
     0x0338,0x00200,1,
     0x0339,0x000c0,1,
     0x033b,0x00400,1,
     0x033c,0x000c0,1,
     0x033e,0x00800,1,
     0x033f,0x000c0,1,
     0x0341,0x01000,1,
     0x0342,0x000c0,1,
     0x0344,0x02000,1,
     0x0345,0x000c0,1,
     0x0347,0x04000,1,
     0x0348,0x000c0,1,
     0x034a,0x08000,1,
     0x034b,0x000c0,1,
     0x034e,0x000c1,1,
     0x0351,0x000c2,1,
     0x0354,0x000c4,1,
     0x0357,0x000c8,1,
     0x035a,0x000d0,1,
     0x035d,0x000e0,1,
     0x035f,0x0ffff,1,
     0x0360,0x000ff,1,
     0x0362,0x0ffff,1,
     0x0363,0x000ff,1,
     0x0400,0x10,1,
     0x0607,0x10,1,
     0x060a,0xafaf,12,
     -1,0,0
};

const uint32_t switchtype_n = 14;

struct switchtype_t switchtypes[14] = {
    {
	"generic",
	"rtl8326",
	"unknown",
	"rtl8326",
	rtl8326,
	26,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,0},
        &reg2eeprom_rtl8326_generic[0],
        &defregval_rtl8326_generic[0]
    },
    {
	"generic",
	"rtl8316b",
	"unknown",
	"rtl8316b",
	rtl8316b,
	16,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,0,0,0,0,0,0,0,0,0,0,0},
        &reg2eeprom_rtl8316b_generic[0],
        &defregval_rtl8316b_generic[0]
    },
    {
	"generic",
	"rtl8318",
	"unknown",
	"rtl8318",
	rtl8318,
	18,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,0,0,0,0,0,0,0,0,0},
        &reg2eeprom_rtl8324_generic[0],   // surprise from Realtek :)
        &defregval_rtl8324_generic[0]
    },
    {
	"generic",
	"rtl8324",
	"unknown",
	"rtl8324",
	rtl8324,
	24,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,0,0,0},
        &reg2eeprom_rtl8324_generic[0],
        &defregval_rtl8324_generic[0]
    },
    {
	"dlink",
	"des1016d",
	"D1",
	"rtl8316b",
	rtl8316b,
	16,
	{2,1,4,3,6,5,8,7,10,9,12,11,14,13,16,15,0,0,0,0,0,0,0,0,0,0,0},
        &reg2eeprom_rtl8316b_generic[0],
        &defregval_rtl8316b_generic[0]
    },
    {
	"dlink",
	"des1024d_b1",
	"B1",
	"rtl8326",
	rtl8326,
	24,
	{2,1,4,3,6,5,8,7,10,9,12,11,14,13,16,15,18,17,20,19,22,21,24,23,0,0,0},
        &reg2eeprom_rtl8326_generic[0],
        &defregval_rtl8326_generic[0]
    },
    {
	"dlink",
	"des1024d_c1",
	"C1",
	"rtl8324",
	rtl8324,
	24,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,0,0,0},
        &reg2eeprom_rtl8324_generic[0],
        &defregval_rtl8324_generic[0]
    },
    {
	"compex",
	"ps2216",
	"unknown",
	"rtl8316b",
	rtl8316b,
	16,
	{16,13,11,9,7,5,3,1,15,14,12,10,8,6,4,2,0,0,0,0,0,0,0,0,0,0,0},
        &reg2eeprom_rtl8316b_generic[0],
        &defregval_rtl8316b_generic[0]
    },
    {
	"ovislink",
	"fsh2402gt",
	"unknown",
	"rtl8326",
	rtl8326,
	26,
	{2,4,6,8,10,12,14,16,18,20,22,24,1,3,5,7,9,11,13,15,17,19,21,23,25,26,0},
        &reg2eeprom_rtl8326_generic[0],
        &defregval_rtl8326_generic[0]
    },
    {
	"zyxel",
	"es116p",
	"unknown",
	"rtl8316b",
	rtl8316b,
	16,
	{2,4,6,8,10,12,14,16,1,3,5,7,9,11,13,15,0,0,0,0,0,0,0,0,0,0,0},
        &reg2eeprom_rtl8316b_generic[0],
        &defregval_rtl8316b_generic[0]
    },
    {
	"compex",
	"sds1224",
	"unknown",
	"rtl8324",
	rtl8324,
	24,
	{2,1,4,3,6,5,8,7,10,9,12,11,14,13,16,15,18,17,20,19,22,21,24,23,0,0,0},
        &reg2eeprom_rtl8324_generic[0],
        &defregval_rtl8324_generic[0]
    },
    {
	"signamax",
	"065-7531a",
	"unknown",
	"rtl8316b",
	rtl8316b,
	16,
	{1,3,5,7,9,11,13,15,2,4,6,8,10,12,14,16,0,0,0,0,0,0,0,0,0,0,0},
        &reg2eeprom_rtl8316b_generic[0],
        &defregval_rtl8316b_generic[0]
    },
    {
	"compex",
	"ps2216",
	"6D", // vendorId 0x11f67003 & 0x11f67004
	"rtl8316b",
	rtl8316b,
	16,
	{16,13,11,9,7,5,3,1,15,14,12,10,8,6,4,2,0,0,0,0,0,0,0,0,0,0,0},
        &reg2eeprom_ps2216_6d[0],
        &defregval_rtl8316b_generic[0]
    },
    {
	"compex",
	"ps2216",
	"6DP", // vendorId 0x11f67005
	"rtl8316bp",
	rtl8316bp,
	16,
	{16,13,11,9,7,5,3,1,15,14,12,10,8,6,4,2,0,0,0,0,0,0,0,0,0,0,0},
        &reg2eeprom_rtl8316b_generic[0],
        &defregval_rtl8316b_generic[0]
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
      if ((uint8_t)rtl83xx_readreg16(0x0218) != test2[i]) {
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
    return ((tmp21==(uint8_t)(tmp11+0x055))&&(tmp22==(uint8_t)(tmp11+0x0aa)));
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
	if (!eeprom_read(0x7f,&test1[0])){
	    detected_eeprom=EEPROM_2401;
	    if (!eeprom_read(0xff,&test1[0])){
		detected_eeprom=EEPROM_2402;
		if (!eeprom_read(0x1ff,&test1[0])){
		    detected_eeprom=EEPROM_2404;
		    if (!eeprom_read(0x3ff,&test1[0])){
			detected_eeprom=EEPROM_2408;
			if (!eeprom_read(0x7ff,&test1[0])){
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
