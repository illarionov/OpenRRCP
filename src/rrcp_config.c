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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "rrcp_io.h"
#include "rrcp_switches.h"
#include "rrcp_config.h"

const char *bandwidth_text[8]={"100M","128K","256K","512K","1M","2M","4M","8M"};
const char *wrr_ratio_text[4]={"4:1","8:1","16:1","1:0"};

struct t_swconfig swconfig;

////////////// read all relevant config from switch into our data structures    
void rrcp_config_read_from_switch(void)
{
    int i;
    
    swconfig.rrcp_config.raw=rtl83xx_readreg16(0x0200);
    for(i=0;i<2;i++)
	swconfig.rrcp_byport_disable.raw[i]=rtl83xx_readreg16(0x0201+i);
    swconfig.alt_igmp_snooping.raw=rtl83xx_readreg16(0x0308);
    for(i=0;i<2;i++)
	swconfig.alt_mrouter_mask.raw[i]=rtl83xx_readreg16(0x0309+i);
    for(i=0;i<14;i++){
	swconfig.vlan.raw[i]=rtl83xx_readreg16(0x030b+i);
    }
    for(i=0;i<4;i++){
	swconfig.vlan_port_output_tag.raw[i]=rtl83xx_readreg16(0x0319+i);
    }
    for(i=0;i<32;i++){
	swconfig.vlan_entry.raw[i*2]  =rtl83xx_readreg16(0x031d+i*3);
	swconfig.vlan_entry.raw[i*2+1]=rtl83xx_readreg16(0x031d+i*3+1);
	swconfig.vlan_vid[i]          =rtl83xx_readreg16(0x031d+i*3+2) & 0xfff;
	//mask out ports absent on this switch
	swconfig.vlan_entry.bitmap[i] &= 0xffffffff>>(32-switchtypes[switchtype].num_ports);
    }
    swconfig.vlan_port_insert_vid.raw[0]=rtl83xx_readreg16(0x037d);
    swconfig.vlan_port_insert_vid.raw[1]=rtl83xx_readreg16(0x037e);
    //mask out ports absent on this switch
    swconfig.vlan_port_insert_vid.bitmap &= 0xffffffff>>(32-switchtypes[switchtype].num_ports);
    for(i=0;i<16;i++){
	swconfig.bandwidth.raw[i]=rtl83xx_readreg16(0x020a+i);
    }
    for(i=0;i<6;i++){
	swconfig.port_monitor.raw[i]=rtl83xx_readreg16(0x0219+i);
    }
    swconfig.alt_config.raw=rtl83xx_readreg16(0x0300);
    for(i=0;i<2;i++){
	swconfig.alt_mask.raw[i]=rtl83xx_readreg16(0x0301+i);
    }
    swconfig.qos_config.raw=rtl83xx_readreg16(0x0400);
    swconfig.qos_port_priority.raw[0]=rtl83xx_readreg16(0x0401);
    swconfig.qos_port_priority.raw[1]=rtl83xx_readreg16(0x0402);
    swconfig.port_config_global.raw=rtl83xx_readreg16(0x0607);
    for(i=0;i<2;i++){
	swconfig.port_disable.raw[i]=rtl83xx_readreg16(0x0608+i);
    }
    for(i=0;i<13;i++){
	swconfig.port_config.raw[i]=rtl83xx_readreg16(0x060a+i);
    }
}

void rrcp_config_commit_vlan_to_switch(void)
{
    int i;

    for(i=0;i<14;i++){
	rtl83xx_setreg16(0x030b+i,swconfig.vlan.raw[i]);
    }
    for(i=0;i<4;i++){
	rtl83xx_setreg16(0x0319+i,swconfig.vlan_port_output_tag.raw[i]);
    }
    for(i=0;i<32;i++){
	//mask out ports absent on this switch
	swconfig.vlan_entry.bitmap[i] &= 0xffffffff>>(32-switchtypes[switchtype].num_ports);
	rtl83xx_setreg16(0x031d+i*3,  swconfig.vlan_entry.raw[i*2]);
	rtl83xx_setreg16(0x031d+i*3+1,swconfig.vlan_entry.raw[i*2+1]);
	rtl83xx_setreg16(0x031d+i*3+2,swconfig.vlan_vid[i]);
    }
    //mask out ports absent on this switch
    swconfig.vlan_port_insert_vid.bitmap &= 0xffffffff>>(32-switchtypes[switchtype].num_ports);
    rtl83xx_setreg16(0x037d,swconfig.vlan_port_insert_vid.raw[0]);
    rtl83xx_setreg16(0x037e,swconfig.vlan_port_insert_vid.raw[1]);
}

