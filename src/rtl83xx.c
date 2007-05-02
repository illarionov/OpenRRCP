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
#include "rrcp_lib.h"
#include "rrcp_config.h"
#include "rrcp_switches.h"
#include "../lib/fake-libcli.h"
#include "rrcp_cli_cmd_show.h"
#include "rrcp_cli_cmd_config.h"
#include "rrcp_cli_cmd_config_int.h"
#include "rrcp_cli_cmd_other.h"
#include "rrcp_cli.h"

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

void print_link_status(void){
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
    printf("VLAN Status reg=0x%04x VLAN enabled: %s, 802.1Q enabled: %s\n",
	    vlan_status & 0x7f,
	    (vlan_status & 1<<0) ? "yes" : "no",
	    (vlan_status & 1<<4) ? "yes" : "no");
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
   if (!swconfig.vlan.s.config.enable) { printf("WARNING: vlan mode not enable\n"); } 
   do_vlan_enable_vlan(swconfig.vlan.s.config.dot1q,1);
 }
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

void cmdcat(char *cmd_str,int size,char *cmd){
 if (strlen(cmd_str)) strncat(cmd_str," ",size);
 strncat(cmd_str,cmd,size);
}

void print_unknown(char *argv, char **command_list){
  printf("Unknown sub-command: \"%s\", allowed commands:\n",argv);
  print_allow_command(command_list);
  exit(1);
}

void print_usage(void){
	printf("Usage: rtl8316b [[authkey-]xx:xx:xx:xx:xx:xx@]if-name <command> [<argument>]\n");
	printf("       rtl8326 ----\"\"----\n");
	printf("       rtl83xx_dlink_des1016d ----\"\"----\n");
	printf("       rtl83xx_dlink_des1024d_c1 ----\"\"----\n");
	printf("       rtl83xx_dlink_des1024d_d1 ----\"\"----\n");
	printf("       rtl83xx_compex_ps2216 ----\"\"----\n");
        printf("       rtl83xx_ovislink_fsh2402gt ----\"\"----\n");
	printf("       rtl83xx_zyxel_es116p ----\"\"----\n");
	printf(" where command may be:\n");
	printf(" show running-config          - show current switch config\n");
	printf(" show interface [<list ports>]- print link status for ports\n");
	printf(" show interface [<list ports>] summary - print port rx/tx counters\n");
	printf(" show vlan [vid <id>]         - show low-level vlan confg\n");
	printf(" scan [verbose]               - scan network for rrcp-enabled switches\n");
	printf(" reboot [soft|hard]           - initiate switch reboot\n");
	printf(" config interface [<list ports>] ... - port(s) configuration\n");
	printf(" --\"\"-- [no] shutdown - enable/disable specified port(s)\n");
	printf(" --\"\"-- speed <arg> [duplex <arg>] - set speed/duplex on specified port(s)\n");
	printf(" --\"\"-- flow-control <arg>  - set flow-control on specified port(s)\n");
	printf(" --\"\"-- rate-limit [input|output] <arg>  - set bandwidth on specified port(s)\n");
	printf(" --\"\"-- mac-address learning enable|disable - enable/disable MAC-learning on port(s)\n");
	printf(" --\"\"-- rrcp enable|disable - enable/disable rrcp on specified ports\n");
	printf(" --\"\"-- mls qos cos 0|7 - set port priority\n");
	printf(" config rrcp enable|disable - global rrcp enable|disable\n");
	printf(" config rrcp echo enable|disable - rrcp echo (REP) enable|disable\n");
	printf(" config rrcp loop-detect enable|disable - network loop detect enable|disable\n"); 
        printf(" config rrcp authkey <hex-value> - set new authkey\n"); 
	printf(" config vlan disable - disable all VLAN support\n");
	printf(" config vlan mode portbased|dot1q - enable specified VLAN support\n");
	printf(" config vlan template-load portbased|dot1qtree - load specified template\n");
	printf(" config vlan clear - clear vlan table (all ports bind to one vlan)\n");
	printf(" config mac-address <mac> - set <mac> as new switch MAC address and reboots\n");
	printf(" config mac-address-table aging-time|drop-unknown <arg>  - address lookup table control\n");
        printf(" config flowcontrol dot3x enable|disable - globally disable full duplex flow control (802.3x pause)\n");
        printf(" config flowcontrol backpressure enable|disable - globally disable/enable half duplex back pressure ability\n");
        printf(" config flowcontrol ondemand-disable enable|disable - disable/enable flow control ability auto turn off for QoS\n");
	printf(" config [no] storm-control broadcast relaxed|strict - broadcast storm control\n"); 
	printf(" config [no] storm-control multicast - multicast storm control\n"); 
        printf(" config [no] monitor [input|output] source interface <list ports> destination interface <port>\n");
	printf(" ping                         - test if switch is responding\n");
	printf(" write memory                 - save current config to EEPROM\n");
	printf(" write defaults               - save to EEPROM chip-default values\n");
        return;
}

