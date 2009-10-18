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
#include <setjmp.h>
#include "fsd1600.h"

volatile jmp_buf reboot;

const struct t_regeeprom_map {
   uint16_t sw_first_reg;
   uint16_t eeprom_first_addr;
   uint8_t reg_cnt;
} reg2eeprom[]={ // generic rtl8316bp
   {0x0005,0x004/2,1}, /* LED display configuration 0 */
   {0x020a,0x080/2,8}, /* BW control registers p0-p15*/
   {0x0219,0x072/2,1}, /* port mirror control p0-p15 */
   {0x021b,0x076/2,1}, /* RX mirror port mask p0-p15 */
   {0x021d,0x07a/2,1}, /* TX mirror port mask p0-p15 */
   {0x0300,0x022/2,1}, /* Alt configuration */
   {0x0307,0x024/2,3}, /* port trunking */
   {0x030b,0x028/2,1}, /* vlan control */
   {0x030c,0x098/2,8}, /* port vlan id assigment */
   {0x0319,0x0b2/2,2}, /* VLAN TX Priority Tagging */
   {0x031d,0x0ba/2,97},/* Port VLAN configuration, per-port VID enabling */
   {0x0400,0x02e/2,3}, /* QoS control */
   {0x0607,0x038/2,1}, /* global port control */
   {0x0608,0x068/2,1}, /* port disable control */
   {0x060a,0x03a/2,8}, /* port property configuration */
   {0x0209,0x070/2,1}, /* RRCP Password */
   {0x0200,0x00c/2,3}, /* RRCP, RRCP security masks */
};

const char openrrcp_message[] = {"\n\nThis is GNU GPL code and full source is available from OpenRRCP project\n\n"};

static void load_eeprom_config() {
   uint8_t i,j;

   for (i=0; i < sizeof(reg2eeprom)/sizeof(reg2eeprom[0]); i++) {
      for(j=0; j<reg2eeprom[i].reg_cnt; j++) {
	 write_16bit_data(
	       reg2eeprom[i].sw_first_reg + j,
	       eeprom_config[reg2eeprom[i].eeprom_first_addr+j]);
	 RESET_WATCHDOG
      }
   }
}

static void load_port_powerdown_register() {
   uint16_t c;
   uint8_t i;

   c=eeprom_config[EEPROM_PORT_PHY_POWERDOWN_ADDR];
   for (i=0; c ; c>>=1, i++ ) {
      if (c&1)
	 phy_port_power_down(i);
      RESET_WATCHDOG
   }
   return;
}

static void init_counter_types() {
   uint8_t i;
   for (i=0;i<15;i++){
      /* count RX bytes, TX bytes, Dropped bytes */
      write_16bit_data(0x0700+i, 0x00);
      RESET_WATCHDOG
   }
}

static void init_rrcp_enable_jumper() {
#ifdef RRCP_ENABLE_JUMPER
   uint16_t tmp;
   tmp = read_16bit_data(0x200);

   if(RRCP_ENABLE_JUMPER) {
      if ((tmp&0x01)!=0)
         write_16bit_data(0x200, tmp&0xfffe);
   }else {
      if ((tmp&0x01) == 0)
	 write_16bit_data(0x200, tmp|0x01);
   }
//   write_16bit_data(0x0209,eeprom_config[0x070/2]); /* authkey */
#endif
}

static void check_mac() {
   uint16_t tmp;
   uint8_t i;
   for(i=0;i<3;i++){
      tmp = read_16bit_data(0x0203+i);
      if (tmp != 0) {
	 write_16bit_data(0x0, 0x02);
	 longjmp(reboot,0);
      }
   }
}

void main(void)
{
   IE=0;
   SCONF=1;
   INIT_WATCHDOG
//   P3 = P2 = P1 = P0 = 0xff;
//   for(;SCK;) RESET_WATCHDOG;
   P1=0xff;

   /* REBOOT start point */
   setjmp(reboot);
   P3 = P2 = P0 = 0xff;
   SCK=0;
   SDA=1;

   init_timer();
   /* wait 800 ms */
   reset_ms_timer(800);
   wait_ms_timer(0,0);
   SDA=SCK=1;
   wait_period();
   stop_ms_timer();

   RESET_WATCHDOG

   load_eeprom_config();
   load_port_powerdown_register();
   init_standart_jumpers();
   init_counter_types();
   init_loopback_detection();

   reset_ms_timer(POLLING_INTERVAL_MS);
   port_tester();

   for(;;) {
      loopback_detection();
      init_rrcp_enable_jumper();
      check_mac();
      wait_ms_timer(1,1);
   }
}

