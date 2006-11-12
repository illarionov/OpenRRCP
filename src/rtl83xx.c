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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#ifndef __linux__
#include <sys/socket.h>
#include <net/if.h>
#endif

#include <signal.h>
#include <unistd.h>
#include "rrcp_packet.h"
#include "rrcp_io.h"
#include "rrcp_switches.h"
#include "rrcp_config.h"

int myPid = 0;

void sigHandler(int sig)
{
    printf("Timeout! (no switch with rtl83xx chip found?)\n");
    kill( myPid, SIGKILL );
    exit( 0 );
}

void engage_timeout(int seconds)
{
    myPid = getpid();
    signal( SIGALRM, sigHandler );
    alarm( seconds );
}

void dumpmem(void *ptr, int len){
    int i;
    unsigned char *p;
    p=(unsigned char*)ptr;
    printf("DUMPINMG... len=%d\n",len);
    for (i=0;i<=len/8;i++){
	printf("%04x - %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",i*8,
	    p[i*8+0],p[i*8+1],p[i*8+2],p[i*8+3],
	    p[i*8+4],p[i*8+5],p[i*8+6],p[i*8+7]);
    }
}

void print_port_link_status(int port_no, int enabled, unsigned char encoded_status){
    struct t_rtl83xx_port_link_status {
	unsigned char	speed:2,
  		duplex:1,
  		reserved:1,
  		link:1,
  		flow_control:1,
  		asy_pause:1,
  		auto_negotiation:1;
    };
    struct t_rtl83xx_port_link_status stat;
    memcpy(&stat,&encoded_status,1);
    printf("%s/%-2d: ",ifname,port_no);
    printf("%s ",enabled ? " ENABLED" : "disabled");
    printf("%s",stat.auto_negotiation ? "auto" : " set:");
    if (stat.link){
	switch (stat.speed){
	    case 0:
		printf("  10M");
		break;
	    case 1:
		printf(" 100M");
		break;
	    case 2:
		printf("1000M");
		break;
	    case 3:
		printf(" ???M");
		break;		
	}
	printf("-%s",stat.duplex ? "FD" : "HD");
    }else{
	printf("     -  ");
    }
    printf(" %s",stat.link ? "LINK" : " -- ");
    printf(" %s",stat.flow_control ? "flowctl" : "noflowctl");
    printf("\n");
}

void print_link_status(void){
    int i;
    union {
	unsigned short sh[13];
	unsigned char  ch[26];
    } r;
    union {
        struct {
            unsigned short low;
            unsigned short high;
	} doubleshort;
        unsigned long signlelong;
    } u;
    u.doubleshort.low=rtl83xx_readreg16(0x0608);
    u.doubleshort.high=rtl83xx_readreg16(0x0609);
    for(i=0;i<switchtypes[switchtype].num_ports/2;i++){
	r.sh[i]=rtl83xx_readreg16(0x0619+i);
    }
    for(i=1;i<=switchtypes[switchtype].num_ports;i++){
	print_port_link_status(
		i,
		!((u.signlelong>>(map_port_number_from_logical_to_physical(i)))&1),
		r.ch[map_port_number_from_logical_to_physical(i)]);
    }
}

void do_reboot(void){
    rtl83xx_setreg16(0x0000,0x0002);
}

void do_softreboot(void){
    rtl83xx_setreg16(0x0000,0x0001);
}

void print_counters(void){
    int i,port_tr;
    for (i=0;i<=0x0c;i++){
	rtl83xx_setreg16(0x0700+i,0x0000);//read rx byte, tx byte, drop byte
    }
    printf("port              RX          TX        drop\n");
    for (i=1;i<=switchtypes[switchtype].num_ports;i++){
	port_tr=map_port_number_from_logical_to_physical(i);
	printf("%s/%-2d: %11lu %11lu %11lu\n",
		ifname,i,
		rtl83xx_readreg32(0x070d+port_tr),
		rtl83xx_readreg32(0x0727+port_tr),
		rtl83xx_readreg32(0x0741+port_tr));
    }
    return;
}