int main(int argc, char **argv){
    struct cli_def *cli;
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
    int subconfcmd;
    int vid=-1;
    int duplex=0;
    int negate=0;
    unsigned short int *p_port_list=NULL;
    unsigned short int port_list[26];
    char qs[]="?";
    char *internal_argv[32];
    char temp_str[128];
    char cmd_to_cli[128];
    char temp_cmd_to_cli[128];
    char *ena_disa[]={"disable","enable",""};
    char *ena_only[]={"enable",""};
    char *cmd_level_1[]={"show","config","scan","reload","reboot","write","ping","copy",""}; 
    char *show_sub_cmd[]={"running-config","startup-config","interface","vlan","version","switch-register","eeprom-register","phy-register","ip",""};
    char *scan_sub_cmd[]={"verbose",""};
    char *show_sub_cmd_l2[]={"full","verbose",""};
    char *show_sub_cmd_l3[]={"summary",""};
    char *show_sub_cmd_l4[]={"id",""};
    char *show_sub_cmd_l5[]={"igmp","snooping","mrouter",""};
    char *reset_sub_cmd[]={"soft","hard",""};
    char *write_sub_cmd[]={"memory","eeprom","defaults","terminal",""};
    char *copy_sub_cmd[]={"running-config","eeprom",""};
    char *config_sub_cmd_l1[]={"interface","rrcp","vlan","mac-address","mac-address-table","flowcontrol","storm-control","monitor","vendor-id","ip","spanning-tree","mls","wrr-queue",""};
    char *config_intf_sub_cmd_l1[]={"no","shutdown","speed","duplex","rate-limit","mac-address","rrcp","mls","flow-control",""};
    char *config_duplex[]={"half","full",""};
    char *config_rate[]={"100m","128k","256k","512k","1m","2m","4m","8m","input","output",""};
    char *config_mac_learn[]={"learning","disable","enable",""};
    char *config_port_qos[]={"7","0","qos","cos",""};
    char *config_port_flow[]={"none","asym2remote","symmetric","asym2local",""};
    char *config_rrcp[]={"enable","echo","loop-detect","authkey",""};
    char *config_alt[]={"aging-time","unknown-destination",""};
    char *config_alt_dest[]={"drop","pass",""};
    char *config_vlan[]={"disable","transparent","clear","mode","template-load","leaky","drop",""};
    char *config_vlan_mode[]={"portbased","dot1q",""};
    char *config_vlan_tmpl[]={"portbased","dot1qtree",""};
    char *config_vlan_leaky[]={"arp","multicast","unicast",""};
    char *config_vlan_drop[]={"untagged_frames","invalid_vid",""};
    char *config_flowc[]={"dot3x","backpressure","ondemand-disable",""};
    char *config_storm[]={"broadcast","multicast",""};
    char *config_storm_br[]={"relaxed","strict",""};
    char *config_ip_igmp[]={"igmp","snooping",""};
    char *config_span_tree[]={"bpdufilter",""};
    char *config_mls[]={"qos","trust","cos","dscp",""};
    char *config_wrr[]={"ratio",""};

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
    }else if (strstr(p,"rtl8318")==argv[0]+strlen(argv[0])-8){
	switchtype=2;
    }else if (strstr(p,"rtl8324")==argv[0]+strlen(argv[0])-7){
	switchtype=3;
    }else if (strstr(p,"rtl83xx_dlink_des1016d")==argv[0]+strlen(argv[0])-22){
	switchtype=4;
    }else if (strstr(p,"rtl83xx_dlink_des1024d_b1")==argv[0]+strlen(argv[0])-25){
	switchtype=5;
    }else if (strstr(p,"rtl83xx_dlink_des1024d_c1")==argv[0]+strlen(argv[0])-25){
	switchtype=6;
    }else if (strstr(p,"rtl83xx_compex_ps2216")==argv[0]+strlen(argv[0])-21){
	switchtype=7;
    }else if (strstr(p,"rtl83xx_ovislink_fsh2402gt")==argv[0]+strlen(argv[0])-26){
	switchtype=8;
    }else if (strstr(p,"rtl83xx_zyxel_es116p")==argv[0]+strlen(argv[0])-20){ 
        switchtype=9; 
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
            swconfig.mac_address[i]=(unsigned char)x[i];
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

    engage_timeout(10);
    rtl83xx_prepare();

    cli=cli_init();
    cli->client=stdout;
    bzero(cmd_to_cli,sizeof(cmd_to_cli));

    cmd=compare_command(argv[2+shift],&cmd_level_1[0]);
    switch (cmd){
         case 0: //show
                cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"show");
                check_argc(argc,2+shift,NULL,&show_sub_cmd[0]);
                subcmd=compare_command(argv[3+shift],&show_sub_cmd[0]);
                cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,show_sub_cmd[subcmd]);
                switch(subcmd){
                   case 0: // running-config
   	                  if (argc == (4+shift)){
	           	     cmd_show_config(cli,cmd_to_cli, NULL, 0);
                             exit(0);
                          }
                          (void) get_cmd_num(argv[4+shift],-1,NULL,&show_sub_cmd_l2[0]);
                           cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,show_sub_cmd_l2[0]);
                           cmd_show_config(cli,cmd_to_cli,&argv[4+shift], argc-shift-4);
                          exit(0);
                   case 1: // startup-config
                          printf("Under construction\n");
                          exit(0);
                   case 2: // interfaces
   	                  if (argc == (4+shift)){
                             print_link_status();
                             exit(0);
                          }
                          if (str_portlist_to_array(argv[4+shift],&port_list[0],switchtypes[switchtype].num_ports)==0){
                            p_port_list=&port_list[0];
   	                    if (argc == (5+shift)){
                               for(i=0;i<switchtypes[switchtype].num_ports;i++){
                                 if (!port_list[i]) continue;
                                 bzero(temp_str,sizeof(temp_str));
                                 bzero(temp_cmd_to_cli,sizeof(temp_cmd_to_cli));
                                 snprintf(temp_str,sizeof(temp_str)-1,"%u",i+1);
                                 snprintf(temp_cmd_to_cli,sizeof(temp_cmd_to_cli)-1,"%s %u",cmd_to_cli,i+1);
                                 internal_argv[0]=temp_str;
                                 cmd_show_interfaces(cli,temp_cmd_to_cli,&internal_argv[0],1);
                               }
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
                   case 4: // version
                          rrcp_autodetect_switch_chip_eeprom(&swconfig.switch_type, &swconfig.chip_type, &swconfig.eeprom_type);
                          rrcp_io_probe_switch_for_facing_switch_port(swconfig.mac_address,&swconfig.facing_switch_port_phys);
                          cmd_show_version(cli, cmd_to_cli, &argv[4+shift], argc-shift-4);
                          exit(0);
                   case 5: // switch-register
   	                  if (argc == (4+shift)){
                            internal_argv[0]=qs;
                            exit(cmd_show_switch_register(cli,cmd_to_cli,&internal_argv[0],1));
                          }else{
                            cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,argv[4+shift]);
                            exit(cmd_show_switch_register(cli,cmd_to_cli,&argv[4+shift],argc-shift-4));
                          }
                   case 6: // eeprom-register
   	                  if (argc == (4+shift)){
                            internal_argv[0]=qs;
                            exit(cmd_show_eeprom_register(cli,cmd_to_cli,&internal_argv[0],1));
                          }else{
                            cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,argv[4+shift]);
                            exit(cmd_show_eeprom_register(cli,cmd_to_cli,&argv[4+shift],argc-shift-4));
                          }
                   case 7: // phy-register
   	                  if (argc == (4+shift)){
                            internal_argv[0]=qs;
                            exit(cmd_show_phy_register(cli,cmd_to_cli,&internal_argv[0],1));
                          }else{
                            cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,argv[4+shift]);
                            exit(cmd_show_phy_register(cli,cmd_to_cli,&argv[4+shift],argc-shift-4));
                          }
                   case 8: // ip igmp snooping
                          for(;;){
                            check_argc(argc,3+shift,"No sub-command, allowed commands: igmp snooping [mrouter]\n",NULL);
                            subcmd=get_cmd_num(argv[4+shift],-1,"Incorrect sub-commands, allowed: igmp snooping [mrouter]\n",&show_sub_cmd_l5[0]);
                            cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,show_sub_cmd_l5[subcmd]);
