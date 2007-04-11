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
    for (i=0;i<=0x0c;i++){
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
    _exit(!r);
}

void print_vlan_status(void){
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
    printf("VLAN Status reg=0x%04x VLAN enabled: %s, 802.1Q enabled: %s\n",
	    vlan_status & 0x7f,
	    (vlan_status & 1<<0) ? "yes" : "no",
	    (vlan_status & 1<<4) ? "yes" : "no");
    for (port=1;port<=switchtypes[switchtype].num_ports;port++){
	port_phys=map_port_number_from_logical_to_physical(port);
	//insert_vid option is available only in rtl8316b
	printf("%s/%-2d: VLAN_IDX=%02d, VID=%d, Insert_VID=%s, Out_tag_strip=%s\n",
		ifname,
		port,
		vlan_port_vlan.index[port_phys],
		vlan_vid[vlan_port_vlan.index[port_phys]],
		(switchtypes[switchtype].chip_id==rtl8316b) ? (vlan_port_insert_vid.bitmap&(1<<port_phys) ? "yes" : "no ") : "N/A",
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
    union t_vlan_port_vlan vlan_port_vlan;
    union t_vlan_port_output_tag vlan_port_output_tag;
    union t_vlan_entry vlan_entry;
    unsigned short vlan_vid[32];
    union t_vlan_port_insert_vid vlan_port_insert_vid;

    //fill-up local data structures
    if (is_8021q){
	vlan_status=(rtl83xx_readreg32(0x030b)|1)|(1<<4);
    }else{
	vlan_status=(rtl83xx_readreg32(0x030b)|1)&~(1<<4);
    }

    if (root_port < 0){
      rtl83xx_setreg16reg16(0x030b,vlan_status);
      return;
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

void do_vlan_disable(){

  swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
  swconfig.vlan.s.config.enable=0;
  if (swconfig.vlan.s.config.dot1q) swconfig.vlan.s.config.dot1q=0;
  rtl83xx_setreg16(0x030b,swconfig.vlan.raw[0]);
}

void do_write_memory(){
 int i,numreg;

 numreg=(switchtypes[switchtype].num_ports==16)?12:13;
 rrcp_config_read_from_switch();
 if (do_write_eeprom(0x0d,swconfig.rrcp_config.raw)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x0f,swconfig.rrcp_byport_disable.raw[0])) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x11,swconfig.rrcp_byport_disable.raw[1])) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x23,swconfig.alt_config.raw)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x29,swconfig.vlan.raw[0])) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x2f,swconfig.qos_config.raw)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x31,swconfig.qos_port_priority.raw[0])) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x33,swconfig.qos_port_priority.raw[1])) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x39,swconfig.port_config_global.raw)) {printf("write eeprom\n");exit(1);}
 for(i=0;i<numreg;i++){
    if (do_write_eeprom(0x3b+i*2,swconfig.port_config.raw[i])) {printf("write eeprom\n");exit(1);}
 }
}

void do_write_eeprom_defaults(){
 int i;
 uint16_t data;

 if (do_write_eeprom(0x01,0x0a80)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x03,0x0155)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x0d,0)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x0f,0)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x11,0)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x23,0x0004)) {printf("write eeprom\n");exit(1);}
 if (do_read_eeprom(0x25,&data) != 0) {printf("read eeprom\n");exit(1);}
 else if (do_write_eeprom(0x25,data&0xffe1)) {printf("write eeprom\n");exit(1);}
 if (do_read_eeprom(0x27,&data) != 0) {printf("read eeprom\n");exit(1);}
 else if (do_write_eeprom(0x27,data&0xfffe)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x29,0)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x2f,0x0010)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x31,0)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x33,0)) {printf("write eeprom\n");exit(1);}
 if (do_write_eeprom(0x39,0x0010)) {printf("write eeprom\n");exit(1);}
 for(i=0;i<12;i++){
    if (do_write_eeprom(0x3b+i*2,0xafaf)) {printf("write eeprom\n");exit(1);}
 }
 if (switchtypes[switchtype].num_ports==26) {if (do_write_eeprom(0x53,0xbfbf)) {printf("write eeprom\n");exit(1);}}
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
 int i=0;

 do{
//  printf("Compare %s %s\n",argv,command_list[i]);
  if (strlen(argv) > strlen(command_list[i])) continue;
  if (strstr(command_list[i],argv)==command_list[i]) return(i);
 }while (strlen(command_list[i++]));
 return(-1);
}

