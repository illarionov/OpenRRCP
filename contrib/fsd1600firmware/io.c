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

#define PHYS2PHY(_portno) ((_portno)+16)

volatile static unsigned long milliSeconds;
volatile static unsigned long ReloadMilliSeconds;
volatile static __bit tf1;
volatile static __bit tf0;
volatile static __bit tf2;

extern volatile jmp_buf reboot;

void ClockIrqHandler (void) interrupt 1 {
     TL0 = TIMER0_RELOAD_VALUE&0xff;
     TH0 = TIMER0_RELOAD_VALUE>>8;
     tf0=1;
}

void ClockIrqHandler1 (void) interrupt 3 using 2 {
     TL1 = TIMER1_RELOAD_VALUE&0xff;
     TH1 = TIMER1_RELOAD_VALUE>>8;
     if (milliSeconds==0) {
	tf1=1;
	milliSeconds = ReloadMilliSeconds;
     }
     milliSeconds--;
     RESET_WATCHDOG
}

void init_timer() {
   TR0 = 0; // stop timer 0

   TL0 = TIMER0_RELOAD_VALUE&0xff;
   TH0 = TIMER0_RELOAD_VALUE>>8;
   TMOD = (TMOD&0xf0)|0x01; // T0=16bit timer
   ET0=0; /* timer 0 interrupt */

   TR0=1; // start timer 0
   ET0=1;

   EA=1; /* global interrupts */
}

/* input: ET0=1 */
static void wait_half_period0(bit check_sck, bit check_sda) {
   for(tf0=0;tf0==0;) {
      if (check_sck && (SCK == 0))
	 longjmp(reboot, 0);
      if (check_sda && (SDA == 0))
	 longjmp(reboot, 0);
   }
}

void wait_half_period() {
   for(tf0=0;tf0==0;);
}

void wait_half_period_sck() {
   wait_half_period0(1,0);
}

void wait_half_period_sck_sda() {
   wait_half_period0(1,1);
}

static void wait_period0(bit check_sck, bit check_sda) {
   wait_half_period0(check_sck, check_sda);
   wait_half_period0(check_sck, check_sda);
}

void wait_period() {
   for(tf0=0;tf0==0;);
   for(tf0=0;tf0==0;);
}

void wait_period_sck() {
   wait_half_period0(1,0);
   wait_half_period0(1,0);
}


void wait_period_sck_sda() {
   wait_half_period0(1,1);
   wait_half_period0(1,1);
}


void reset_ms_timer(unsigned long ms) {
   ET1 = 0;   /* stop timer interrupts */
   TR1 = 0; /* stop timer */

   TMOD = (TMOD&0x0f)|0x10; // T0=16bit timer
   TL1 = TIMER1_RELOAD_VALUE&0xff;
   TH1 = TIMER1_RELOAD_VALUE>>8;

   milliSeconds = ReloadMilliSeconds = ms;
   tf1=0;

   TR1=1; // start timer 1
   ET1=1; /* timer 1 interrupt */
   EA=1;
}

void wait_ms_timer(bit check_sck, bit check_sda) {
   ET0=0;
   for(tf1=0;tf1==0;) {
      if (check_sck && (SCK == 0))
	 longjmp(reboot, 0);
      if (check_sda && (SDA == 0))
	 longjmp(reboot, 0);
   }
   ET0=1;
}

void stop_ms_timer() {
   ET1=0;
}

void wait_ms(unsigned long ms) {
   unsigned long last_ms;

   ET1=0;
   last_ms=milliSeconds;
   ET1=1;

   while (ms) {
      ET1=0;
      if (last_ms != milliSeconds)
	 ms--;
      last_ms = milliSeconds;
      ET1=1;
   }
}

/* input:  SDA=LOW, SCK=LOW, on 1/2 of period, ET0=1
 * output: SDA=LOW, SCK=LOW, on 1/2 of period
 * 0 - OK, 1 - fail
 */
static bit send_byte(uint8_t b) {
   uint8_t i;
   bit res;
   bit cur;
   for (i=0;i<8;i++) {
      /* change bit */
      SDA = cur = ((b&0x80) != 0);
      wait_half_period();
      /* send clock pulse */
      SCK=1;
      wait_period0(1,cur);
      SCK=0;
      wait_half_period();

      b <<= 1;
   }

   /* wait for ack */
   SDA=1;
   res = 0;

   wait_half_period();
   SCK=1;
   for (i=0; i < 5; i++) {
      wait_period_sck();
      res = SDA;
      if (res == 0)
	 break;
   }

   SCK=0;
   wait_half_period();
   SDA=0;

   return res;
}

/* input:  SDA=LOW, SCK=LOW, on 1/2 of period, ET0=1
 * output: SDA=LOW, SCK=LOW, on 1/2 of period
 */
static uint8_t read_byte(bit ack) {
   uint8_t res;
   uint8_t i;

   res = 0;
   SDA = 1;
   for (i=0; i<8; i++) {
      wait_half_period();
      SCK=1;
      wait_half_period_sck();
      res <<= 1;
      res |= SDA;
      wait_half_period_sck();
      SCK = 0;
      wait_half_period();
   }
   SDA = ack;

   /* send_ack */
   wait_half_period();
   SCK=1;
   wait_period_sck();
   SCK=0;
   wait_half_period();
   SDA=0;

   return res;
}