//                            printf("argc=%i, argv[%i]=%s, command=%s\n",argc-shift-4,4+shift,argv[4+shift],cmd_to_cli);
                            shift++;
                            if (argc == (4+shift)){ 
                             if (subcmd) break;
                             else{ 
                               printf("No sub-command, allowed commands: igmp snooping [mrouter]\n");
                               exit(1);
                             }
                            }
                          }
                          exit(cmd_show_ip_igmp_snooping(cli,cmd_to_cli,NULL,0));

                   default: 
                          print_unknown(argv[3+shift],&show_sub_cmd[0]);
                }
         case 1: //config
                check_argc(argc,2+shift,NULL,&config_sub_cmd_l1[0]);
                if (strcmp(argv[3+shift],"no")==0){ 
                  shift++; negate++;
                  cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"no"); 
                  check_argc(argc,2+shift,NULL,&config_sub_cmd_l1[0]);
                }
                subconfcmd=compare_command(argv[3+shift],&config_sub_cmd_l1[0]);
                cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_sub_cmd_l1[subconfcmd]); 
                switch (subconfcmd){
                   case 0: // interface
                          check_argc(argc,3+shift,"No list of ports\n",NULL);
                          if (str_portlist_to_array(argv[4+shift],&port_list[0],switchtypes[switchtype].num_ports)!=0){
                             printf("Incorrect list of ports: \"%s\"\n",argv[4+shift]);
                             exit(1);
                          }
                          check_argc(argc,4+shift,NULL,&config_intf_sub_cmd_l1[0]);
                          bzero(temp_cmd_to_cli,sizeof(temp_cmd_to_cli));
                          bzero(cmd_to_cli,sizeof(cmd_to_cli));
                          switch (compare_command(argv[5+shift],&config_intf_sub_cmd_l1[0])){
                                 case 0: // no
                                        check_argc(argc,5+shift,"No sub-command, allowed commands: shutdown\n",NULL);
                                        (void) get_cmd_num(argv[6+shift],1,"Incorrect sub-commands, allowed: shutdown\n",&config_intf_sub_cmd_l1[0]);
                                        cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"no");
                                 case 1: // shutdown
                                        cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"shutdown");
                                        for(i=0;i<switchtypes[switchtype].num_ports;i++){
                                          if (!port_list[i]) continue;
                                          bzero(temp_str,10);
                                          snprintf(temp_str,sizeof(temp_str)-1,"/%u",i+1);
                                          cli->modestring=temp_str;
                                          (void)cmd_config_int_shutdown(cli,cmd_to_cli,NULL,0);
                                        }
                                        exit(0);
                                 case 2: // speed
                                        check_argc(argc,5+shift,"No sub-command, allowed commands: 10|100|1000|auto [duplex half|full]\n",NULL);
                                        if ((media_speed=speed_encode(argv[6+shift])) == -1){
                                          printf("Incorect speed, valid are: 10|100|1000|auto\n");
                                          exit(1);
                                        }
                                        cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"speed");
                                        switch(media_speed){
                                              case 0x1:
                                                       cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"10");
                                                       break;
                                              case 0x4:
                                                       cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"100");
                                                       break;
                                              case 0x10:
                                                       cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"1000");
                                                       break;
                                              default:
                                                       cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"auto");
                                        }
                                        for(i=0;i<switchtypes[switchtype].num_ports;i++){
                                          if (!port_list[i]) continue;
                                          bzero(temp_str,10);
                                          snprintf(temp_str,sizeof(temp_str)-1,"/%u",i+1);
                                          cli->modestring=temp_str;
                                          (void)cmd_config_int_speed_duplex(cli,cmd_to_cli,NULL,0);
					}
                                        if ( (argc > 6+shift+1) && (media_speed!=0x8) ){ //duplex
                                          (void)get_cmd_num(argv[7+shift],3,"Incorrect sub-command, allowed commands: duplex half|full\n",&config_intf_sub_cmd_l1[0]);
                                          check_argc(argc,7+shift,NULL,&config_duplex[0]);
                                          duplex=get_cmd_num(argv[8+shift],-1,NULL,&config_duplex[0]);
                                          bzero(cmd_to_cli,sizeof(cmd_to_cli));
                                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"duplex");
                                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_duplex[duplex]);
                                          for(i=0;i<switchtypes[switchtype].num_ports;i++){
                                            if (!port_list[i]) continue;
                                            bzero(temp_str,10);
                                            snprintf(temp_str,sizeof(temp_str)-1,"/%u",i+1);
                                            cli->modestring=temp_str;
                                            (void)cmd_config_int_speed_duplex(cli,cmd_to_cli,NULL,0);
					  }
                                        }
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
                                           if (direction==1) cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"rate-limit input");
                                           else cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"rate-limit output");
                                           for(i=0;i<switchtypes[switchtype].num_ports;i++){
                                            if (!port_list[i]) continue;
                                             bzero(temp_str,10);
                                             snprintf(temp_str,sizeof(temp_str)-1,"/%u",i+1);
                                             cli->modestring=temp_str;
                                             (void)cmd_rate_limit(cli,cmd_to_cli,&argv[6+shift],argc-6-shift);
					    }
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
                                           if (subcmd==2) cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"mac-learn enable");
                                           else cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"mac-learn disable");
                                           for(i=0;i<switchtypes[switchtype].num_ports;i++){
                                            if (!port_list[i]) continue;
                                             bzero(temp_str,10);
                                             snprintf(temp_str,sizeof(temp_str)-1,"/%u",i+1);
                                             cli->modestring=temp_str;
                                             (void)cmd_config_int_mac_learning(cli,cmd_to_cli,NULL,0);
					    }
                                           exit(0);
                                        }
                                 case 6:  // rrcp
                                        check_argc(argc,5+shift,"No sub-command, allowed commands: enable|disable\n",NULL);
                                        subcmd=get_cmd_num(argv[6+shift],-1,"Incorrect sub-commands, allowed: enable|disable\n",&ena_disa[0]);
                                        if (subcmd) cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"rrcp enable");
                                        else cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"no rrcp enable");
                                        for(i=0;i<switchtypes[switchtype].num_ports;i++){
                                         if (!port_list[i]) continue;
                                         bzero(temp_str,10);
                                         snprintf(temp_str,sizeof(temp_str)-1,"/%u",i+1);
                                         cli->modestring=temp_str;
                                         (void)cmd_config_int_rrcp(cli,cmd_to_cli,NULL,0);
					}
                                        exit(0);
                                 case 7: // mls qos
                                        for(;;){
                                           check_argc(argc,5+shift,"No sub-command, allowed commands: qos cos 0|7\n",NULL);
                                           subcmd=get_cmd_num(argv[6+shift],-1,"Incorrect sub-commands, allowed: qos cos 0|7\n",&config_port_qos[0]);
                                           if (subcmd > 1){
                                             shift++;
                                             continue;
                                           }
                                           cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,"mls cos qos");
                                           for(i=0;i<switchtypes[switchtype].num_ports;i++){
                                             if (!port_list[i]) continue;
                                             bzero(temp_str,10);
                                             snprintf(temp_str,sizeof(temp_str)-1,"/%u",i+1);
                                             cli->modestring=temp_str;
                                             (void)cmd_config_int_mls(cli,cmd_to_cli,&argv[6+shift],argc-6-shift);
                                           }
                                           exit(0);
                                        }
                                 case 8:  // flow control
                                        check_argc(argc,5+shift,NULL,&config_port_flow[0]);
                                        subcmd=get_cmd_num(argv[6+shift],-1,NULL,&config_port_flow[0]);
                                        if (subcmd) subcmd=subcmd<<5;
                                        printf("under construction\n");
                                        exit(0);
                                 default:
                                         print_unknown(argv[5+shift],&config_intf_sub_cmd_l1[0]);
                          }
                   case 1: // rrcp
                          check_argc(argc,3+shift,NULL,&config_rrcp[0]);
                          subcmd=compare_command(argv[4+shift],&config_rrcp[0]);
                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_rrcp[subcmd]);
                          switch (subcmd){
                                 case 1: // rrcp echo
                                 case 2: // rrcp loopdetect
                                        check_argc(argc,4+shift,"No sub-command, allowed commands: enable\n",NULL);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&ena_only[0]);
                                        cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,ena_only[subcmd]);
                                 case 0: // rrcp enable
                                        exit(cmd_config_rrcp(cli,cmd_to_cli,NULL,0));
                                 case 3: // rrcp authkey
                                        exit(cmd_config_rrcp_authkey(cli,cmd_to_cli,&argv[5+shift],argc-shift-5));
                                 default:
                                         print_unknown(argv[4+shift],&config_rrcp[0]);
                          }
                   case 2: // vlan
                          check_argc(argc,3+shift,NULL,&config_vlan[0]);
                          subcmd=compare_command(argv[4+shift],&config_vlan[0]);
                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_vlan[subcmd]);
                          switch (subcmd){
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
                                 case 5: // leaky
                                        check_argc(argc,4+shift,NULL,&config_vlan_leaky[0]);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&config_vlan_leaky[0]);
                                        cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_vlan_leaky[subcmd]);
                                        exit(cmd_config_vlan_leaky(cli,cmd_to_cli,NULL,0));
                                 case 6: // drop
                                        check_argc(argc,4+shift,NULL,&config_vlan_drop[0]);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&config_vlan_drop[0]);
                                        cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_vlan_drop[subcmd]);
                                        exit(cmd_config_vlan_drop(cli,cmd_to_cli,NULL,0));
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
                          subcmd=compare_command(argv[4+shift],&config_alt[0]);
                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_alt[subcmd]);
                          switch (subcmd){
                                 case 0: // aging-time
               	                        if (argc == (5+shift)){
                                          internal_argv[0]=qs;
                                          exit(cmd_config_mac_aging(cli,cmd_to_cli,&internal_argv[0],1));
                                        }else{
                                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,argv[5+shift]);
                                          exit(cmd_config_mac_aging(cli,cmd_to_cli,&argv[5+shift],argc-5-shift));
                                        }
                                 case 1: // unknown-destination
                                        check_argc(argc,4+shift,NULL,&config_alt_dest[0]);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&config_alt_dest[0]);
                                        printf ("Under construction\n");
                                        exit(0);
                                 default: 
                                         print_unknown(argv[4+shift],&config_alt[0]);
                          }
                   case 5: // flowcontrol
                          check_argc(argc,3+shift,NULL,&config_flowc[0]);
                          subcmd=get_cmd_num(argv[4+shift],-1,NULL,&config_flowc[0]);
                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_flowc[subcmd]);
                          exit(cmd_config_flowcontrol(cli,cmd_to_cli,NULL,0));
                   case 6: // storm-control
                          check_argc(argc,3+shift,NULL,&config_storm[0]);
                          subcmd=get_cmd_num(argv[4+shift],-1,NULL,&config_storm[0]);
                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_storm[subcmd]);
                          switch (subcmd){
                                 case 0: // broadcast
                                        if (!negate){
                                          check_argc(argc,4+shift,NULL,&config_storm_br[0]);
                                          subcmd=get_cmd_num(argv[5+shift],-1,NULL,&config_storm_br[0]);
                                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_storm_br[subcmd]);
                                        }
                                 case 1: // multicast
                                        exit(cmd_config_stormcontrol(cli,cmd_to_cli,NULL,0));
                                 default: 
                                         print_unknown(argv[4+shift],&config_storm[0]);
                          }
                   case 7: // monitor
                          if (switchtypes[switchtype].chip_id==rtl8326){
                            printf("Port mirroring not working with hardware based on rtl8326/rtl8326s\n");
                            exit(1);
                          } 
                          printf("Under construction\n");
                          exit(0);
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
                   case 9: // ip igmp snooping
                          for(;;){
                            check_argc(argc,3+shift,"No sub-command, allowed commands: [no] igmp snooping \n",NULL);
                            subcmd=get_cmd_num(argv[4+shift],-1,"Incorrect sub-commands, allowed: [no] igmp snooping\n",&show_sub_cmd_l5[0]);
                            cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_ip_igmp[subcmd]);