void sncprintf(char *str, size_t size, const char *format, ...) {
    char line[1024];
    va_list ap;

    line[0]=0;
    va_start(ap, format);
    vsnprintf (line, size-1, format, ap);
    va_end(ap);
    if (size>(strlen(str)+strlen(line)+1)){
	strncat(str,line,sizeof(line)-1);
    }
}

char *rrcp_config_get_portname(char *buffer, int buffer_size, int port_number, int port_number_phys)
{
    if (port_number_phys<=23){
	snprintf(buffer,buffer_size,"FastEthernet0/%d",port_number);
    }else{
	snprintf(buffer,buffer_size,"GigabitEthernet0/%d",port_number);
    }
    return buffer;
}

void rrcp_config_bin2text(char *sc, int l, int show_defaults)
{
    int i,port,port_phys,port2,port2_phys;
    char pn[64];

    sprintf(sc,"!\n");
    {
	sncprintf(sc,l,"version %s\n",switchtypes[switchtype].chip_name);
	sncprintf(sc,l,"!\n");
    }
    {
	int mac_aging_time=-1;
	if (swconfig.alt_config.s.config.mac_aging_disable){
	    mac_aging_time=0;
	}else if (swconfig.alt_config.s.config.mac_aging_fast){
	    mac_aging_time=12;
	}else{
	    mac_aging_time=300;
	}
        sncprintf(sc,l,"mac-address-table aging-time %d\n",mac_aging_time);
	sncprintf(sc,l,"!\n");
    }
    {
	sncprintf(sc,l,"%sip igmp snooping\n", swconfig.alt_igmp_snooping.config.en_igmp_snooping ? "":"no ");
    }
    {
	sncprintf(sc,l,"%srrcp enable\n", swconfig.rrcp_config.config.rrcp_disable ? "no ":"");
	sncprintf(sc,l,"%srrcp echo enable\n", swconfig.rrcp_config.config.echo_disable ? "no ":"");
	sncprintf(sc,l,"%srrcp loop-detect enable\n", swconfig.rrcp_config.config.loop_enable ? "":"no ");
	sncprintf(sc,l,"!\n");
    }
    {
	if (swconfig.vlan.s.config.enable){
	    if (swconfig.vlan.s.config.dot1q){
		if (switchtypes[switchtype].chip_id!=rtl8316b){
		    sncprintf(sc,l,"! WARNING: 802.1Q VLANs enabled on hardware, that do not support them properly.\n");
		    sncprintf(sc,l,"! WARNING: Read here: http://openrrcp.org.ru/wiki:rtl8326_dot1q_bug for details.\n");
		    sncprintf(sc,l,"vlan dot1q force\n");
		}else{
		    sncprintf(sc,l,"vlan dot1q\n");
		}
	    }else{
		sncprintf(sc,l,"vlan portbased\n");
	    }
	    {
		int i;
		char vlanlist[128],s[16];

		vlanlist[0]=0;
		for(i=0;i<32;i++){
		    sprintf(s,"%d",swconfig.vlan_vid[i]);
			if (vlanlist[0]!=0){
			    strcat(vlanlist,",");
			}
		    strcat(vlanlist,s);
		}
		sncprintf(sc,l,"! vlan numbers: %s\n",vlanlist);
	    }
	}else{
	    sncprintf(sc,l,"no vlan\n");
	}
	if (show_defaults || swconfig.vlan.s.config.arp_leaky){
	    sncprintf(sc,l,"%svlan leaky arp\n", swconfig.vlan.s.config.arp_leaky ? "":"no ");
	}
	if (show_defaults || swconfig.vlan.s.config.multicast_leaky){
	    sncprintf(sc,l,"%svlan leaky multicast\n", swconfig.vlan.s.config.multicast_leaky ? "":"no ");
	}
	if (show_defaults || swconfig.vlan.s.config.unicast_leaky){
	    sncprintf(sc,l,"%svlan leaky unicast\n", swconfig.vlan.s.config.unicast_leaky ? "":"no ");
	}
	if (show_defaults || swconfig.vlan.s.config.drop_untagged_frames){
	    sncprintf(sc,l,"%svlan drop untagged_frames\n", swconfig.vlan.s.config.drop_untagged_frames ? "":"no ");
	}
	if (show_defaults || !swconfig.vlan.s.config.ingress_filtering){
	    sncprintf(sc,l,"%svlan drop invalid_vid\n", swconfig.vlan.s.config.ingress_filtering ? "":"no ");
	}
	sncprintf(sc,l,"!\n");
    }
    {
	sncprintf(sc,l,"%smls qos trust dscp\n", swconfig.qos_config.config.dscp_enable ? "":"no ");
	sncprintf(sc,l,"%smls qos trust cos\n", swconfig.qos_config.config.cos_enable ? "":"no ");
	sncprintf(sc,l,"wrr-queue ratio %s\n", wrr_ratio_text[swconfig.qos_config.config.wrr_ratio]);
	sncprintf(sc,l,"!\n");
    }
    {
	sncprintf(sc,l,"%sflowcontrol dot3x\n", swconfig.port_config_global.config.flow_dot3x_disable ? "no ":"");
	sncprintf(sc,l,"%sflowcontrol backpressure\n", swconfig.port_config_global.config.flow_backpressure_disable ? "no ":"");
	if (show_defaults || swconfig.qos_config.config.flow_ondemand_disable){
	    sncprintf(sc,l,"%sflowcontrol ondemand-disable\n", swconfig.qos_config.config.flow_ondemand_disable ? "":"no ");
	}
	if (swconfig.port_config_global.config.storm_control_broadcast_disable){
	    sncprintf(sc,l,"no storm-control broadcast\n");
	}else{
	    sncprintf(sc,l,"storm-control broadcast %s\n", swconfig.port_config_global.config.storm_control_broadcast_strict ? "strict":"relaxed");
	}
	sncprintf(sc,l,"%sstorm-control multicast\n", swconfig.port_config_global.config.storm_control_multicast_strict ? "":"no ");
	sncprintf(sc,l,"!\n");
    }
    if (show_defaults || swconfig.alt_config.s.config.stp_filter){
	sncprintf(sc,l,"%sspanning-tree bpdufilter enable\n", swconfig.alt_config.s.config.stp_filter ? "":"no ");
	sncprintf(sc,l,"!\n");
    }

    for(port=1;port<=switchtypes[switchtype].num_ports;port++){
	int is_trunk=0;
	port_phys=map_port_number_from_logical_to_physical(port);
	is_trunk=(swconfig.vlan_port_insert_vid.bitmap&(1<<port_phys))>>port_phys;
	sncprintf(sc,l,"interface %s\n",rrcp_config_get_portname(pn,sizeof(pn),port,port_phys));
	if (show_defaults || (swconfig.port_disable.bitmap&(1<<port_phys))){
	    sncprintf(sc,l," %sshutdown\n", (swconfig.port_disable.bitmap&(1<<port_phys)) ? "":"no ");
	}
	if (swconfig.vlan.s.config.enable){
	    //print vlan-related lines only if vlans are enabled globally by this config
	    if (is_trunk){
		char vlanlist[256],s[16];
		vlanlist[0]=0;
		for(i=0;i<32;i++){
		    if(swconfig.vlan_entry.bitmap[i]&(1<<port_phys)){
			sprintf(s,"%d",swconfig.vlan_vid[i]);
			if (vlanlist[0]!=0){
			    strcat(vlanlist,",");
			}
			strcat(vlanlist,s);
		    }
		}
		if (vlanlist[0]==0){
		    strcpy(vlanlist,"none");
		}
		sncprintf(sc,l," switchport trunk allowed vlan %s\n",vlanlist);
	    }else{
		sncprintf(sc,l," switchport access vlan %d\n",swconfig.vlan_vid[swconfig.vlan.s.port_vlan_index[port_phys]]);
	    }
	    sncprintf(sc,l," switchport mode %s\n",is_trunk ? "trunk":"access");
	}
	if (show_defaults || swconfig.bandwidth.rxtx[port_phys].rx!=0){
	    sncprintf(sc,l," rate-limit input %s\n",bandwidth_text[swconfig.bandwidth.rxtx[port_phys].rx]);
	}
	if (show_defaults || swconfig.bandwidth.rxtx[port_phys].tx!=0){
	    sncprintf(sc,l," rate-limit output %s\n",bandwidth_text[swconfig.bandwidth.rxtx[port_phys].tx]);
	}
	if (switchtypes[switchtype].chip_id==rtl8316b){
	    //port mirroring working only with rtl8316b
	    if (swconfig.port_monitor.sniff.sniffer & (1<<port_phys)){
		for(port2=1;port2<=switchtypes[switchtype].num_ports;port2++){
		    port2_phys=map_port_number_from_logical_to_physical(port2);
		    if ((swconfig.port_monitor.sniff.sniffed_rx & (1<<port2_phys))&&
			(swconfig.port_monitor.sniff.sniffed_tx & (1<<port2_phys))){
			sncprintf(sc,l," port monitor %s\n", rrcp_config_get_portname(pn,sizeof(pn),port2,port2_phys));
		    }else if (swconfig.port_monitor.sniff.sniffed_rx & (1<<port2_phys)){
			sncprintf(sc,l," port monitor %s rx\n", rrcp_config_get_portname(pn,sizeof(pn),port2,port2_phys));
		    }else if (swconfig.port_monitor.sniff.sniffed_tx & (1<<port2_phys)){
			sncprintf(sc,l," port monitor %s tx\n", rrcp_config_get_portname(pn,sizeof(pn),port2,port2_phys));
		    }
		}
	    }
	}

	if (show_defaults || (swconfig.alt_mask.mask&(1<<port_phys))){
	    sncprintf(sc,l," %smac-learn disable\n",(swconfig.alt_mask.mask&(1<<port_phys)) ? "":"no ");
	}
	if (show_defaults || !(swconfig.rrcp_byport_disable.bitmap&(1<<port_phys))){
	    sncprintf(sc,l," %srrcp enable\n",(swconfig.rrcp_byport_disable.bitmap&(1<<port_phys)) ? "no ":"");
	}
	if (show_defaults || (swconfig.qos_port_priority.bitmap & (1<<port_phys))){
	    sncprintf(sc,l," mls qos cos %d\n", (swconfig.qos_port_priority.bitmap & (1<<port_phys)) ? 7:0);
	}
	if (swconfig.port_config.config[port_phys].autoneg){
	    if (show_defaults){
		sncprintf(sc,l," speed auto\n");
	    }
	}else if (swconfig.port_config.config[port_phys].media_100full){
	    sncprintf(sc,l," speed 100\n duplex full\n");
	}else if (swconfig.port_config.config[port_phys].media_100half){
	    sncprintf(sc,l," speed 100\n duplex half\n");
	}else if (swconfig.port_config.config[port_phys].media_10full){
	    sncprintf(sc,l," speed 10\n duplex full\n");
	}else if (swconfig.port_config.config[port_phys].media_10half){
	    sncprintf(sc,l," speed 10\n duplex half\n");
	}
	sncprintf(sc,l,"!\n");
    }
    sncprintf(sc,l,"end\n");
}

void do_show_config(int verbose)
{
    char *text;

    text=malloc(65536);
    rrcp_config_read_from_switch();
    rrcp_config_bin2text(text,65535,verbose);
    printf("%s",text);
    free(text);
}

int find_vlan_index_by_vid(int vid)
{
    int i;
    for(i=0;i<32;i++){
	if (swconfig.vlan_vid[i]==vid){
	    return i;
	}
    }
    return -1;
}

int find_or_create_vlan_index_by_vid(int vid)
{
    int i;
    for(i=0;i<32;i++){
	if (swconfig.vlan_vid[i]==vid){
	    return i;
	}
	if (swconfig.vlan_vid[i]==0){
	    swconfig.vlan_vid[i]=vid;
	    return i;
	}
    }
    return -1;
}