/* input:  SDA=HIGH, SCK=HIGH (after send_stop_bit)
 * output: SDA=LOW,  SCK=LOW, on 1/2 of period
 */
static void send_start_bit() {
   /* SDA high-to-low width SCK is HIGH */
   wait_period_sck_sda();
   SDA=0;
   wait_half_period_sck();
   SCK=0;
   wait_half_period();
   return;
}

/* input:  SDA=LOW, SCK=LOW, on 1/2 of period
 * output: SDA=HIGH, SCK=HIGH, on 1/2 of period
 */
static void send_stop_bit()
{
   /* SDA low-to-hidh on SCK is HIGH */
   wait_period();
   SCK=1;
   wait_half_period_sck();
   SDA=1;
   wait_half_period_sck_sda();
   SCK=0;
   wait_period();
   SCK=1;
   wait_period_sck_sda();
   return;
}

/*
 * input: SCK=HIGH, SDA=HIGH
 * output: SCK=HIGH, SDA=HIGH
 */
uint16_t read_16bit_data(uint16_t addr)
{
   uint16_t res;
   res = 0xffff;

   send_start_bit();

   /* 16 bit read. control code: 1010, chip select: 100, rw: 1 */
   /* reg_addr [7:0] */
   /* reg_addr [15:8] */
   send_byte(0xa9);
   send_byte(addr&0xff);
   send_byte(addr>>8);
   /* read [7:0] of data */
   /* read [15:8] of data */
   res = read_byte(0);
   res |= ((uint16_t)read_byte(1)<<8);
   send_stop_bit();

   return res;
}

/*
 * input: SCK=HIGH, SDA=HIGH
 * output: SCK=HIGH, SDA=HIGH
 */
void write_16bit_data(uint16_t addr, uint16_t val)
{
   send_start_bit();
   /* 16 bit read. control code: 1010, chip select: 100, rw: 0 */
   /* reg_addr [7:0] */
   /* reg_addr [15:8] */
   /* reg_data [7:0] */
   /* reg_data [8:15] */
   send_byte(0xa8);
   send_byte(addr&0xff);
   send_byte(addr>>8);
   send_byte(val & 0xff);
   send_byte(val>>8);
   send_stop_bit();

   return;
}

/*
 * input: SCK=HIGH, SDA=HIGH
 * output: SCK=HIGH, SDA=HIGH
 */
uint32_t read_32bit_data(uint16_t addr)
{
   uint32_t res;

   send_start_bit();
   /* 16 bit read. control code: 1010, chip select: 100, rw: 1 */
   send_byte(0xa9);
   /* reg_addr [7:0] */
   send_byte(addr&0xff);
   /* reg_addr [15:8] */
   send_byte(addr>>8);
   /* read [0:7] of data */
   res = read_byte(0)&0xff;
   /* read [8:15] of data */
   res |= ((uint32_t)read_byte(0)<<8);
   /* read [16:23] of data */
   res |= ((uint32_t)read_byte(0)<<16);
   /* read [24:31] of data */
   res |= ((uint32_t)read_byte(1)<<24);

   send_stop_bit();

   return res;
}


static uint16_t phy_wait() {
   uint8_t i;
   uint16_t res;

   for(i=0;i<50;i++){
      wait_period_sck_sda();
      res=read_16bit_data(0x500);
      if ((res&0x8000) == 0)
	   return(res);
   }
   return 0xffff;
}

bit phy_write(uint8_t portno, uint8_t phy_regno, uint16_t val){
   write_16bit_data(0x501, val);
   write_16bit_data(0x500, ((PHYS2PHY(portno)&0x01f)<<5)|(phy_regno&0x01f)|(1<<14));

   if ((phy_wait()&0x8000)!=0)
	return 1;
    return 0;
}

bit phy_read(uint8_t portno, uint8_t phy_regno, uint16_t *val){
    write_16bit_data(0x500,((PHYS2PHY(portno)&0x01f)<<5)|(phy_regno&0x01f));
    if ((phy_wait()&0x8000)!=0)
	return 1;
    *val=read_16bit_data(0x502);
    return 0;
}

int phy_port_power_up(uint8_t port) {
   uint16_t t1;

   if (phy_read(port, 0,&t1)==0) {
      if ((t1&0x0800)!=0) {
	 phy_write(port,0,t1&0xf7ff);
	 return 0;
      }else
	 return 1;
   }
   return -1;
}

int phy_port_power_down(uint8_t port) {
   uint16_t t1;

   if (phy_read(port, 0,&t1)==0) {
      if ((t1&0x0800)==0) {
	 phy_write(port,0,t1|0x0800);
	 return 0;
      }else
	 return 1;
   }
   return -1;
}

void port_disable(uint8_t port) {
   uint16_t val;
   val = read_16bit_data(0x0608);
   write_16bit_data(0x0608, val|((uint16_t)1<<port));
}

void port_enable(uint8_t port) {
   uint16_t val;
   val = read_16bit_data(0x0608);
   write_16bit_data(0x0608, val&~((uint16_t)1<<port));
}



