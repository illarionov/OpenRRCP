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

    struct t_swconfig {
	union {//0x0200
	    struct  t_rrcp_config{
		unsigned short
		rrcp_disable:1,
		echo_disable:1,
		loop_enable:1,
		reserved:13;
	    } config;
	    unsigned short raw;
	} rrcp_config;
	union {//0x0201
	    unsigned long bitmap;
	    unsigned short raw[2];
	} rrcp_byport_disable;
	union {
	    struct t_rxtx {
    		unsigned char
		rx:4,
		tx:4;
	    } rxtx[32];
	    unsigned short raw[16];
	} bandwidth;
	union {
	    struct t_sniff{
		unsigned long sniffer;
		unsigned long sniffed_rx;
		unsigned long sniffed_tx;
	    } sniff;
	    unsigned short raw[6];
	} port_monitor;
	union {
	    struct t_alt_s{
		struct  t_alt_config{
		    unsigned short
		    mac_aging_disable:1,
		    mac_aging_fast:1,
		    stp_filter:1,
		    mac_drop_unknown:1,
		    reserved:12;
		} alt_config;
		unsigned short alt_control;
	    }s;
	    unsigned short raw[2];    
	} alt;
	union {
	    struct t_vlan_s{
		struct  t_vlan_s_config{
		    unsigned short
		    enable:1,
		    unicast_leaky:1,
		    arp_leaky:1,
		    multicast_leaky:1,
		    dot1q:1,
		    drop_untagged_frames:1,
		    ingress_filtering:1,
		    reserved:9;
		} config;
		unsigned char port_vlan_index[26];
	    }s;
	    unsigned short raw[14];    
	} vlan;
	union {
	    unsigned long long bitmap;
	    unsigned short raw[4];
	} vlan_port_output_tag;
	union {
	    unsigned long bitmap[32];
	    unsigned short raw[64];
	} vlan_entry;
	unsigned short vlan_vid[32];
	union {
	    unsigned long bitmap;
	    unsigned short raw[2];
	} vlan_port_insert_vid;
	union {
	    struct  t_qos_s_config{
		unsigned short
		    tos_enable:1,
		    dot1p_enable:1,
		    flow_control_jam:1,
		    wrr_ratio:2,
		    reserved:11;
	    } config;
	    unsigned short raw;
	} qos_config;
	union {
	    unsigned long bitmap;
	    unsigned short raw[2];
	} qos_port_priority;
	union {
	    struct  t_port_config_global_config{
		unsigned short
		    flow_dot3x_disable:1,
		    flow_backpressure_disable:1,
		    storm_control_broadcast_strict:1,
		    storm_control_multicast_strict:1,
		    storm_control_broadcast_disable:1,
		    reserved:11;
	    } config;
	    unsigned short raw;
	} port_config_global;
	union {
	    unsigned long bitmap;
	    unsigned short raw[2];
	} port_disable;
	union {
	    struct  t_port_config_config{
		unsigned char
		    media_10half:1,
		    media_10full:1,
		    media_100half:1,
		    media_100full:1,
		    media_1000full:1,
		    pause:1,
		    pause_asy:1,
		    autoneg:1;
	    } config[26];
	    unsigned short raw[13];
	} port_config;
    } ;

extern const char *bandwidth_text[8];
extern const char *wrr_ratio_text[4];
extern struct t_swconfig swconfig;

void rrcp_config_read_from_switch(void);
void rrcp_config_bin2text(char *text_buffer, int buffer_length, int show_defaults);

char *rrcp_config_get_portname(char *buffer, int buffer_size, int port_number, int port_number_phys);

void do_show_config(int verbose);
