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
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <ctype.h>

#ifndef __linux__
#include <sys/socket.h>
#include <net/if.h>
#endif

#include <signal.h>
#include <unistd.h>
#include "rrcp_packet.h"
#include "rrcp_io.h"
#include "rrcp_config.h"
#include "rrcp_switches.h"

int myPid = 0;
extern char ErrIOMsg[];

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

void print_port_link_status(int port_no, int enabled, unsigned char encoded_status, int loopdetect){
    struct t_rtl83xx_port_link_status stat;

    memcpy(&stat,&encoded_status,1);
    printf("%s/%-2d: ",ifname,port_no);
    printf("%s ",enabled ? " ENABLED" : "disabled");
    printf("%s",stat.auto_negotiation ? "auto" : "set:");
    if (stat.link || !stat.auto_negotiation){
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
    printf(" %s",stat.flow_control ? "flowctl  " : "noflowctl");
    switch(loopdetect){
        case 2:
            printf(" LOOP");
            break;
        case 1:
            printf(" noloop");
            break;
        default:
            break;
    }
    printf("\n");
}

void print_link_status(unsigned short int *arr){
    int i;
    union {
	uint16_t sh[13];
	uint8_t  ch[26];
    } r;
    union {
        struct {
            uint16_t low;
            uint16_t high;
	} doubleshort;
        uint32_t signlelong;
    } u;
    uint32_t port_loop_status=0;
    unsigned int EnLoopDet=0;
    
    u.doubleshort.low=rtl83xx_readreg16(0x0608);
    u.doubleshort.high=rtl83xx_readreg16(0x0609);
    for(i=0;i<switchtypes[switchtype].num_ports/2;i++){
	r.sh[i]=rtl83xx_readreg16(0x0619+i);
    }
    EnLoopDet=rtl83xx_readreg16(0x0200)&0x4;
    if (EnLoopDet) port_loop_status=rtl83xx_readreg32(0x0101);
    for(i=1;i<=switchtypes[switchtype].num_ports;i++){
        if (arr) { if (!*(arr+i-1)){ continue;} }
	print_port_link_status(
		i,
		!((u.signlelong>>(map_port_number_from_logical_to_physical(i)))&1),
		r.ch[map_port_number_from_logical_to_physical(i)],
                (EnLoopDet)?((port_loop_status>>(map_port_number_from_logical_to_physical(i)))&0x1)+1:0);
    }
}

void do_reboot(void){
    rtl83xx_setreg16(0x0000,0x0002);
}

void do_softreboot(void){
    rtl83xx_setreg16(0x0000,0x0001);
}

void print_counters(unsigned short int *arr){
    int i,port_tr;
    for (i=0;i<switchtypes[switchtype].num_ports/2;i++){
	rtl83xx_setreg16(0x0700+i,0x0000);//read rx byte, tx byte, drop byte
    }
    printf("port              RX          TX        drop\n");
    for (i=1;i<=switchtypes[switchtype].num_ports;i++){
        if (arr) { if (!*(arr+i-1)){ continue;} }
	port_tr=map_port_number_from_logical_to_physical(i);
	printf("%s/%-2d: %11lu %11lu %11lu\n",
		ifname,i,
		(unsigned long)rtl83xx_readreg32(0x070d+port_tr),
		(unsigned long)rtl83xx_readreg32(0x0727+port_tr),
		(unsigned long)rtl83xx_readreg32(0x0741+port_tr));
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
    fflush(stdout);
    _exit(!r);
}

void print_vlan_status(int show_vid){
    int i,port,port_phys;

    unsigned short vlan_status;
    union t_vlan_port_vlan vlan_port_vlan;
    union t_vlan_port_output_tag vlan_port_output_tag;
    union t_vlan_entry vlan_entry;
    unsigned short vlan_vid[32];
    union t_vlan_port_insert_vid vlan_port_insert_vid;

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
    printf( "VLAN status register: 0x%04x\n"
	    "VLAN enabled: %s\n"
	    "VLAN mode: %s\n",
	    vlan_status & 0x7f,
	    (vlan_status & 1<<0) ? "yes" : "no",
	    (vlan_status & 1<<4) ? "802.1Q" : "port-based");
    for (port=1;port<=switchtypes[switchtype].num_ports;port++){
	port_phys=map_port_number_from_logical_to_physical(port);
	//insert_vid option is available only in rtl8316b/rtl8324
        if ( (show_vid >= 0) && (show_vid != vlan_vid[vlan_port_vlan.index[port_phys]]) ) continue; 
	printf("%s/%-2d: VLAN_IDX=%02d, VID=%d, Insert_VID=%s, Out_tag_strip=%s\n",
		ifname,
		port,
		vlan_port_vlan.index[port_phys],
		vlan_vid[vlan_port_vlan.index[port_phys]],
		(switchtypes[switchtype].chip_id==rtl8326) ? "N/A" : (vlan_port_insert_vid.bitmap&(1<<port_phys) ? "yes" : "no "),
		vlan_port_output_tag_descr[(vlan_port_output_tag.bitmap>>(port_phys*2))&3]
		);
    }
    printf("IDX VID Member ports\n");
    for (i=0;i<32;i++){
	if (vlan_entry.bitmap[i]){
        if ( (show_vid > 0) && (show_vid != vlan_vid[i]) ) continue; 

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

void do_vlan_enable_vlan(int is_8021q, int do_clear){
    int i,port,vlan;
    union t_vlan_port_vlan vlan_port_vlan;
    union t_vlan_port_output_tag vlan_port_output_tag;
    union t_vlan_entry vlan_entry;
    unsigned short vlan_vid[32];
    union t_vlan_port_insert_vid vlan_port_insert_vid;

    vlan_port_output_tag.bitmap=0;
    vlan_port_insert_vid.bitmap=0;

    for(port=0;port<switchtypes[switchtype].num_ports;port++){
	vlan_port_vlan.index[port]=(do_clear)?0:(uint16_t)port;
	vlan_port_output_tag.bitmap|=3<<port*2;
        vlan_port_insert_vid.bitmap|=0;
    }

    memset(&vlan_entry,0,sizeof(vlan_entry));
    memset(&vlan_vid,0,sizeof(vlan_vid));

    if (do_clear) {
     vlan_entry.bitmap[0]=0xffffffff>>(32-switchtypes[switchtype].num_ports);
     vlan_vid[0]=(is_8021q)?1:0;
    }else{
     for(vlan=0;vlan<switchtypes[switchtype].num_ports;vlan++){
 	 if ( (vlan!=(switchtypes[switchtype].num_ports-2)) &&
              (vlan!=(switchtypes[switchtype].num_ports-1)) ) {
	     vlan_entry.bitmap[vlan]=(1<<vlan)|(1<<(switchtypes[switchtype].num_ports-2))|(1<<(switchtypes[switchtype].num_ports-1));
//	     vlan_vid[vlan]=(is_8021q)?vid_base+vlan:0;
	     vlan_vid[vlan]=0;
	 }else{
//	     vlan_entry.bitmap[vlan]=(is_8021q)?0:0xffffffff>>(32-switchtypes[switchtype].num_ports);
	     vlan_entry.bitmap[vlan]=0xffffffff>>(32-switchtypes[switchtype].num_ports);
         }
     }
    }

    //write all relevant config to switch from our data structures    
    for(i=0;i<switchtypes[switchtype].num_ports/2;i++){
	rtl83xx_setreg16(0x030c+i,vlan_port_vlan.raw[i]);
    }
    rtl83xx_setreg16(0x0319,vlan_port_output_tag.raw[0]);
    rtl83xx_setreg16(0x031a,vlan_port_output_tag.raw[1]);
    if (switchtypes[switchtype].num_ports > 16){
       rtl83xx_setreg16(0x031b,vlan_port_output_tag.raw[2]);
       rtl83xx_setreg16(0x031c,vlan_port_output_tag.raw[3]);
    }
    for(i=0;i<32;i++){
	rtl83xx_setreg16(0x031d+i*3+0,vlan_entry.raw[i*2]);
	if (switchtypes[switchtype].num_ports > 16){ rtl83xx_setreg16(0x031d+i*3+1,vlan_entry.raw[i*2+1]);}
	rtl83xx_setreg16(0x031d+i*3+2,vlan_vid[i]);
    }
    rtl83xx_setreg16(0x037d,vlan_port_insert_vid.raw[0]);
    if (switchtypes[switchtype].num_ports > 16){ rtl83xx_setreg16(0x037e,vlan_port_insert_vid.raw[1]);}
}

void do_vlan(int mode){
  swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
  switch (mode){
    case 1: // portbased
      swconfig.vlan.s.config.enable=1;
      swconfig.vlan.s.config.dot1q=0;
      break;
    case 2: // .1q
      swconfig.vlan.s.config.enable=1;
      swconfig.vlan.s.config.dot1q=1;
      break;
    default: // disable
      swconfig.vlan.s.config.enable=0;
      swconfig.vlan.s.config.dot1q=0;
  }
  rtl83xx_setreg16(0x030b,swconfig.vlan.raw[0]);
}

void do_vlan_tmpl(int mode){
 if (mode) {
   do_vlan(mode);
   do_vlan_enable_vlan(mode-1,0);
 }else{
   swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
   if (!swconfig.vlan.s.config.enable) { printf("WARNING: vlan mode not enabled\n"); } 
   do_vlan_enable_vlan(swconfig.vlan.s.config.dot1q,1);
 }
}

int str_portlist_to_array(char *list,unsigned short int *arr,unsigned int arrlen){
short int i,k;
char *s,*c,*n;
char *d[16];
unsigned int st,sp; 

 s=list;
 for (i=0;i<arrlen;i++) { *(arr+i)=0; }
 for (i=0;i<strlen(s);i++){  //check allowed symbols
  if ( ((s[i] >= '0') && (s[i] <= '9')) || (s[i] == ',') || (s[i] == '-') ) continue; 
  return(1);
 }
 while(*s){
  bzero(d,sizeof(d));
  // parsing
  if ( (c=strchr(s,',')) != NULL ) { k=c-s; n=c+1; }
  else { k=strlen(s); n=s+k; }
  if (k >= sizeof(d)) return(1);
  memcpy(d,s,k);s=n;
  // range of ports or one port?
  if (strchr((char *)d,'-')!=NULL){ 
   // range
   if (sscanf((char *)d,"%u-%u",&st,&sp) != 2) return(2);
   if ( !st || !sp || (st > sp) || (st > arrlen) || (sp > arrlen) ) return(3);
   for (i=st;i<=sp;i++) { *(arr+i-1)=1; }
  }else{
   // one port
   st=(unsigned int)strtoul((char *)d, (char **)NULL, 10);
   if ( !st || (st > arrlen) ) return(3);
   *(arr+st-1)=1;
  }
 }
 return(0);
}

int compare_command(char *argv, char **command_list){
/*
   found word from argv in command_list and return his number,
   else return -1. If found more then 1 concurrences - print
   error and close program
*/
 int i=0;
 int res=-1;
 int count=0;

 do{
  if (strlen(argv) > strlen(command_list[i])) continue;
  if (strstr(command_list[i],argv)==command_list[i]) {
   if (strcmp(argv,command_list[i])==0) {return(i);}
   res=i; count++;
  }
 }while (strlen(command_list[i++]));
 if (count > 1){
   printf("Ambiguous command: \"%s\"\n",argv);
   exit(1);
 }
 return(res);
}

// set (val==1) or reset (val==0) port bits in register with base address "base_reg",
//     for the ports listed in "arr"
void do_32bit_reg_action(unsigned short int *arr, unsigned short int val, unsigned int base_reg){
    int i;
    uint32_t portmask;
    union {
        struct {
            uint16_t low;
            uint16_t high;
        } doubleshort;
        uint32_t singlelong;
    } u;

    portmask=u.singlelong=0;
    for(i=0;i<switchtypes[switchtype].num_ports;i++){
      portmask|=(((*(arr+map_port_number_from_physical_to_logical(i)-1))&0x1)<<i);
    }
    u.doubleshort.low=rtl83xx_readreg16(base_reg);
    if (switchtypes[switchtype].num_ports > 16) u.doubleshort.high=rtl83xx_readreg16(base_reg+1);

    if (val) u.singlelong&=~portmask; 
    else u.singlelong|=portmask;

    rtl83xx_setreg16(base_reg,u.doubleshort.low);
    if (switchtypes[switchtype].num_ports > 16) rtl83xx_setreg16(base_reg+1,u.doubleshort.high);
    return;
}

void do_restrict_rrcp(unsigned short int *arr, unsigned short int val){
    do_32bit_reg_action(arr,val,0x201);
}

void do_port_qos(unsigned short int *arr,unsigned short int val){
    do_32bit_reg_action(arr,val,0x401);
}

void do_port_disable(unsigned short int *arr,unsigned short int val){
    do_32bit_reg_action(arr,val,0x608);
//    if (!val) printf ("Warning! This setting(s) can be saved and be forged after reboot\n");
}

void do_port_learning(int mode,unsigned short int *arr){
    do_32bit_reg_action(arr,mode,0x301);
//    if (!mode) printf ("Warning! This setting(s) can be saved and be forged after reboot\n");
}

void do_insert_vid(unsigned short int *arr, unsigned short int val){
    swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
    if (!swconfig.vlan.s.config.dot1q) { printf("WARNING: vlan dot1q mode not enabled\n"); }
    do_32bit_reg_action(arr,val,0x37D);
}

void do_tag_command(unsigned short int *arr, unsigned short int val){
	swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
	if (!swconfig.vlan.s.config.dot1q) { printf("WARNING: vlan dot1q mode not enabled\n"); }
	
	int i;
	int phys_port;
	union t_vlan_port_output_tag vlan_port_output_tag;
	
	for(i=0;i<4;i++){
		vlan_port_output_tag.raw[i]=rtl83xx_readreg16(0x0319+i);
	}
	
	for(i=1;i<=switchtypes[switchtype].num_ports;i++){
		if(*(arr+i-1)){
			phys_port=map_port_number_from_logical_to_physical(i);
			
			vlan_port_output_tag.bitmap&=~(3<<phys_port*2);
			vlan_port_output_tag.bitmap|=val<<phys_port*2;
		}
	}
	
	rtl83xx_setreg16(0x0319,vlan_port_output_tag.raw[0]);
	rtl83xx_setreg16(0x031a,vlan_port_output_tag.raw[1]);
	if (switchtypes[switchtype].num_ports > 16) {
		rtl83xx_setreg16(0x031b,vlan_port_output_tag.raw[2]);
		rtl83xx_setreg16(0x031c,vlan_port_output_tag.raw[3]);
	}
}

void do_vlan_table_config(unsigned short int *arr, unsigned int idx, unsigned short int val){
	swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
	if (!swconfig.vlan.s.config.dot1q) { printf("WARNING: vlan dot1q mode not enabled\n"); }
	
	do_32bit_reg_action(arr,val,0x31d+3*idx);
}

void do_vlan_index_ctrl(unsigned short int *arr, unsigned short int val){
	swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
	if (!swconfig.vlan.s.config.enable) { printf("WARNING: vlan mode not enabled\n"); }
	
	int i;
	int phys_port;
	union t_vlan_port_vlan vlan_port_vlan;
	
	for(i=0;i<13;i++){
		vlan_port_vlan.raw[i]=rtl83xx_readreg16(0x030c+i);
	}
	
	for(i=1;i<=switchtypes[switchtype].num_ports;i++){
		if(*(arr+i-1)){
			phys_port=map_port_number_from_logical_to_physical(i);
			vlan_port_vlan.index[phys_port]=val;
		}
	}
	
	for(i=0;i<switchtypes[switchtype].num_ports/2;i++){
		rtl83xx_setreg16(0x030c+i,vlan_port_vlan.raw[i]);
    }
}

void do_vlan_index_config(unsigned int idx, unsigned int val){
	swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
	if (!swconfig.vlan.s.config.dot1q) { printf("WARNING: vlan dot1q mode not enabled\n"); }
	
	rtl83xx_setreg16(0x031d+3*idx+2,val & 0xfff);
}

void do_rrcp_ctrl(int state){
    swconfig.rrcp_config.raw=rtl83xx_readreg16(0x0200);
    swconfig.rrcp_config.config.rrcp_disable=state&0x1;
    rtl83xx_setreg16(0x0200,swconfig.rrcp_config.raw);
}

void do_rrcp_echo(int state){
    swconfig.rrcp_config.raw=rtl83xx_readreg16(0x0200);
    swconfig.rrcp_config.config.echo_disable=state&0x1;
    rtl83xx_setreg16(0x0200,swconfig.rrcp_config.raw);
}

void do_loopdetect(int state){
    swconfig.rrcp_config.raw=rtl83xx_readreg16(0x0200);
    swconfig.rrcp_config.config.loop_enable=state&0x1;
    rtl83xx_setreg16(0x0200,swconfig.rrcp_config.raw);
}

void do_storm_control(int mode,int state){
    swconfig.port_config_global.raw=rtl83xx_readreg16(0x0607);
    if (mode) { // broadcast
      switch (state){ 
        case 1: // relaxed
              swconfig.port_config_global.config.storm_control_broadcast_disable=0;
              swconfig.port_config_global.config.storm_control_broadcast_strict=0;
              break;
        case 2: // strict
              swconfig.port_config_global.config.storm_control_broadcast_disable=0;
              swconfig.port_config_global.config.storm_control_broadcast_strict=1;
              break;
        default: // off
              swconfig.port_config_global.config.storm_control_broadcast_disable=1;
              swconfig.port_config_global.config.storm_control_broadcast_strict=0;
      }
    }else{ // multicast
      if (state){ 
        swconfig.port_config_global.config.storm_control_multicast_strict=1;
      }else{
        swconfig.port_config_global.config.storm_control_multicast_strict=0;
      }
    } 
    rtl83xx_setreg16(0x0607,swconfig.port_config_global.raw);
}

void do_port_config(int mode,unsigned short int *arr, int val){
  int i;
  int phys_port;
  int log_port0;
  int log_port1;

/* 
 * mode = 0 - set speed
 * mode = 1 - set flow control
 */ 

  for(i=0;i<switchtypes[switchtype].num_ports/2;i++){
    swconfig.port_config.raw[i]=rtl83xx_readreg16(0x060a+i);
  }

  for(i=1;i<=switchtypes[switchtype].num_ports;i++){
    if(*(arr+i-1)){
      phys_port=map_port_number_from_logical_to_physical(i);
      if (!mode){
         if ( (val==0x10) && (phys_port!=24) && (phys_port!=25) ){ // Small protection against the fool ;)
            printf("Port %i can't be set in 1000Mbit/s\n",i);
            continue;
         }
         swconfig.port_config.config[phys_port].autoneg=0;
         swconfig.port_config.config[phys_port].media_10half=0;
         swconfig.port_config.config[phys_port].media_10full=0;
         swconfig.port_config.config[phys_port].media_100half=0;
         swconfig.port_config.config[phys_port].media_100full=0;
         swconfig.port_config.config[phys_port].media_1000full=0;
      }
      switch (val){
        case 0x1:
               swconfig.port_config.config[phys_port].media_10half=0x1;
               break;
        case 0x2:
               swconfig.port_config.config[phys_port].media_10full=0x1;
               break;
        case 0x4:
               swconfig.port_config.config[phys_port].media_100half=0x1;
               break;
        case 0x8:
               swconfig.port_config.config[phys_port].media_100full=0x1;
               break;
        case 0x10:
               swconfig.port_config.config[phys_port].media_1000full=0x1;
               break;
        case 0x20:
               swconfig.port_config.config[phys_port].pause=0x1;
               swconfig.port_config.config[phys_port].pause_asy=0;
               break;
        case 0x40:
               swconfig.port_config.config[phys_port].pause=0;
               swconfig.port_config.config[phys_port].pause_asy=0x1;
               break;
        case 0x60:
               swconfig.port_config.config[phys_port].pause=0x1;
               swconfig.port_config.config[phys_port].pause_asy=0x1;
               break;
        default:
               if (mode){
                  swconfig.port_config.config[phys_port].pause=0;
                  swconfig.port_config.config[phys_port].pause_asy=0;
               }else{
                  swconfig.port_config.config[phys_port].autoneg=1;
                  swconfig.port_config.config[phys_port].media_10half=1;
                  swconfig.port_config.config[phys_port].media_10full=1;
                  swconfig.port_config.config[phys_port].media_100half=1;
                  swconfig.port_config.config[phys_port].media_100full=1;
                  if ( (phys_port==24) || (phys_port==25) ){
                    swconfig.port_config.config[phys_port].media_1000full=1;
                  }
               }
      }
    }
  }
  for(i=0;i<switchtypes[switchtype].num_ports/2;i++){
    log_port0=map_port_number_from_physical_to_logical(i*2);
    log_port1=map_port_number_from_physical_to_logical(i*2+1);
    if (*(arr+log_port0-1) || *(arr+log_port1-1)){ // write 2 switch only changed value
      rtl83xx_setreg16(0x060a+i,swconfig.port_config.raw[i]);
    }
  }
  printf("This operation requires a (soft) reset to force restart the auto-negotiation process\n");
  printf("If used hard reset, don't forget make \"write memory\"\n");
}

void do_port_config_bandwidth(int dir,unsigned short int *arr, int val){
  int i;
  int phys_port;
  int log_port0;
  int log_port1;

  for(i=0;i<switchtypes[switchtype].num_ports/2;i++){
    swconfig.bandwidth.raw[i]=rtl83xx_readreg16(0x020a+i);
  }

  for(i=1;i<=switchtypes[switchtype].num_ports;i++){
    if(*(arr+i-1)){
      phys_port=map_port_number_from_logical_to_physical(i);
      if ( !dir || (dir == 1)) swconfig.bandwidth.rxtx[phys_port].rx=val&0x7;
      if ( !dir || (dir == 2)) swconfig.bandwidth.rxtx[phys_port].tx=val&0x7;
    }
  }

  for(i=0;i<switchtypes[switchtype].num_ports/2;i++){
    log_port0=map_port_number_from_physical_to_logical(i*2);
    log_port1=map_port_number_from_physical_to_logical(i*2+1);
    if (*(arr+log_port0-1) || *(arr+log_port1-1)){ // write 2 switch only changed value
      rtl83xx_setreg16(0x020a+i,swconfig.bandwidth.raw[i]);
    }
  }
  if (val) printf ("Warning! This setting(s) can be saved and be forged after hardware reset\n");
}

void do_port_config_mirror(int dir,unsigned short int *arr, int dest_port ){
  int i,step;
  
  swconfig.port_monitor.sniff.sniffer=0;
  swconfig.port_monitor.sniff.sniffed_tx=0;
  swconfig.port_monitor.sniff.sniffed_rx=0;

  if (dir < 3){
    swconfig.port_monitor.sniff.sniffer=0x1<<map_port_number_from_logical_to_physical(dest_port);
    for(i=0;i<switchtypes[switchtype].num_ports;i++){
      if ( !dir || (dir==1) ) 
          swconfig.port_monitor.sniff.sniffed_rx|=(((*(arr+map_port_number_from_physical_to_logical(i)-1))&0x1)<<i);
      if ( !dir || (dir==2) ) 
          swconfig.port_monitor.sniff.sniffed_tx|=(((*(arr+map_port_number_from_physical_to_logical(i)-1))&0x1)<<i);
    }
  }

  step=(switchtypes[switchtype].num_ports > 16)?1:2;
  for (i=0;i<6;i+=step){
    rtl83xx_setreg16(0x219+i, swconfig.port_monitor.raw[0+i]);
  }
}

void do_alt_config(mode){
  swconfig.alt_config.raw=rtl83xx_readreg16(0x0300);
  switch (mode){
    case 1: 
           swconfig.alt_config.s.config.mac_aging_disable=1;
           swconfig.alt_config.s.config.mac_aging_fast=0;
           break;
    case 2: 
           swconfig.alt_config.s.config.mac_aging_disable=0;
           swconfig.alt_config.s.config.mac_aging_fast=1;
           break;
    case 3: 
           swconfig.alt_config.s.config.mac_aging_disable=0;
           swconfig.alt_config.s.config.mac_aging_fast=0;
           break;
    case 4: 
           swconfig.alt_config.s.config.mac_drop_unknown=1;
           break;
    case 5: 
           swconfig.alt_config.s.config.mac_drop_unknown=0;
           break;
    default:
           return;
  }
  rtl83xx_setreg16(0x0300,swconfig.alt_config.raw);
}

void do_glob_flowctrl(int mode, int state){
 /*
   mode: 0 - dot3x, 1 - backpressure, 3 - ondemand
   state: 0 - no, 1 - yes
 */
 if (mode < 2) swconfig.port_config_global.raw=rtl83xx_readreg16(0x0607);
 else swconfig.qos_config.raw=rtl83xx_readreg16(0x0400);
 switch (mode){
       case 0:
              swconfig.port_config_global.config.flow_dot3x_disable=(!state)&0x1;
              break;
       case 1:
              swconfig.port_config_global.config.flow_backpressure_disable=(!state)&0x1;
              break;
       case 2:
              swconfig.qos_config.config.flow_ondemand_disable=state&0x1;
              break;
       default:
              return;
 }
 if (mode < 2) rtl83xx_setreg16(0x0607,swconfig.port_config_global.raw);
 else rtl83xx_setreg16(0x0400,swconfig.qos_config.raw);
}

void do_igmp_snooping(int is_enabled){
    swconfig.alt_igmp_snooping.raw=rtl83xx_readreg16(0x0308);
    swconfig.alt_igmp_snooping.config.en_igmp_snooping=(is_enabled!=0)&0x1;
    rtl83xx_setreg16(0x0308,swconfig.alt_igmp_snooping.raw);
}

void print_allow_command(char **command_list){
 int i=0;

 while (strlen(command_list[i])){
  printf("%s\n",command_list[i++]);
 }
 return;
}

int speed_encode(char *arg){
  if ( (strcmp(arg,"a")==0) ||
       (strcmp(arg,"au")==0) ||
       (strcmp(arg,"aut")==0) ||
       (strcmp(arg,"auto")==0) ) return(0);
  if (strcmp(arg,"1000")==0) return(0x10);
  if (strcmp(arg,"100")==0) return(0x4);
  if (strcmp(arg,"10")==0)  return(0x1);
  return(-1);
}

void check_argc(int argc, int cur_argc, char *msg, char **command_list){
/*
   check number of argc and if it is last number - print error and close program
   if ptr to msg not NULL, print given error message, else - default message
*/
  if (argc == (cur_argc+1)){
   if (msg) printf("%s",msg);
   else printf("No sub-command specified, allowed commands:\n");
   if (command_list) print_allow_command(command_list);
   exit(1);
  }
}

int get_cmd_num(char *argv, int needed, char *msg, char **command_list){
/*
   search word from argv in command_list and return his number, if not found - 
   print error and close programm. 
   if ptr to msg not NULL, print given error message, else - default message.
   if `needed' not equal -1 and return number not equal value of `needed' -
   print error and close program 
*/
int subcmd=-1;
  if ((subcmd=compare_command(argv,command_list)) != -1){
   if (needed >= 0) {
    if (needed == subcmd) { return subcmd; }
   }else{ 
    return subcmd;
   }
  }
  if (msg) { printf("%s",msg); }
  else { 
   printf("Incorrect sub-command, valid commands:\n");
   print_allow_command(command_list);
  }
  exit(1);
}

void print_unknown(char *argv, char **command_list){
  printf("Unknown sub-command: \"%s\", allowed commands:\n",argv);
  print_allow_command(command_list);
  exit(1);
}

void print_usage(void){
	printf("Usage: rtl8316b [[authkey-]xx:xx:xx:xx:xx:xx@]if-name <command> [<argument>]\n");
	printf("       rtl8324 ----\"\"----\n");
	printf("       rtl8326 ----\"\"----\n");
	printf("       rtl83xx_dlink_des1016d ----\"\"----\n");
	printf("       rtl83xx_dlink_des1024d_c1 ----\"\"----\n");
	printf("       rtl83xx_dlink_des1024d_d1 ----\"\"----\n");
	printf("       rtl83xx_compex_ps2216 ----\"\"----\n");
	printf("       rtl83xx_compex_ps2216_6d ----\"\"----\n");
	printf("       rtl83xx_compex_ps2216_6e ----\"\"----\n");
        printf("       rtl83xx_ovislink_fsh2402gt ----\"\"----\n");
	printf("       rtl83xx_zyxel_es116p ----\"\"----\n");
	printf("       rtl83xx_compex_sds1224 ----\"\"----\n");
	printf("       rtl83xx_signamax_065-7531a ----\"\"----\n");
	printf("       rtl83xx_repotec_g3224x ----\"\"----\n");
	printf("       rtl83xx_edimax_es-3116p ----\"\"----\n");
	printf(" where command may be:\n");
	printf(" show running-config                   - show current switch config\n");
	printf(" show interface [<list ports>]         - print link status for ports\n");
	printf(" show interface [<list ports>] summary - print port rx/tx counters\n");
	printf(" show vlan [vid <id>]                  - show low-level vlan confg\n");
	printf(" scan [verbose] [retries <number>]     - scan network for rrcp-enabled switches\n");
	printf(" reboot [soft|hard]                    - initiate switch reboot\n");
	printf(" config interface [<list ports>] ...   - port(s) configuration\n");
	printf(" --\"\"-- [no] shutdown                - enable/disable specified port(s)\n");
	printf(" --\"\"-- speed <arg> [duplex <arg>]     - set speed/duplex on specified port(s)\n");
	printf(" --\"\"-- flow-control <arg>             - set flow-control on specified port(s)\n");
	printf(" --\"\"-- rate-limit [input|output] <arg> - set bandwidth on specified port(s)\n");
	printf(" --\"\"-- mac-address learning enable|disable - enable/disable MAC-learning on port(s)\n");
	printf(" --\"\"-- rrcp enable|disable            - enable/disable rrcp on specified ports\n");
	printf(" --\"\"-- mls qos cos 0|7                - set port priority\n");
	printf(" --\"\"-- trunk enable|disable           - enable/disable per-port VID inserting\n");
	printf(" --\"\"-- tag remove|insert-high|insert-all|none - VLAN output port priority-tagging control\n");
	printf(" --\"\"-- index <idx>                    - set port VLAN table index assignment\n");
	printf(" config rrcp enable|disable            - global rrcp enable|disable\n");
	printf(" config rrcp echo enable|disable       - rrcp echo (REP) enable|disable\n");
	printf(" config rrcp loop-detect enable|disable - network loop detect enable|disable\n"); 
        printf(" config rrcp authkey <hex-value>       - set new authkey\n"); 
	printf(" config vlan disable                   - disable all VLAN support\n");
	printf(" config vlan mode portbased|dot1q      - enable specified VLAN support\n");
	printf(" config vlan template-load portbased|dot1qtree - load specified template\n");
	printf(" config vlan clear                     - clear vlan table (all ports in vlan 1)\n");
	printf(" config vlan add port <port-list> index <idx> - add port(s) in VLAN table on index\n");
	printf(" config vlan delete port <port-list> index <idx> - delete port(s) from VLAN table on index\n");
	printf(" config vlan index <idx> vid <vid>     - set VLAN ID for index in VLAN table\n");
	printf(" config mac-address <mac>              - set a new <mac> address to switch and reboot\n");
	printf(" config mac-address-table aging-time|drop-unknown <arg>  - address lookup table control\n");
        printf(" config flowcontrol dot3x enable|disable - globally disable full duplex flow control (802.3x pause)\n");
        printf(" config flowcontrol backpressure enable|disable - globally disable/enable half duplex back pressure ability\n");
        printf(" config flowcontrol ondemand-disable enable|disable - disable/enable flow control ability auto turn off for QoS\n");
	printf(" config [no] storm-control broadcast relaxed|strict - broadcast storm control\n"); 
	printf(" config [no] storm-control multicast   - multicast storm control\n"); 
        printf(" config [no] monitor [input|output] source interface <list ports> destination interface <port>\n");
        printf(" config [no] igmp-snooping\n");
	printf(" ping                                  - test if switch is responding\n");
	printf(" write memory                          - save current config to EEPROM\n");
	printf(" write defaults                        - save to EEPROM default values\n");
        return;
}

int main(int argc, char **argv){
    unsigned int x[6];
    unsigned int ak;
    int i;
    char *p;
    int media_speed=0;
    int direction=0; 
    int bandw=0;
    int shift=0;
    int subcmd=0;
    int cmd;
    int vid=-1;
    int duplex=0;
    int negate=0;
    int t1=0;
    int t2=0;
    int exists_source=0;
    int exists_destination=0;
    int dest_port=0;
    unsigned short int *p_port_list=NULL;
    unsigned short int port_list[26];
    char *ena_disa[]={"disable","enable",""};
    char *cmd_level_1[]={"show","config","scan","reload","reboot","write","ping",""}; 
    char *show_sub_cmd[]={"running-config","startup-config","interface","vlan",""};
    char *scan_sub_cmd[]={"verbose","retries",""};
    char *show_sub_cmd_l2[]={"full","verbose",""};
    char *show_sub_cmd_l3[]={"summary",""};
    char *show_sub_cmd_l4[]={"id",""};
    char *reset_sub_cmd[]={"soft","hard",""};
    char *write_sub_cmd[]={"memory","eeprom","defaults",""};
    char *config_sub_cmd_l1[]={"interface","rrcp","vlan","mac-address","mac-address-table","flowcontrol","storm-control","monitor","vendor-id","igmp-snooping",""};
    char *config_intf_sub_cmd_l1[]={"no","shutdown","speed","duplex","rate-limit","mac-address","rrcp","mls","flow-control","trunk","tag","index",""};
    char *config_duplex[]={"half","full",""};
    char *config_rate[]={"100m","128k","256k","512k","1m","2m","4m","8m","input","output",""};
    char *config_mac_learn[]={"learning","disable","enable",""};
    char *config_port_qos[]={"7","0","qos","cos",""};
    char *config_port_flow[]={"none","asym2remote","symmetric","asym2local",""};
    char *config_rrcp[]={"disable","enable","echo","loop-detect","authkey",""};
    char *config_alt[]={"aging-time","unknown-destination",""};
    char *config_alt_time[]={"0","12","300",""};
    char *config_alt_dest[]={"drop","pass",""};
    char *config_vlan[]={"disable","transparent","clear","mode","template-load","add","delete","index",""};
    char *config_vlan_mode[]={"portbased","dot1q",""};
    char *config_vlan_tmpl[]={"portbased","dot1qtree",""};
    char *config_vlan_port[]={"port","index",""};
    char *config_vlan_idx[]={"vid",""};
    char *config_flowc[]={"dot3x","backpressure","ondemand-disable",""};
    char *config_storm[]={"broadcast","multicast",""};
    char *config_storm_br[]={"relaxed","strict",""};
    char *config_monitor[]={"interface","source","destination","input","output",""};
    char *config_tagcontrol[]={"remove","insert-high","insert-all","none",""};
    
    if (argc<3){
        print_usage();
	exit(0);
    }
    p=argv[0];
    if ((p=rindex(p,'/'))==NULL){
	p=argv[0];
    }
    if (strstr(p,"rtl8326")==argv[0]+strlen(argv[0])-7){
	switchtype=0;
    }else if (strstr(p,"rtl8316b")==argv[0]+strlen(argv[0])-8){
	switchtype=1;
    }else if (strstr(p,"rtl8316bp")==argv[0]+strlen(argv[0])-8){
	switchtype=2;
    }else if (strstr(p,"rtl8318")==argv[0]+strlen(argv[0])-8){
	switchtype=3;
    }else if (strstr(p,"rtl8324")==argv[0]+strlen(argv[0])-7){
	switchtype=4;
    }else if (strstr(p,"rtl8324p")==argv[0]+strlen(argv[0])-7){
	switchtype=5;
    }else if (strstr(p,"rtl83xx_asus_gigax_1024p")==argv[0]+strlen(argv[0])-24){
	switchtype=6;
    }else if (strstr(p,"rtl83xx_compex_sds1224")==argv[0]+strlen(argv[0])-22){
	switchtype=7;
    }else if (strstr(p,"rtl83xx_compex_ps2216")==argv[0]+strlen(argv[0])-21){
	switchtype=8;
    }else if (strstr(p,"rtl83xx_compex_ps2216_6d")==argv[0]+strlen(argv[0])-24){
	switchtype=9;
    }else if (strstr(p,"rtl83xx_compex_ps2216_6e")==argv[0]+strlen(argv[0])-24){
	switchtype=10;
    }else if (strstr(p,"rtl83xx_dlink_des1016d")==argv[0]+strlen(argv[0])-22){
	switchtype=11;
    }else if (strstr(p,"rtl83xx_dlink_des1024d_b1")==argv[0]+strlen(argv[0])-25){
	switchtype=12;
    }else if (strstr(p,"rtl83xx_dlink_des1024d_c1")==argv[0]+strlen(argv[0])-25){
	switchtype=13;
    }else if (strstr(p,"rtl83xx_edimax_es-3116p")==argv[0]+strlen(argv[0])-23){
	switchtype=14;
    }else if (strstr(p,"rtl83xx_ovislink_fsh2402gt")==argv[0]+strlen(argv[0])-26){
	switchtype=15;
    }else if (strstr(p,"rtl83xx_repotec_g3224x")==argv[0]+strlen(argv[0])-22){
	switchtype=16;
    }else if (strstr(p,"rtl83xx_signamax_065-7531a")==argv[0]+strlen(argv[0])-26){
	switchtype=17;
    }else if (strstr(p,"rtl83xx_zyxel_es116p")==argv[0]+strlen(argv[0])-20){ 
        switchtype=18; 
    }else if (strcmp(argv[2],"scan")==0){ //allow scan command to be runt without specifying chip/switch
	switchtype=0;
    }else {
	printf("%s: unknown switch/chip type\n",argv[0]);
	exit(0);
    }

    if (sscanf(argv[1], "%x-%x:%x:%x:%x:%x:%x@%s",&ak,x,x+1,x+2,x+3,x+4,x+5,ifname)==8){
        if (ak > 0xffff) {
          printf("invalid authkey 0x%x\n",ak);
  	  exit(0);
        }
        authkey=(uint16_t)ak;
	for (i=0;i<6;i++){
	    dest_mac[i]=(unsigned char)x[i];
	}
    }else if (sscanf(argv[1], "%x:%x:%x:%x:%x:%x@%s",x,x+1,x+2,x+3,x+4,x+5,ifname)==7){
	for (i=0;i<6;i++){
	    dest_mac[i]=(unsigned char)x[i];
	}
    }else if (argc>=2 && strcmp(argv[2],"scan")==0){
	strcpy(ifname,argv[1]);
    }else{
	printf("%s: malformed switch reference: '%s'\n",argv[0],argv[1]);
	printf("Usage: rtl8316b [[authkey-]xx:xx:xx:xx:xx:xx@]if-name <command> [<argument>]\n");
	exit(0);	
    }

    if (if_nametoindex(ifname)<=0){
	printf("%s: no such interface: %s\n",argv[0],ifname);
	exit(0);
    }

    if (argc<2 || strcmp(argv[2],"scan")!=0){
	printf("! rtl83xx: trying to reach %d-port \"%s %s\" switch at %s\n",
	    switchtypes[switchtype].num_ports,
	    switchtypes[switchtype].vendor,
	    switchtypes[switchtype].model,
	    argv[1]);
    }

    engage_timeout(30);
    if (rtl83xx_prepare()){ printf("%s\n",ErrIOMsg); exit(1);}

    cmd=compare_command(argv[2+shift],&cmd_level_1[0]);
    switch (cmd){
         case 0: //show
                check_argc(argc,2+shift,NULL,&show_sub_cmd[0]);
                switch(compare_command(argv[3+shift],&show_sub_cmd[0])){
                   case 0: // running-config
   	                  if (argc == (3+shift+1)){
	           	     do_show_config(0);
                             exit(0);
                          }
                          (void) get_cmd_num(argv[4+shift],-1,NULL,&show_sub_cmd_l2[0]);
	                  do_show_config(1);
                          exit(0);
                   case 1: // startup-config
                          printf("Under construction\n");
                          exit(0);
                   case 2: // interfaces
   	                  if (argc == (3+shift+1)){
                             print_link_status(NULL);
                             exit(0);
                          }
                          if (str_portlist_to_array(argv[4+shift],&port_list[0],switchtypes[switchtype].num_ports)==0){
                            p_port_list=&port_list[0];
   	                    if (argc == (4+shift+1)){
                               print_link_status(p_port_list);
                               exit(0);
                            }
                            shift++;
                          }
                          (void) get_cmd_num(argv[4+shift],0,NULL,&show_sub_cmd_l3[0]);
	                  print_counters(p_port_list);
                          exit(0);
                   case 3: // vlan
   	                  if (argc == (3+shift+1)){
                             print_vlan_status(-1);
                             exit(0);
                          }
                          (void) get_cmd_num(argv[4+shift],0,NULL,&show_sub_cmd_l4[0]);
                          if (argc > (4+shift+1)){
                            if (sscanf(argv[5+shift], "%i",&vid) == 1) { 
                              print_vlan_status(vid);
                              exit(0); 
                            }
                          }
	                  printf("Vlan-id not specified\n");
                          exit(1);
                   default: 
                          print_unknown(argv[3+shift],&show_sub_cmd[0]);
                }
         case 1: //config
                check_argc(argc,2+shift,NULL,&config_sub_cmd_l1[0]);
                if (strcmp(argv[3+shift],"no")==0){ 
                  shift++; negate++; 
                  check_argc(argc,2+shift,NULL,&config_sub_cmd_l1[0]);
                }
                switch (compare_command(argv[3+shift],&config_sub_cmd_l1[0])){
                   case 0: // interface
                          check_argc(argc,3+shift,"No list of ports\n",NULL);
                          if (str_portlist_to_array(argv[4+shift],&port_list[0],switchtypes[switchtype].num_ports)!=0){
                             printf("Incorrect list of ports: \"%s\"\n",argv[4+shift]);
                             exit(1);
                          }
                          check_argc(argc,4+shift,NULL,&config_intf_sub_cmd_l1[0]);
                          switch (compare_command(argv[5+shift],&config_intf_sub_cmd_l1[0])){
                                 case 0: // no
                                        check_argc(argc,5+shift,"No sub-command, allowed commands: shutdown\n",NULL);
                                        (void) get_cmd_num(argv[6+shift],1,"Incorrect sub-commands, allowed: shutdown\n",&config_intf_sub_cmd_l1[0]);
                                        do_port_disable(&port_list[0],1);
                                        exit(0);
                                 case 1: // shutdown
                                        do_port_disable(&port_list[0],0);
                                        exit(0);
                                 case 2: // speed
                                        check_argc(argc,5+shift,"No sub-command, allowed commands: 10|100|1000|auto [duplex half|full]\n",NULL);
                                        if ((media_speed=speed_encode(argv[6+shift])) == -1){
                                          printf("Incorect speed, valid are: 10|100|1000|auto\n");
                                          exit(1);
                                        }
                                        if ( (argc > 6+shift+1) && (media_speed!=0) && (media_speed!=0x8) ){ //duplex
                                          (void)get_cmd_num(argv[7+shift],3,"Incorrect sub-command, allowed commands: duplex half|full\n",&config_intf_sub_cmd_l1[0]);
                                          check_argc(argc,7+shift,NULL,&config_duplex[0]);
                                          duplex=get_cmd_num(argv[8+shift],-1,NULL,&config_duplex[0]);
                                          media_speed=media_speed<<duplex;
                                        }
                                        do_port_config(0,&port_list[0],media_speed);
                                        exit(0);
                                 case 4:  //rate-limit
                                        for(;;){
                                           check_argc(argc,5+shift,"No sub-command, allowed commands: [input|output] 128K|256K|512K|1M|2M|4M|8M|100M\n",NULL);
                                           for(i=0;i<strlen(argv[6+shift]);i++) argv[6+shift][i]=tolower(argv[6+shift][i]);
                                           bandw=get_cmd_num(argv[6+shift],-1,"Incorrect sub-commands, allowed: [input|output] 128K|256K|512K|1M|2M|4M|8M|100M\n",&config_rate[0]);
                                           if (bandw > 7){
                                             direction=bandw-7;
                                             shift++;
                                             continue;
                                           }
                                           do_port_config_bandwidth(direction,&port_list[0],bandw);
                                           exit(0);
                                        };
                                 case 5:  //mac-address
                                        for(;;){
                                           check_argc(argc,5+shift,"No sub-command, allowed commands: learning enable|disable\n",NULL);
                                           subcmd=get_cmd_num(argv[6+shift],-1,"Incorrect sub-commands, allowed: learning enable|disable\n",&config_mac_learn[0]);
                                           if (!subcmd){
                                             shift++;
                                             continue;
                                           }
                                           do_port_learning(subcmd-1,&port_list[0]);
                                           exit(0);
                                        }
                                 case 6:  // rrcp
                                        check_argc(argc,5+shift,"No sub-command, allowed commands: enable|disable\n",NULL);
                                        subcmd=get_cmd_num(argv[6+shift],-1,"Incorrect sub-commands, allowed: enable|disable\n",&ena_disa[0]);
                                        do_restrict_rrcp(&port_list[0],subcmd);
                                        exit(0);
                                 case 7: // mls qos
                                        for(;;){
                                           check_argc(argc,5+shift,"No sub-command, allowed commands: qos cos 0|7\n",NULL);
                                           subcmd=get_cmd_num(argv[6+shift],-1,"Incorrect sub-commands, allowed: qos cos 0|7\n",&config_port_qos[0]);
                                           if (subcmd > 1){
                                             shift++;
                                             continue;
                                           }
                                           do_port_qos(&port_list[0],subcmd);
                                           exit(0);
                                        }
                                 case 8:  // flow control
                                        check_argc(argc,5+shift,NULL,&config_port_flow[0]);
                                        subcmd=get_cmd_num(argv[6+shift],-1,NULL,&config_port_flow[0]);
                                        if (subcmd) subcmd=subcmd<<5;
                                        do_port_config(1,&port_list[0],subcmd);
                                        exit(0);
                                 case 9:  // trunk
                                        check_argc(argc,5+shift,"No sub-command, allowed commands: enable|disable\n",NULL);
                                        subcmd=get_cmd_num(argv[6+shift],-1,"Incorrect sub-commands, allowed: enable|disable\n",&ena_disa[0]);
                                        do_insert_vid(&port_list[0],!subcmd);
                                        exit(0);
                                 case 10: // tag
                                        check_argc(argc,5+shift,"No sub-command, allowed commands: remove|insert-high|insert-all|none\n",NULL);
                                        subcmd=get_cmd_num(argv[6+shift],-1,"Incorrect sub-commands, allowed: remove|insert-high|insert-all|none\n",&config_tagcontrol[0]);
                                        do_tag_command(&port_list[0],subcmd);
                                        exit(0);
                                 case 11: // index
										check_argc(argc,5+shift,"Incorrect VLAN index.\n",NULL);
										
										if(!sscanf(argv[6+shift], "%d", &subcmd)){
											printf("Incorrect VLAN index: \"%s\"\n",argv[6+shift]);
											exit(1);
										} else if(subcmd < 0 || subcmd > 31) {
											printf("Incorrect VLAN index: \"%s\"\n",argv[6+shift]);
											printf("This hardware can store only 32 indexes (0-31).\n");
											exit(1);
										}
										
										do_vlan_index_ctrl(&port_list[0],subcmd);
										
										exit(0);
                                 default:
                                         print_unknown(argv[5+shift],&config_intf_sub_cmd_l1[0]);
                          }
                   case 1: // rrcp
                          check_argc(argc,3+shift,NULL,&config_rrcp[0]);
                          switch (compare_command(argv[4+shift],&config_rrcp[0])){
                                 case 0: // rrcp disable
                                        do_rrcp_ctrl(1);
                                        exit(1);
                                 case 1: // rrcp enable
                                        do_rrcp_ctrl(0);
                                        exit(1);
                                 case 2: // rrcp echo
                                        check_argc(argc,4+shift,"No sub-command, allowed commands: enable|disable\n",NULL);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&ena_disa[0]);
                                        do_rrcp_echo(!subcmd);
                                        exit(0);
                                 case 3: // rrcp loopdetect
                                        check_argc(argc,4+shift,"No sub-command, allowed commands: enable|disable\n",NULL);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&ena_disa[0]);
                                        do_loopdetect(subcmd);
                                        exit(0);
                                 case 4: // rrcp authkey
                                        check_argc(argc,4+shift,"Authkey needed\n",NULL);
                                        if (sscanf(argv[5+shift],"%04x",&ak) == 1){
                                          if (ak <= 0xffff) {
                                            rtl83xx_setreg16(0x209,ak);
                                            printf ("Setting of new authkey is no save into EEPROM and may be forged after reboot.\n");
                                            printf ("After change authkey switch not answering on broadcast \"Hello\" scan, be close.\n");
                                            exit(0);
                                          }
                                         }
                                         printf("Invalid Authkey\n");
                                         exit(1);
                                 default:
                                         print_unknown(argv[4+shift],&config_rrcp[0]);
                          }
                   case 2: // vlan
                          check_argc(argc,3+shift,NULL,&config_vlan[0]);
                          switch (compare_command(argv[4+shift],&config_vlan[0])){
                                 case 0: // disable
                                 case 1: // transparent
                                        do_vlan(0);
                                        exit(0);
                                 case 2: //clear
                                        do_vlan_tmpl(0);
                                        exit(0);
                                 case 3: // mode
                                        check_argc(argc,4+shift,NULL,&config_vlan_mode[0]);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&config_vlan_mode[0]);
                                        do_vlan(subcmd+1);
                                        exit(0);
                                 case 4: // template-load
                                        check_argc(argc,4+shift,NULL,&config_vlan_tmpl[0]);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&config_vlan_tmpl[0]);
                                        do_vlan_tmpl(subcmd+1);
                                        exit(0);
                                 case 5: // add
										for(i=4;i<=7;i++) check_argc(argc,i+shift,NULL,&config_vlan_port[0]);
										if(strcmp(argv[5+shift],config_vlan_port[0])) print_unknown(argv[5+shift],&config_vlan_port[0]);
										if (str_portlist_to_array(argv[6+shift],&port_list[0],switchtypes[switchtype].num_ports)!=0){
											printf("Incorrect list of ports: \"%s\"\n",argv[6+shift]);
											exit(1);
										}
										if(strcmp(argv[7+shift],config_vlan_port[1])) print_unknown(argv[7+shift],&config_vlan_port[0]);
										
										if(!sscanf(argv[8+shift], "%d", &subcmd)){
											printf("Incorrect VLAN index: \"%s\"\n",argv[8+shift]);
											exit(1);
										} else if(subcmd < 0 || subcmd > 31) {
											printf("Incorrect VLAN index: \"%s\"\n",argv[8+shift]);
											printf("This hardware can store only 32 indexes (0-31).\n");
											exit(1);
										}
										do_vlan_table_config(&port_list[0],subcmd,0);
										exit(0);
                                 case 6: // delete
										for(i=4;i<=7;i++) check_argc(argc,i+shift,NULL,&config_vlan_port[0]);
										if(strcmp(argv[5+shift],config_vlan_port[0])) print_unknown(argv[5+shift],&config_vlan_port[0]);
										if (str_portlist_to_array(argv[6+shift],&port_list[0],switchtypes[switchtype].num_ports)!=0){
											printf("Incorrect list of ports: \"%s\"\n",argv[6+shift]);
											exit(1);
										}
										if(strcmp(argv[7+shift],config_vlan_port[1])) print_unknown(argv[7+shift],&config_vlan_port[0]);
										
										if(!sscanf(argv[8+shift], "%d", &subcmd)){
											printf("Incorrect VLAN index: \"%s\"\n",argv[8+shift]);
											exit(1);
										} else if(subcmd < 0 || subcmd > 31) {
											printf("Incorrect VLAN index: \"%s\"\n",argv[8+shift]);
											printf("This hardware can store only 32 indexes (0-31).\n");
											exit(1);
										}
										do_vlan_table_config(&port_list[0],subcmd,1);
										exit(0);
                                 case 7: // index
                                        for(i=4;i<=6;i++) check_argc(argc,i+shift,NULL,&config_vlan_idx[0]);
										
										if(!sscanf(argv[5+shift], "%d", &vid)){
											printf("Incorrect VLAN index: \"%s\"\n",argv[5+shift]);
											exit(1);
										} else if(vid < 0 || vid > 31) {
											printf("Incorrect VLAN index: \"%s\"\n",argv[5+shift]);
											printf("This hardware can store only 32 indexes (0-31).\n");
											exit(1);
										}
										
										if(strcmp(argv[6+shift],config_vlan_idx[0])) print_unknown(argv[6+shift],&config_vlan_idx[0]);
										
										if(!sscanf(argv[7+shift], "%d", &subcmd)){
											printf("Incorrect VID: \"%s\"\n",argv[7+shift]);
											exit(1);
										} else if(subcmd == 0 || subcmd == 1 || subcmd == 4095) {
											printf("WARNING: VID %d is not recommended.\n", subcmd);
										} else if(subcmd < 0 || subcmd > 4095) {
											printf("Incorrect VID: \"%s\"\n",argv[7+shift]);
											printf("VID can be only between 0-4095. Values 0, 1 and 4095 are not recommended.\n");
											exit(1);
										}
										
										do_vlan_index_config(vid,subcmd);
										
                                        exit(0);
                                 default: 
                                         print_unknown(argv[4+shift],&config_vlan[0]);
                          }
                   case 3: // mac-address
                          check_argc(argc,3+shift,"MAC address needed\n",NULL);
   	                  if ((sscanf(argv[4+shift], "%02x:%02x:%02x:%02x:%02x:%02x",x,x+1,x+2,x+3,x+4,x+5)!=6)&&
	                      (sscanf(argv[4+shift], "%02x%02x.%02x%02x.%02x%02x",x,x+1,x+2,x+3,x+4,x+5)!=6)){
   		              printf("malformed mac address: '%s'!\n",argv[4+shift]);
                              exit(1);
                          }
		          for (i=0;i<6;i++){
		            if (do_write_eeprom_byte(0x12+i,(unsigned char)x[i])){
		             printf ("error writing eeprom!\n");
		             exit(1);
		            }
	                  }
		          do_reboot();
                          exit(0);
                   case 4: // mac-address-table
                          check_argc(argc,3+shift,NULL,&config_alt[0]);
                          switch (compare_command(argv[4+shift],&config_alt[0])){
                                 case 0: // aging-time
                                        check_argc(argc,4+shift,NULL,&config_alt_time[0]);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&config_alt_time[0]);
                                        do_alt_config(subcmd+1);
                                        exit(0);
                                 case 1: // unknown-destination
                                        check_argc(argc,4+shift,NULL,&config_alt_dest[0]);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&config_alt_dest[0]);
                                        do_alt_config(subcmd+4);
                                        exit(0);
                                 default: 
                                         print_unknown(argv[4+shift],&config_alt[0]);
                          }
                   case 5: // flowcontrol
                          check_argc(argc,3+shift,NULL,&config_flowc[0]);
                          subcmd=get_cmd_num(argv[4+shift],-1,NULL,&config_flowc[0]);
                          check_argc(argc,4+shift,NULL,&ena_disa[0]);
                          do_glob_flowctrl(subcmd,get_cmd_num(argv[5+shift],-1,NULL,&ena_disa[0]));
                          exit(0);
                   case 6: // storm-control
                          check_argc(argc,3+shift,NULL,&config_storm[0]);
                          switch (compare_command(argv[4+shift],&config_storm[0])){
                                 case 0: // broadcast
                                        check_argc(argc,4+shift,NULL,&config_storm_br[0]);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&config_storm_br[0]);
                                        do_storm_control(1,(negate)?0:++subcmd);
                                        exit(0);
                                 case 1: // multicast
                                        do_storm_control(0,!negate);
                                        exit(0);
                                 default: 
                                         print_unknown(argv[4+shift],&config_alt[0]);
                          }
                   case 7: // monitor
                          if (switchtypes[switchtype].chip_id==rtl8326){
                            printf("Port mirroring not working with hardware based on rtl8326/rtl8326s\n");
                            exit(1);
                          } 
                          if (negate) {
                            do_port_config_mirror(3,&port_list[0],1);
                            exit(0);
                          }
                          direction=0;
                          for (;;){
                             if (exists_source && exists_destination){
                               do_port_config_mirror(direction,&port_list[0],dest_port);
                               exit(0);
                             }
                             check_argc(argc,3+shift,NULL,&config_monitor[0]);
                             switch (compare_command(argv[4+shift],&config_monitor[0])){
                                   case 1:
                                          t1++;
                                          shift++;
                                          continue;
                                   case 2:
                                          t2++;
                                          shift++;
                                          continue;
                                   case 3:
                                          direction=1;
                                          shift++;
                                          continue;
                                   case 4:
                                          direction=2;
                                          shift++;
                                          continue;
                                   case 0:
                                          shift++;
                                          if (t1){
                                            check_argc(argc,3+shift,"No list of source ports\n",NULL);
                                            if (str_portlist_to_array(argv[4+shift],&port_list[0],switchtypes[switchtype].num_ports)!=0){
                                              printf("Incorrect list of ports: \"%s\"\n",argv[4+shift]);
                                              exit(1);
                                            }
                                            t1=0; exists_source++;
                                            shift++;
                                            continue;
                                          }
                                          if (t2){
                                            check_argc(argc,3+shift,"No destination port\n",NULL);
                                            if (sscanf(argv[4+shift], "%i",&dest_port) != 1) { 
                                              printf("Incorrect port: \"%s\"\n",argv[4+shift]);
                                              exit(0); 
                                            }
                                            t2=0; exists_destination++;
                                            shift++;
                                            continue;
                                          }
                                          printf("Needed port(s)\n");
                                          exit(0);
                                   default: 
                                           print_unknown(argv[4+shift],&config_monitor[0]);
                             }
                          }
                   case 8: // vendor-id
                          check_argc(argc,3+shift,"vendor-id needed\n",NULL);
   	                  if (sscanf(argv[4+shift], "%02x%02x%02x%02x",x,x+1,x+2,x+3)!=4){
   		              printf("malformed vendor-id: '%s'!\n",argv[4+shift]);
                              exit(1);
                          }
		          for (i=0;i<4;i++){
		            if (do_write_eeprom_byte(0x1a+(3-i),(unsigned char)x[i])){
		             printf ("error writing eeprom!\n");
		             exit(1);
		            }
	                  }
		          do_reboot();
                          exit(0);

                   case 9: // igmp-snooping
                          do_igmp_snooping(!negate);
                          exit(0);

                   default:
                          print_unknown(argv[3+shift],&config_sub_cmd_l1[0]);
                }
                break;
         case 2: //scan
		{
		    int verbose=0;
		    int retries=5;
		    while (1){
			if (argc==shift+3){
			    printf("! rtl83xx: scannig. is_verbose=%d, retries=%d\n",verbose,retries);
			    rtl83xx_scan(verbose,retries);
			    exit(0);
			}else{
        		    check_argc(argc,2+shift,NULL,&scan_sub_cmd[0]);
            		    switch (compare_command(argv[3+shift],&scan_sub_cmd[0])){
				case 0: //verbose
				    verbose=1;
				    shift++;
				    continue;
        			case 1: //retries
        			    if (argc<shift+5 || atoi(argv[4+shift])<=0 || atoi(argv[4+shift])>100 ){
					printf("retries number not specified or out of range (1..100)!\n");
					exit(0);
        			    }else{
        				retries=atoi(argv[4+shift]);
					shift+=2;
					continue;
        			    }
				default:
				    print_unknown(argv[3+shift],&scan_sub_cmd[0]);
				    exit(0);
			    }
			}
		    }
		}
         case 3: //reload
         case 4: //reboot
  	        if (argc == (2+shift+1)){
                   do_reboot();
                   exit(0);
                }
                if (!get_cmd_num(argv[3+shift],-1,NULL,&reset_sub_cmd[0])){
	          do_softreboot();
                }else{
	          do_reboot();
                }
                exit(0);
         case 5: //write
                check_argc(argc,2+shift,NULL,&write_sub_cmd[0]);
                switch(compare_command(argv[3+shift],&write_sub_cmd[0])){
                   case 0:
                   case 1:
                          do_write_memory();
                          exit(0);
                   case 2:
                          do_write_eeprom_defaults();
                          exit(0);
                   default:
                          print_unknown(argv[3+shift],&write_sub_cmd[0]);
                }
         case 6: //ping
                do_ping();
                exit(0);
         default:
                print_usage();
    }
    exit(0);
}