void do_ping(void){
    int r=rtl83xx_ping();
	printf("%02x:%02x:%02x:%02x:%02x:%02x %sresponded\n",
			dest_mac[0],
			dest_mac[1],
			dest_mac[2],
			dest_mac[3],
			dest_mac[4],
			dest_mac[5],
			r ? "":"not ");
    _exit(!r);
}

void print_vlan_status(void){
    int i,port,port_phys;

    unsigned short vlan_status;
    union {
	unsigned char index[26];
	unsigned short raw[13];
    } vlan_port_vlan;
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

    char *vlan_port_output_tag_descr[4]={
	"strip_all(00)",
	"insert_prio(01)",
	"insert_all(10)",
	"dont_touch(11)"
    };

    //read all relevant config from switch into our data structures    
    vlan_status=rtl83xx_readreg16(0x030b);
    
    for(i=0;i<13;i++){
	vlan_port_vlan.raw[i]=rtl83xx_readreg16(0x030c+i);
    }
    for(i=0;i<4;i++){
	vlan_port_output_tag.raw[i]=rtl83xx_readreg16(0x0319+i);
    }
    for(i=0;i<32;i++){
	vlan_entry.raw[i*2]  =rtl83xx_readreg16(0x031d+i*3);
	vlan_entry.raw[i*2+1]=rtl83xx_readreg16(0x031d+i*3+1);
	vlan_vid[i]          =rtl83xx_readreg16(0x031d+i*3+2) & 0xfff;
	//mask out ports absent on this switch
	vlan_entry.bitmap[i] &= 0xffffffff>>(32-switchtypes[switchtype].num_ports);
    }
    vlan_port_insert_vid.raw[0]=rtl83xx_readreg16(0x037d);
    vlan_port_insert_vid.raw[1]=rtl83xx_readreg16(0x037e);
    //mask out ports absent on this switch
    vlan_port_insert_vid.bitmap &= 0xffffffff>>(32-switchtypes[switchtype].num_ports);

    //display
    printf("VLAN Status reg=0x%04x VLAN enabled: %s, 802.1Q enabled: %s\n",
	    vlan_status & 0x7f,
	    (vlan_status & 1<<0) ? "yes" : "no",
	    (vlan_status & 1<<4) ? "yes" : "no");
    for (port=1;port<=switchtypes[switchtype].num_ports;port++){
	port_phys=map_port_number_from_logical_to_physical(port);
	printf("%s/%-2d: VLAN_IDX=%02d, VID=%d, Insert_VID=%s, Out_tag_strip=%s\n",
		ifname,
		port,
		vlan_port_vlan.index[port_phys],
		vlan_vid[vlan_port_vlan.index[port_phys]],
		vlan_port_insert_vid.bitmap&(1<<port_phys) ? "yes" : "no",
		vlan_port_output_tag_descr[(vlan_port_output_tag.bitmap>>(port_phys*2))&3]
		);
    }
    printf("IDX VID Member ports\n");
    for (i=0;i<32;i++){
	if (vlan_entry.bitmap[i]){
	    char s[16],portlist[128];
	    int port;
	    portlist[0]=0;
	    for(port=1;port<=switchtypes[switchtype].num_ports;port++){
		port_phys=map_port_number_from_logical_to_physical(port);
		if(vlan_entry.bitmap[i]&(1<<port_phys)){
		    sprintf(s,"%d",port);
		    if (portlist[0]!=0){
			strcat(portlist,",");
		    }
		    strcat(portlist,s);
		}
	    }
	    printf("%2d %4d %s\n",
		    i,
		    vlan_vid[i],		    
		    portlist);
	}
    }
}