void do_restrict_rrcp(unsigned short int *arr){
    int i;
    union t_rrcp_status u;

    u.signlelong=0;
    for(i=0;i<switchtypes[switchtype].num_ports;i++){
      u.signlelong|=(((~*(arr+map_port_number_from_physical_to_logical(i)-1))&0x1)<<i);
    }
    rtl83xx_setreg16(0x0201,u.doubleshort.low);
    rtl83xx_setreg16(0x0202,u.doubleshort.high);
}

void print_rrcp_status(void){
    int i;
    union t_rrcp_status u;

    u.doubleshort.low=rtl83xx_readreg16(0x0201);
    u.doubleshort.high=rtl83xx_readreg16(0x0202);
    for(i=1;i<=switchtypes[switchtype].num_ports;i++){
      printf("%s/%-2d: ",ifname,i);
      if ( ((u.signlelong>>(map_port_number_from_logical_to_physical(i)))&0x1) == 0) printf("ENABLED\n");
      else printf("disable\n");
    }
}

void do_port_disable(unsigned short int *arr,unsigned short int val){
    int i;
    uint32_t portmask;

    portmask=swconfig.port_disable.bitmap=0;
    for(i=0;i<switchtypes[switchtype].num_ports;i++){
      portmask|=(((*(arr+map_port_number_from_physical_to_logical(i)-1))&0x1)<<i);
    }
    swconfig.port_disable.raw[0]=rtl83xx_readreg16(0x0608);
    if (switchtypes[switchtype].num_ports > 16) swconfig.port_disable.raw[1]=rtl83xx_readreg16(0x0609);

    if (val) swconfig.port_disable.bitmap&=~portmask; 
    else swconfig.port_disable.bitmap|=portmask;

    rtl83xx_setreg16(0x0608,swconfig.port_disable.raw[0]);
    if (switchtypes[switchtype].num_ports > 16) rtl83xx_setreg16(0x0609,swconfig.port_disable.raw[1]);
    
    if (!val) printf ("Warning! This setting(s) can be saved and be forged after reboot\n");
}

void do_port_learning(int mode,unsigned short int *arr){
    int i;
    uint32_t portmask;

    portmask=swconfig.alt_mask.mask=0;
    for(i=0;i<switchtypes[switchtype].num_ports;i++){
      portmask|=(((*(arr+map_port_number_from_physical_to_logical(i)-1))&0x1)<<i);
    }
    swconfig.alt_mask.raw[0]=rtl83xx_readreg16(0x0301);
    if (switchtypes[switchtype].num_ports > 16) swconfig.alt_mask.raw[1]=rtl83xx_readreg16(0x0302);

    if (mode) swconfig.alt_mask.mask&=~portmask; 
    else swconfig.alt_mask.mask|=portmask;

    rtl83xx_setreg16(0x0301,swconfig.alt_mask.raw[0]);
    if (switchtypes[switchtype].num_ports > 16) rtl83xx_setreg16(0x0301,swconfig.alt_mask.raw[1]);
    
    if (!mode) printf ("Warning! This setting(s) can be saved and be forged after reboot\n");
}

void do_loopdetect(int state){
    swconfig.rrcp_config.raw=rtl83xx_readreg16(0x0200);
    swconfig.rrcp_config.config.loop_enable=state&0x1;
    rtl83xx_setreg16(0x0200,swconfig.rrcp_config.raw);
}

void do_broadcast_storm_control(int state){
    swconfig.port_config_global.raw=rtl83xx_readreg16(0x0607);
    swconfig.port_config_global.config.storm_control_broadcast_disable=~(state&0x1);
    rtl83xx_setreg16(0x0607,swconfig.port_config_global.raw);
}

int port_mode_encode(int mode,char *arg){
 if (mode){ //flow control
   if (strcmp(arg,"none")==0) return(0);
   if (strcmp(arg,"asym2remote")==0)  return(0x20);
   if (strcmp(arg,"symmetric")==0)  return(0x40);
   if (strcmp(arg,"asym2local")==0) return(0x60);
 }else{ //speed
   if (strcmp(arg,"auto")==0) return(0);
   if (strcmp(arg,"10h")==0)  return(0x1);
   if (strcmp(arg,"10f")==0)  return(0x2);
   if (strcmp(arg,"100h")==0) return(0x4);
   if (strcmp(arg,"100f")==0) return(0x8);
   if (strcmp(arg,"1000")==0) return(0x10);
}
 return(-1); 
}

