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

#include <stdint.h>

    typedef enum {
	EEPROM_NONE = 0,
	EEPROM_WRITEPROTECTED = 1,
	EEPROM_2401 = 2,
	EEPROM_2402 = 3,
	EEPROM_2404 = 4,
	EEPROM_2408 = 5,
	EEPROM_2416 = 6
    } t_eeprom_type;

    struct t_swconfig {
	uint8_t mac_address[6];
	uint8_t switch_type;
	uint8_t chip_type;
	t_eeprom_type eeprom_type;
	uint8_t	port_count;
	uint8_t facing_switch_port_phys;
	union {//0x0200
	    struct  t_rrcp_config{
		uint16_t
		rrcp_disable:1,
		echo_disable:1,
		loop_enable:1,
		reserved:13;
	    } config;
	    uint16_t raw;
	} rrcp_config;
	union {//0x0201
	    uint32_t bitmap;
	    uint16_t raw[2];
	} rrcp_byport_disable;
	union {
	    struct t_rxtx {
    		uint8_t
		rx:4,
		tx:4;
	    } rxtx[32];
	    uint16_t raw[16];
	} bandwidth;
	union {
	    struct t_sniff{
		uint32_t sniffer;
		uint32_t sniffed_rx;
		uint32_t sniffed_tx;
	    } sniff;
	    uint16_t raw[6];
	} port_monitor; // 0x0219..0x021e, supported only on rtl8316b/rtl8324
	union {
	    struct t_alt_config_s{
		struct  t_alt_config{
		    uint16_t
		    mac_aging_disable:1,
		    mac_aging_fast:1,
		    stp_filter:1,
		    mac_drop_unknown:1,
		    reserved:12;
		} config;
	    }s;
	    uint16_t raw;    
	} alt_config; // 0x0300
	union {
	    uint32_t mask;
	    uint16_t raw[2];    
	} alt_mask; // 0x0301..0x0302
	union {
	    struct  t_alt_igmp_snooping{
		uint16_t
	    	    en_igmp_snooping:1,
	    	    reserved:15;
	    } config;
	    uint16_t raw;
	} alt_igmp_snooping; // 0x0308
	union {
	    uint32_t mask;
	    uint16_t raw[2];    
	} alt_mrouter_mask; // 0x0309..0x030a
	union {
	    struct t_vlan_s{
		struct  t_vlan_s_config{
		    uint16_t
		    enable:1,
		    unicast_leaky:1,
		    arp_leaky:1,
		    multicast_leaky:1,
		    dot1q:1,
		    drop_untagged_frames:1,
		    ingress_filtering:1,
		    reserved:9;
		} config;
		uint8_t port_vlan_index[26];
	    }s;
	    uint16_t raw[14];    
	} vlan;
	union {
	    uint32_t bitmap;
	    uint16_t raw[4];
	} vlan_port_output_tag; //0x0319
	union {
	    uint32_t bitmap[32];
	    uint16_t raw[64];
	} vlan_entry;
	uint16_t vlan_vid[32];
	union {
	    uint32_t bitmap;
	    uint16_t raw[2];
	} vlan_port_insert_vid;
	union {
	    struct  t_qos_s_config{
		uint16_t
		    dscp_enable:1,
		    cos_enable:1,
		    flow_ondemand_disable:1,
		    wrr_ratio:2,
		    reserved:11;
	    } config;
	    uint16_t raw;
	} qos_config;
	union {
	    uint32_t bitmap;
	    uint16_t raw[2];
	} qos_port_priority;
	union {
	    struct  t_port_config_global_config{
		uint16_t
		    flow_dot3x_disable:1,
		    flow_backpressure_disable:1,
		    storm_control_broadcast_strict:1,
		    storm_control_multicast_strict:1,
		    storm_control_broadcast_disable:1,
		    reserved:11;
	    } config;
	    uint16_t raw;
	} port_config_global;
	union {
	    uint32_t bitmap;
	    uint16_t raw[2];
	} port_disable;
	union {
	    struct  t_port_config_config{
		uint8_t
		    media_10half:1,
		    media_10full:1,
		    media_100half:1,
		    media_100full:1,
		    media_1000full:1,
		    pause:1,
		    pause_asy:1,
		    autoneg:1;
	    } config[26];
	    uint16_t raw[13];
	} port_config;
    } ;

extern const char *bandwidth_text[8];
extern const char *wrr_ratio_text[4];
extern const char *eeprom_type_text[7];
extern const int eeprom_type_size[7];
extern struct t_swconfig swconfig;

void rrcp_config_read_from_switch(void);

void rrcp_config_commit_vlan_to_switch(void);

void rrcp_config_bin2text(char *text_buffer, int buffer_length, int show_defaults);

char *rrcp_config_get_portname(char *buffer, int buffer_size, int port_number, int port_number_phys);

void do_show_config(int verbose);

int find_vlan_index_by_vid(int vid);

int find_or_create_vlan_index_by_vid(int vid);