void do_vlan_enable_vlan(int is_8021q, int root_port, int vid_base){
    int i,port,vlan,port_phys,root_port_phys;
    unsigned short vlan_status;
    union {
	unsigned char index[26];
	unsigned short raw[13];
    } vlan_port_vlan;
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

    //fill-up local data structures
    if (is_8021q){
	vlan_status=(rtl83xx_readreg32(0x030b)|1)|(1<<4);
    }else{
	vlan_status=(rtl83xx_readreg32(0x030b)|1)&~(1<<4);
    }

    vlan_port_output_tag.bitmap=0;
    vlan_port_insert_vid.bitmap=0;
    for(port=1;port<=switchtypes[switchtype].num_ports;port++){
    	port_phys=map_port_number_from_logical_to_physical(port);
	if (port==root_port){
	    vlan_port_vlan.index[port_phys]=0;
	    vlan_port_output_tag.bitmap|=3<<port_phys*2;
	    vlan_port_insert_vid.bitmap|=1<<port_phys;
	}else{
	    vlan_port_vlan.index[port_phys]=(unsigned char)port;
	}
    }

    root_port_phys=map_port_number_from_logical_to_physical(root_port);
    memset(&vlan_entry,0,sizeof(vlan_entry));
    memset(&vlan_vid,0,sizeof(vlan_vid));
    if (is_8021q){
	vlan_entry.bitmap[0]=0;
    }else{
	vlan_entry.bitmap[0]=0xffffffff>>(32-switchtypes[switchtype].num_ports);
    }
    for(vlan=1;vlan<=switchtypes[switchtype].num_ports;vlan++){
	if (vlan!=root_port){
	    port_phys=map_port_number_from_logical_to_physical(vlan);
	    vlan_entry.bitmap[vlan]=(1<<port_phys)|(1<<root_port_phys);
	    vlan_vid[vlan]=vid_base+vlan;
	}
    }

    //write all relevant config to switch from our data structures    
    rtl83xx_setreg16reg16(0x030b,vlan_status);
    for(i=0;i<13;i++){
	rtl83xx_setreg16reg16(0x030c+i,vlan_port_vlan.raw[i]);
    }
    for(i=0;i<4;i++){
	rtl83xx_setreg16(0x0319+i,vlan_port_output_tag.raw[i]);
    }
    for(i=0;i<32;i++){
	rtl83xx_setreg16(0x031d+i*3+0,vlan_entry.raw[i*2]);
	rtl83xx_setreg16(0x031d+i*3+1,vlan_entry.raw[i*2+1]);
	rtl83xx_setreg16(0x031d+i*3+2,vlan_vid[i]);
    }
    rtl83xx_setreg16(0x037d,vlan_port_insert_vid.raw[0]);
    rtl83xx_setreg16(0x037e,vlan_port_insert_vid.raw[1]);
}