int bandwidth_encode(char *arg){
 int i;
 for(i=0;i<strlen(arg);i++) arg[i]=toupper(arg[i]);
 for(i=0;i<8;i++) if (strcmp(arg,bandwidth_text[i])==0) return(i);
 return(-1);
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
           swconfig.alt_config.s.config.mac_aging_fast=0;
           break;
    case 3: 
           swconfig.alt_config.s.config.mac_aging_disable=0;
           swconfig.alt_config.s.config.mac_aging_fast=1;
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

void print_allow_command(char **command_list){
 int i=0;

 while (strlen(command_list[i])){
  printf("%s\n",command_list[i++]);
 }
 return;
}

void print_usage(void){
	printf("Usage: rtl8316b [[authkey-]xx:xx:xx:xx:xx:xx@]if-name <command> [<argument>]\n");
	printf("       rtl8326 ----\"\"----\n");
	printf("       rtl83xx_dlink_des1016d ----\"\"----\n");
	printf("       rtl83xx_dlink_des1024d ----\"\"----\n");
	printf("       rtl83xx_compex_ps2216 ----\"\"----\n");
        printf("       rtl83xx_ovislink_fsh2402gt ----\"\"----\n");
	printf("       rtl83xx_zyxel_es116p ----\"\"----\n");
	printf(" where command may be:\n");
	printf(" show running-config          - show current switch config\n");
	printf(" show interface [<list ports>]- print link status for ports\n");
	printf(" show interface [<list ports>] summary - print port rx/tx counters\n");
	printf(" scan [verbose]               - scan network for rrcp-enabled switches\n");
	printf(" reboot [soft|hard]           - initiate switch reboot\n");
//	printf(" vlan status                  - show low-level vlan confg\n");
//	printf(" vlan enable_hvlan [<port>]   - configure switch as home-vlan tree with specified uplink port\n");
//	printf(" vlan enable_8021q [<port>]   - configure switch as IEEE 802.1Q vlan tree with specified uplink port\n");
//	printf(" vlan disable               - disable all VLAN support\n");
//	printf(" restrict-rrcp <list ports>   - enable rrcp on specified ports, disable on other\n");
//	printf(" restrict-rrcp status         - print rrcp status for all ports\n");
//	printf(" bcast-storm-ctrl enable|disable   - broadcast storm control on/off\n"); 
//	printf(" loopdetect enable|disable         - network loop detect on/off\n"); 
//	printf(" port <list ports> enable|disable  - enable/disable specified port(s)\n");
//	printf(" port <list ports> media <speed>   - set speed/duplex on specified port(s)\n");
//	printf(" port <list ports> flowctrl <mode> - set flow control mode on specified port(s)\n");
//	printf(" port <list ports> bandwidth <arg> - set bandwidth on specified port(s)\n");
//	printf(" port <list ports> mirror <arg>    - mirroring port(s)\n");
//	printf(" port <list ports> learning <arg>  - enable/disable MAC-learning on port(s)\n");
//	printf(" mac-aging <arg>              - address lookup table control\n");
	printf(" ping                         - test if switch is responding\n");
	printf(" write memory                 - save current config to EEPROM\n");
	printf(" write defaults               - save to EEPROM chip-default values\n");
	printf(" mac-address <mac>            - set <mac> as new switch MAC address and reboots\n");
//      printf(" authkey <hex-value>          - set new authkey\n"); 
        return;
}

int main(int argc, char **argv){
    unsigned int x[6];
    unsigned int ak;
    int i;
    char *p;
    int root_port=-1;
    int media_speed;
    int direction; 
    int bandw;
    int shift=0;
    int negate=0;
    int cmd;
    unsigned short int *p_port_list=NULL;
    unsigned short int port_list[26];
    char *cmd_level_1[]={"show", "config", "scan", "reload", "reboot", "write", "ping", "mac-address",""}; 
    char *show_sub_cmd[]={"running-config","startup-config","interface",""};
    char *scan_sub_cmd[]={"verbose",""};
    char *show_sub_cmd_l2[]={"full","verbose",""};
    char *show_sub_cmd_l3[]={"summary",""};
    char *reset_sub_cmd[]={"soft","hard",""};
    char *write_sub_cmd[]={"memory","eeprom","defaults",""};

    if (argc<3){
        print_usage();
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
    }else if (strstr(p,"rtl83xx_ovislink_fsh2402gt")==argv[0]+strlen(argv[0])-26){
	switchtype=5;
    }else if (strstr(p,"rtl83xx_zyxel_es116p")==argv[0]+strlen(argv[0])-20){
	switchtype=6;
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

    if(strcmp(argv[2],"no")==0){
     negate++;
     shift++;
    }

    cmd=compare_command(argv[2+shift],&cmd_level_1[0]);
    //printf("%i\n",cmd);
    switch (cmd){
         case 0: //show
  	        if (argc == (2+shift+1)){
                   printf("No sub-command specified, allowed commands:\n");
                   print_allow_command(&show_sub_cmd[0]);
                   exit(1);
                }
                switch(compare_command(argv[3+shift],&show_sub_cmd[0])){
                   case 0: // running-config
   	                  if (argc == (3+shift+1)){
	           	     do_show_config(0);
                             exit(0);
                          }
                          switch (compare_command(argv[4+shift],&show_sub_cmd_l2[0])){
                                 case 0:
                                 case 1:
	                                do_show_config(1);
                                        exit(0);
                                 default:
                                        printf("Unknown sub-command: \"%s\", allowed commands:\n",argv[4+shift]);
                                        print_allow_command(&show_sub_cmd_l2[0]);
                                        exit(1);
	                  }
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
                          switch (compare_command(argv[4+shift],&show_sub_cmd_l3[0])){
                                 case 0:
	                                print_counters(p_port_list);
                                        exit(0);
                                 default:
                                        printf("Unknown sub-command: \"%s\", allowed commands:\n",argv[4+shift]);
                                        print_allow_command(&show_sub_cmd_l3[0]);
                                        exit(1);
	                  }
                          exit(0);
                   default:
                          printf("Unknown sub-command: \"%s\", allowed commands:\n",argv[3+shift]);
                          print_allow_command(&scan_sub_cmd[0]);
                          exit(1);
                }
         case 1: //config
                break;
         case 2: //scan
  	        if (argc == (2+shift+1)){
	           rtl83xx_scan(0);
                   exit(0);
                }
                switch (compare_command(argv[3+shift],&scan_sub_cmd[0])){
                   case 0:
	                  rtl83xx_scan(1);
                          exit(0);
                   default:
                          printf("Unknown sub-command: \"%s\", allowed commands:\n",argv[3+shift]);
                          print_allow_command(&scan_sub_cmd[0]);
                          exit(1);
	        }
         case 3: //reload
         case 4: //reboot
  	        if (argc == (2+shift+1)){
                   do_reboot();
                   exit(0);
                }
                switch (compare_command(argv[3+shift],&reset_sub_cmd[0])){
                   case 0:
	                  do_softreboot();
                          exit(0);
                   case 1:
	                  do_reboot();
                          exit(0);
                   default:
                          printf("Unknown sub-command: \"%s\", allowed commands:\n",argv[3+shift]);
                          print_allow_command(&reset_sub_cmd[0]);
                          exit(1);
	        }
         case 5: //write
  	        if (argc == (2+shift+1)){
                   printf("No sub-command specified, allowed commands:\n");
                   print_allow_command(&write_sub_cmd[0]);
                   exit(1);
                }
                switch(compare_command(argv[3+shift],&write_sub_cmd[0])){
                   case 0:
                   case 1:
                          do_write_memory();
                          exit(0);
                   case 2:
                          do_write_eeprom_defaults();
                          exit(0);
                   default:
                          printf("Unknown sub-command: \"%s\", allowed commands:\n",argv[3+shift]);
                          print_allow_command(&write_sub_cmd[0]);
                          exit(1);
                }
         case 6: //ping
                do_ping();
                exit(0);
         case 7: //mac-address
  	        if (argc == (2+shift+1)){
		   printf("not specified mac address\n");
                   exit(1);
                }
   	        if ((sscanf(argv[3+shift], "%02x:%02x:%02x:%02x:%02x:%02x",x,x+1,x+2,x+3,x+4,x+5)!=6)&&
	            (sscanf(argv[3+shift], "%02x%02x.%02x%02x.%02x%02x",x,x+1,x+2,x+3,x+4,x+5)!=6)){
   		   printf("malformed mac address: '%s'!\n",argv[3+shift]);
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
         default:
                print_usage();
    }
    exit(0);
}
