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

#include <stdint.h>

#define SCK (P1_0)
#define SDA  (P1_1)

sfr at (0x9f) WDTC; /* Watch Dog Timer of SyncMOS SM8952A */
sfr at (0xbf) SCONF; /* System Control Register of SyncMOS SM8952A */

#define INIT_WATCHDOG	WDTC=0x87;
#define RESET_WATCHDOG  WDTC=0x20;

/* jumpers */
#define JUMPER_3  (!P0_4)
#define JUMPER_4  (!P0_5)
#define JUMPER_5  (!P0_6)
#define JUMPER_6  (!P0_7)
#define JUMPER_7  (!P2_0)
#define JUMPER_8  (!P2_1)
#define JUMPER_9  (!P2_2)
#define JUMPER_10 (!P2_3)
#define JUMPER_11 (!P0_3)
#define JUMPER_12 (!P0_2)


#define RRCP_ENABLE_JUMPER	    JUMPER_6
#define LOOPBACK_DETECTION_JUMPER   JUMPER_7
#define PORT_TESTER_JUMPER	    JUMPER_8

#define VLAN_TOPOLOGY_JUMPER        JUMPER_3
#define PORT_BASED_VLAN_JUMPER      JUMPER_4

#define PORT_TRUNK0_JUMPER          JUMPER_5
/*
#define PORT_TRUNK1_JUMPER          JUMPER_6
#define PORT_TRUNK2_JUMPER          JUMPER_7
#define PORT_TRUNK3_JUMPER          JUMPER_8

#define DIFFSERV_PRIORITY_BASED_QOS_JUMPER  JUMPER_9
#define VLAN_TAG_PRIORITY_BASED_QOS_JUMPER  JUMPER_10
#define PORT_BASED_PRIORITY_QOS_JUMPER1     JUMPER_11
#define PORT_BASED_PRIORITY_QOS_JUMPER2     JUMPER_12
*/

/* EEPROM */
#if !defined(EEPROM_CONFIG_ADDR)
#define EEPROM_CONFIG_ADDR 0x1c00
#endif

#if !defined(EEPROM_CONFIG_SIZE)
#define EEPROM_CONFIG_SIZE 0x400
#endif

__code __at(EEPROM_CONFIG_ADDR) uint16_t eeprom_config[EEPROM_CONFIG_SIZE/2];

#define EEPROM_PORT_PHY_POWERDOWN_ADDR   0x180/2
#define EEPROM_PORT_DISABLE_LOOP_DETECT_ADDR 0x184/2
#define EEPROM_OPENRRCP_CONTROL_ADDR	0x188/2

#define OPENRRCP_CONTROL_LOOP_DETECT	0x01
#define OPENRRCP_CONTROL_PORT_TESTER	0x02
#define OPENRRCP_CONTROL_CPU_WATCHDOG	0x04
#define OPENRRCP_CONTROL_PORT_WATCHDOG	0x08


/* Crystal Speed */
#define OSCILLATOR 25000000
/*The number of crystal cycles per timer increment  */
#define TMRCYCLE   12
/* The # of timer increments per second  */
#define TMR_SEC    (OSCILLATOR/TMRCYCLE)

/* 200KHZ timer */
#define TIMER0_FREQ  200000
#define TIMER0_RELOAD_VALUE (65536-(TMR_SEC/TIMER0_FREQ))

/* 1KHz timer (period=1ms) */
#define TIMER1_FREQ  1000
#define TIMER1_RELOAD_VALUE (65536-(TMR_SEC/TIMER1_FREQ))

#define POLLING_INTERVAL_MS   500
#define LOOPBACK_RECOVER_TIME_S 60


/* io.c */
void ClockIrqHandler (void) interrupt 1;
void ClockIrqHandler1 (void) interrupt 3 using 2;

void init_timer();
void wait_half_period();
void wait_half_period_sck();
void wait_half_period_sck_sda();
void wait_period();
void wait_period_sck();
void wait_period_sck_sda();

void reset_ms_timer(unsigned long ms);
void wait_ms_timer(bit check_sck, bit check_sda);
void stop_ms_timer();
void wait_ms(unsigned long ms);

uint16_t read_16bit_data(uint16_t addr);
uint32_t read_32bit_data(uint16_t addr);
void write_16bit_data(uint16_t addr, uint16_t val);

bit phy_write(uint8_t portno, uint8_t phy_regno, uint16_t data);
bit phy_read(uint8_t portno, uint8_t phy_regno, uint16_t *data);

int phy_port_power_up(uint8_t portno);
int phy_port_power_down(uint8_t portno);
void port_enable(uint8_t port);
void port_disable(uint8_t port);

/* Jumpers */
void init_standart_jumpers();
/* Loopback detection */
void init_loopback_detection();
void loopback_detection();

/* Port tester */
void port_tester();