int main(int argc, char **argv){
    unsigned int x[6];
    int i;
    char *p;

    if (argc<3){
	printf("Usage: rtl8316b <if-name|xx:xx:xx:xx:xx:xx@if-name> <command> [<argument>]\n");
	printf("       rtl8326 ----\"\"----\n");
	printf("       rtl83xx_dlink_des1016d ----\"\"----\n");
	printf("       rtl83xx_dlink_des1024d ----\"\"----\n");
	printf("       rtl83xx_compex_ps2216 ----\"\"----\n");
	printf(" where command may be:\n");
	printf(" scan [verbose]           - scan network for rrcp-enabled switches\n");
	printf(" reboot                   - initiate switch reboot\n");
	printf(" show config              - show current switch config\n");
	printf(" vlan status              - show low-level vlan confg\n");
	printf(" vlan enable_hvlan <port> - configure switch as home-vlan tree with specified uplink port\n");
	printf(" vlan enable_8021q <port> - configure switch as IEEE 802.1Q vlan tree with specified uplink port\n");
	printf(" link-status          - print link status for all ports\n");
	printf(" counters             - print port rx/tx counters for all ports\n");
	printf(" ping                 - test if switch is responding\n");
	exit(0);
    }
    p=argv[0];
    if ((p=rindex(p,'/'))==NULL){
	p=argv[0];
    }
    if (strstr(p,"rtl8316b")==argv[0]+strlen(argv[0])-8){
	switchtype=0;
    }else if (strstr(p,"rtl8326")==argv[0]+strlen(argv[0])-7){
	switchtype=1;
    }else if (strstr(p,"rtl83xx_dlink_des1016d")==argv[0]+strlen(argv[0])-22){
	switchtype=2;
    }else if (strstr(p,"rtl83xx_dlink_des1024d")==argv[0]+strlen(argv[0])-22){
	switchtype=3;
    }else if (strstr(p,"rtl83xx_compex_ps2216")==argv[0]+strlen(argv[0])-21){
	switchtype=4;
    }else {
	printf("%s: unknown chip type\n",argv[0]);
	exit(0);
    }

    if (sscanf(argv[1], "%x:%x:%x:%x:%x:%x@%s",x,x+1,x+2,x+3,x+4,x+5,ifname)==7){
	for (i=0;i<6;i++){
	    dest_mac[i]=(unsigned char)x[i];
	}
    }else{
	strcpy(ifname,argv[1]);
    }
    if (if_nametoindex(ifname)<=0){
	printf("%s: no such interface: %s\n",argv[0],ifname);
	exit(0);
    }

    printf("! rtl83xx: trying to reach %d-port \"%s %s\" switch at %s\n",
	    switchtypes[switchtype].num_ports,
	    switchtypes[switchtype].vendor,
	    switchtypes[switchtype].model,
	    argv[1]);

    engage_timeout(5);
    rtl83xx_prepare();


    if(strcmp(argv[2],"scan")==0){
	if (argc==4 && strcmp(argv[3],"verbose")==0){
	    rtl83xx_scan(1);
	}else{
	    rtl83xx_scan(0);
	}
    }else if(strcmp(argv[2],"getreg")==0){
	int ra=strtol(argv[3], (char **)NULL, 16);
	printf("reg(0x%04x)=0x%04x\n",ra,(int)rtl83xx_readreg32(ra));
    }else if(strcmp(argv[2],"setreg")==0){
	int ra=strtol(argv[3], (char **)NULL, 16);
	int rd=strtol(argv[4], (char **)NULL, 16);
	rtl83xx_setreg16(ra,rd);
	printf("reg(0x%04x) has been set to 0x%04x\n",ra,(int)rtl83xx_readreg32(ra));
    }else if(strcmp(argv[2],"reboot")==0){
	do_reboot();
    }else if(strcmp(argv[2],"soft-reboot")==0){
	do_softreboot();
    }else if(strcmp(argv[2],"ping")==0){
	do_ping();
    }else if(strcmp(argv[2],"link-status")==0){
	print_link_status();
    }else if(strcmp(argv[2],"counters")==0){
	print_counters();
    }else if(strcmp(argv[2],"show")==0){
	if (argc<4){
	    printf("No sub-command specified! available subcommands are:\n");
	    printf("show config\n");
	    return(1);
	}
	if(strcmp(argv[3],"config")==0){
	    do_show_config();
	}else{
	    printf("Unknown sub-command: %s",argv[3]);
	}
    }else if(strcmp(argv[2],"vlan")==0){
	if (argc<4){
	    printf("No sub-command specified! available subcommands are:\n");
	    printf("vlan enable_hvlan <root-port>\n");
//	    printf("vlan enable_8021q <root-port>[,VID_base] *(VID_base is 100 by default)\n");
	    printf("vlan enable_8021q <root-port>\n");
	    printf("vlan status\n");
	    return(1);
	}
	if(strcmp(argv[3],"status")==0){
	    print_vlan_status();
	}else if((strcmp(argv[3],"enable_hvlan")==0)||(strcmp(argv[3],"enable_8021q")==0)){
	    if (argc<5){
		printf("no root-port specified!\n");
		return(1);
	    }
	    do_vlan_enable_vlan(strcmp(argv[3],"enable_8021q")==0,atoi(argv[4]),100);
	}else{
	    printf("Unknown sub-command: %s",argv[3]);
	}
    }else{
	printf("unknown command: %s\n",argv[2]);
    }

    return 0;
}