//                            printf("argc=%i, argv[%i]=%s, command=%s\n",argc-shift-4,4+shift,argv[4+shift],cmd_to_cli);
                            shift++;
                            if (argc == (4+shift)){ 
                             if (subcmd) break;
                             else{ 
                               printf("No sub-command, allowed commands: [no] igmp snooping\n");
                               exit(1);
                             }
                            }
                          }
                          exit(cmd_config_ip_igmp_snooping(cli,cmd_to_cli,NULL,0));
                   case 10: // spanning-tree
                          check_argc(argc,3+shift,NULL,&config_span_tree[0]);
                          subcmd=get_cmd_num(argv[4+shift],-1,NULL,&config_span_tree[0]);
                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_span_tree[subcmd]);
                          check_argc(argc,4+shift,"No sub-command, allowed commands: enable\n",NULL);
                          subcmd=get_cmd_num(argv[5+shift],-1,NULL,&ena_only[0]);
                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,ena_only[subcmd]);
                          exit(cmd_config_spanning_tree(cli,cmd_to_cli,NULL,0));
                   case 11: // mls 
                          for(;;){
                            check_argc(argc,3+shift,"No sub-command, allowed commands: [no] qos trust cos|dscp\n",NULL);
                            subcmd=get_cmd_num(argv[4+shift],-1,"Incorrect sub-commands, allowed: [no] qos trust cos|dscp\n",&config_mls[0]);
                            cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_mls[subcmd]);
                            printf("argc=%i, argv[%i]=%s, command=%s\n",argc-shift-4,4+shift,argv[4+shift],cmd_to_cli);
                            shift++;
                            if (argc == (4+shift)){ 
                             if (subcmd>1) break;
                             else{ 
                               printf("No sub-command, allowed commands: [no] qos trust cos|dscp\n");
                               exit(1);
                             }
                            }
                          }
                          exit(cmd_config_qos(cli,cmd_to_cli,NULL,0));
                   case 12: // wrr-queue
                          check_argc(argc,3+shift,NULL,&config_wrr[0]);
                          subcmd=get_cmd_num(argv[4+shift],-1,NULL,&config_wrr[0]);
                          cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,config_wrr[subcmd]);
                          if (negate) exit(cmd_config_qos(cli,cmd_to_cli,NULL,0));
                          else{
                           if (argc == (5+shift)){
                            internal_argv[0]=qs;
                            exit(cmd_config_qos_wrr_queue_ratio(cli,cmd_to_cli,&internal_argv[0],1));
                           }else{
                            cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,argv[5+shift]);
                            exit(cmd_config_qos_wrr_queue_ratio(cli,cmd_to_cli,&argv[5+shift],argc-5-shift));
                           }
                          }
                   default:
                          print_unknown(argv[3+shift],&config_sub_cmd_l1[0]);
                }
                break;
         case 2: //scan
  	        if (argc == (2+shift+1)){
	           rtl83xx_scan(0);
                   exit(0);
                }
                (void)get_cmd_num(argv[3+shift],0,NULL,&scan_sub_cmd[0]);
	        rtl83xx_scan(1);
                exit(0);
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
                subcmd=compare_command(argv[3+shift],&write_sub_cmd[0]);
                cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,write_sub_cmd[subcmd]);
                switch(subcmd){
                   case 0:
                   case 1:
                          cmd_write_memory(cli,cmd_to_cli,NULL,0);
                          exit(0);
                   case 2:
                          printf("Under constrution\n");
                          exit(0);
                   case 3:
                          cmd_write_terminal(cli,cmd_to_cli,NULL,0);
                          exit(0);
                   default:
                          print_unknown(argv[3+shift],&write_sub_cmd[0]);
                }
         case 6: //ping
                do_ping();
                exit(0);
         case 7: //copy
                check_argc(argc,2+shift,NULL,&copy_sub_cmd[0]);
                subcmd=compare_command(argv[3+shift],&copy_sub_cmd[0]);
                cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,copy_sub_cmd[subcmd]);
                switch(subcmd){
                   case 0:
                          if (argc == (4+shift)){
                            internal_argv[0]=qs;
                            exit(cmd_copy_running_config(cli,cmd_to_cli,&internal_argv[0],1));
                          }else{
                            cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,argv[4+shift]);
                            exit(cmd_copy_running_config(cli,cmd_to_cli,&argv[4+shift],argc-shift-4));
                          }
                   case 1:
                          cli->client=stdout;
                          if (argc == (4+shift)){
                            internal_argv[0]=qs;
                            exit(cmd_copy_eeprom(cli,cmd_to_cli,&internal_argv[0],1));
                          }else{
                            cmdcat(cmd_to_cli,sizeof(cmd_to_cli)-1,argv[4+shift]);
                            exit(cmd_copy_eeprom(cli,cmd_to_cli,&argv[4+shift],argc-shift-4));
                          }
                   default:
                          print_unknown(argv[3+shift],&copy_sub_cmd[0]);
                }
         default:
                print_usage();
    }
    exit(0);
}
