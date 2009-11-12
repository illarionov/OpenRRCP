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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <ctype.h>
#include <getopt.h>

#ifndef __linux__
#include <sys/socket.h>
#include <net/if.h>
#include <pcap.h>
#include <err.h>
#endif

#include <signal.h>
#include <unistd.h>
#include <setjmp.h>

#include "rrcp_packet.h"
#include "rrcp_io.h"
#include "rrcp_config.h"
#include "rrcp_switches.h"
#include "rrcp_lib.h"

#define ETH_TYPE_RRCP   0x8899          /* Realtek RRCP Protocol */
/*
#ifndef ETH_ADDR_LEN
#define ETH_ADDR_LEN 	6
#endif
#ifndef ETH_HDR_LEN
#define ETH_HDR_LEN 	14
#endif
*/

const char *progname = "check_rrcp";
const char *revision = "$Revision: 2.1 13/12/2007 $";

/*
 *
 * Standard Values for nagios plugins, exported from common.h
 *
 */

enum {
        OK = 0,
        ERROR = -1
};

/* AIX seems to have this defined somewhere else */
#ifndef FALSE
enum {
        FALSE,
        TRUE
};
#endif

enum {
        STATE_OK,
        STATE_WARNING,
        STATE_CRITICAL,
        STATE_UNKNOWN,
        STATE_DEPENDENT
};

enum {
        DEFAULT_SOCKET_TIMEOUT = 10,     /* timeout after 10 seconds */
        MAX_INPUT_BUFFER = 1024,             /* max size of most buffers we use */
        MAX_HOST_ADDRESS_LENGTH = 256    /* max size of a host address */
};

/*
 *
 * end of Standard Values for nagios plugins
 *
 */


int unsigned 			 debug;
struct timeval 			 send_time;
struct timeval 			 recv_time;
int unsigned			 got_alarm=0;
int unsigned 			 count,verbose;
char				 *status[]={"OK","WARNING","CRITICAL","UNKNOWN","DEPENDENT"};
jmp_buf 			 start_point;
unsigned short int 		port_list[26];
char 				 Temp[512];
extern char			 ErrIOMsg[];
#ifndef __linux__
extern pcap_t                   *handle;
#endif

void print_usage(void){
 fprintf(stdout, "Usage: %s [-P] [-t timeout] [-a authkey] [-I interface] -H MAC\n",progname);
 return;
}

void print_ver(void){
 fprintf(stdout,"%s %s\n",progname,revision); 
}

void print_help(void){
 print_ver();
 print_usage();
 fprintf(stdout,"\
-H, --hostname=MAC\n\
   MAC addr to check\n\
-I, --interface=IFACE\n\
   interface name\n\
-T, --switch_type=TYPE\n\
   switch type\n\
-t, --timeout=TIMEOUT\n\
   timeout for waiting answer, ms, default: 5000ms\n\
-a, --authkey=AUTHKEY\n\
   custom authkey, default: 0x2379\n\
-l, --check_loop\n\
   check loop detect enable & loop detected\n\
-p, --check_vlan\n\
   check vlan enabled\n\
-q, --check_1qvlan\n\
   check 1q vlan enabled\n\
-u, --check_up <ports list>\n\
    check this ports is UP, where <ports list> like 1,2,8-13\n\
-w, --warning=THRESHOLD\n\
   warning threshold pair (only for compatibility)\n\
-c, --critical=THRESHOLD\n\
   critical threshold pair (only for compatibility)\n\n\
This plugin uses the send \"Hello\" command on RRCP (Realtek Remote\
Control Protocol) to probe the specified host to alive and check some parameters\n");

 return;
}

void sAlrm(){
 signal(SIGALRM, SIG_DFL);
 signal(SIGINT, SIG_DFL);
 got_alarm++;
 longjmp(start_point,1);
}

void print_quit(int state,char *msg){
 printf("RRCP %s: %s\n",status[state],msg);
#ifndef __linux__
 if (handle != NULL) pcap_close(handle);
/* if (p_eth != NULL) eth_close(p_eth); */
// #else
// if (s_rec != -1) shutdown(s_rec,SHUT_RDWR);
// if (s_send != -1) shutdown(s_send,SHUT_RDWR);
#endif
 exit(state);
}

void tempstr_append(const char *fmt, ...)
{
   size_t len;
   va_list ap;

   len = strlen(Temp);
   if (len + 1 >= sizeof(Temp))
      return;
   va_start(ap, fmt);
   vsnprintf(Temp+len, sizeof(Temp)-len, fmt, ap);
   va_end(ap);

   return;
}

int main(int argc, char *argv[]){
 int i,c;
 int unsigned base,CheckVlan,Check1qVlan,CheckLoop,mac,EnLoopDet,LoopEnFault,CheckPortsUp,PortsGetFault,PortsDownDet,LoopGetFault,VlanGetFault;
 long unsigned t_out;
 uint32_t port_loop_status=0;
 uint32_t RegValue;
 uint32_t vlan_status;
 unsigned int LoopDet=0;
 ldiv_t difft;
 int exit_state=STATE_OK;
 int option = 0;
 union {
   uint16_t sh[13];
   uint8_t  ch[26];
 } r;
 static struct option longopts[] = {
    {"host", required_argument, 0, 'H'},
    {"timeout", required_argument, 0, 't'},
    {"interface", required_argument, 0, 'I'},
    {"authkey", required_argument, 0, 'a'},
    {"critical", required_argument, 0, 'c'},
    {"warning", required_argument, 0, 'w'},
    {"switch_type", required_argument, 0, 'T'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'},
    {"check_vlan",no_argument, 0, 'p'},
    {"check_1q",no_argument, 0, 'q'},
    {"check_loop",no_argument, 0, 'l'},
    {"check_up", required_argument, 0, 'u'},
    {0, 0, 0, 0}
 };

 debug=0;
 authkey=0x2379;
 t_out=1000*DEFAULT_SOCKET_TIMEOUT/2;
 count=0;
 mac=0;
 verbose=0;
 switchtype=0;
 VlanGetFault=CheckVlan=Check1qVlan=0;
 LoopGetFault=CheckLoop=LoopEnFault=EnLoopDet=0;
 CheckPortsUp=PortsGetFault=PortsDownDet=0;
 while ((c = getopt_long(argc, argv, "dPvVcwH:I:t:T:u:pqlh?",longopts,&option)) != -1) {
  switch (c) {
   case 'd':
             debug++;
             break;
   case 'v':
             verbose++;
             break;
   case 'c':
             break;
   case 'w':
             break;
   case 'V':
             print_ver();
             exit(STATE_UNKNOWN);
             break;
   case 'H':
	     if (parse_switch_id(optarg) >= 0) {
                mac++;
             }else if (sscanf(optarg, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
		      &dest_mac[0], &dest_mac[1], &dest_mac[2],
		      &dest_mac[3], &dest_mac[4], &dest_mac[5])==6){
                mac++;
             }else {
		printf("Invalid switch MAC address: %s\n", optarg);
		exit(STATE_UNKNOWN);
	     }
             break;
   case 'I':
             strncpy(ifname,optarg,sizeof(ifname)-1);
             break;
   case 'a':
             authkey = (int unsigned)strtoul(optarg,(char **)NULL, 16);
             break;
   case 't':
             base=10;
             if (strstr(optarg,"0x")==optarg) base=16;
             t_out = strtoul(optarg,(char **)NULL, base);
             break;
   case 'T':
	     switchtype = rrcp_get_switch_id_by_short_name(optarg);
	     if (switchtype < 0) {
		printf("invalid switch type %s\n",optarg);
		exit(STATE_UNKNOWN);
	     }
             break;
   case 'p':
             CheckVlan++;
             break;
   case 'q':
             Check1qVlan++;
             break;
   case 'l':
             CheckLoop++;
             break;
   case 'u':
             CheckPortsUp++;
             if (str_portlist_to_array(optarg,&port_list[0],switchtypes[switchtype].num_ports)!=0){
                  printf("invalid ports list %s\n",optarg);
                  exit(STATE_UNKNOWN);
             }
             break;
   default:
             print_help();
             exit(STATE_UNKNOWN);
             break;
  }
 }
 if (!mac) { print_usage();exit(STATE_UNKNOWN);}

 if (rtl83xx_prepare()){
  print_quit(STATE_UNKNOWN,ErrIOMsg);
 }
 // Interception of signals
 signal(SIGINT, sAlrm);
 signal(SIGALRM, sAlrm);
 ualarm(t_out*1000,0);
 // Storing of time of sending of a packet
 gettimeofday(&send_time,NULL);
 // Storing of a point of return
 setjmp(start_point);
 if (got_alarm){
   print_quit(STATE_CRITICAL,"Device not responding - timeout");
 }else{
   // run, Lola, run...
   if (debug) fprintf(stderr,"Get register 0x102...");
   if (rtl83xx_readreg32_(0x0102,&RegValue)){
     if (debug) fprintf(stderr,"device not responding\n");
     print_quit(STATE_CRITICAL,"Device not responding");
   }
   if (debug) fprintf(stderr,"done, value=%04x\n",RegValue);
   // Storing of time of receiving of a packet
   gettimeofday(&recv_time,NULL);
   count++;
   // get loop status
   if (CheckLoop){
    LoopDet=RegValue&0x4;
    if (debug) fprintf(stderr,"Get register 0x200...");
    if (rtl83xx_readreg32_(0x0200,&RegValue)){
      if (debug) fprintf(stderr,"device not responding\n");
      LoopEnFault++;
    }else{
     if (debug) fprintf(stderr,"done, value=%04x\n",RegValue);
     EnLoopDet=RegValue&0x4;
     count++;
     // Reception of expanded diagnostics if loop detected
     if (EnLoopDet && LoopDet) {
      if (debug) fprintf(stderr,"Get register 0x101...");
      if (rtl83xx_readreg32_(0x0101,&port_loop_status)){
         if (debug) fprintf(stderr,"device not responding\n");
         LoopGetFault++;
      }else{
       if (debug) fprintf(stderr,"done, value=%08x\n",port_loop_status);
       count++;
      }
     }
    }
   }
   // get VLAN status
   if (CheckVlan||Check1qVlan){
    if (debug) fprintf(stderr,"Get register 0x30b...");
    if (rtl83xx_readreg32_(0x030b,&vlan_status)){
     if (debug) fprintf(stderr,"device not responding\n");
     VlanGetFault++;
    }else{
     if (debug) fprintf(stderr,"done, value=%04x\n",vlan_status);
     count++;
    }
   }
   // get ports status
   if (CheckPortsUp){
     for(i=0;i<switchtypes[switchtype].num_ports/2;i++){
       if (debug) fprintf(stderr,"Get register 0x%04x...",0x0619+i);
       if (rtl83xx_readreg32_(0x0619+i,&RegValue)){
         if (debug) fprintf(stderr,"device not responding\n");
         PortsGetFault++;
         break;
       }
       r.sh[i]=(uint16_t)RegValue;
       if (debug) fprintf(stderr,"done, value=%04x\n",RegValue);
       count++;
     }
   }
   signal(SIGALRM,SIG_DFL);
   signal(SIGINT,SIG_DFL);
 }
 if (debug) fprintf(stderr,"Got %u packets\n",count);
 // Calculation of a delay of the answer of the switch
 difft=ldiv(((recv_time.tv_sec-send_time.tv_sec)*1000000+recv_time.tv_usec)-send_time.tv_usec,1000);
 // Print final result...
 Temp[0]='\0';
 tempstr_append("Reply from %02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX, time %li.%03li ms",
         dest_mac[0],dest_mac[1],dest_mac[2],
	 dest_mac[3],dest_mac[4],dest_mac[5],
	 difft.quot,difft.rem);
 // processing loop detect status
 if (CheckLoop){
  if (LoopEnFault){
   if (exit_state < STATE_WARNING) exit_state=STATE_WARNING;
   tempstr_append(", can not get loop enable/disable value");
  }else{
   if (EnLoopDet){
    if (LoopDet) { // ...loop detected
     exit_state=STATE_WARNING;
     if (LoopGetFault){
	tempstr_append(", can not get loop status");
     }else{
      unsigned is_first_port = 1;
      for(i=1;i<=switchtypes[switchtype].num_ports;i++){
       if ( (port_loop_status>>(map_port_number_from_logical_to_physical(i)))&0x1 ) {
	tempstr_append(is_first_port ? ", loop on port(s): %u" : ",%u",i);
	is_first_port = 0;
       }
      }
     }
    }
   }else{
     if (exit_state < STATE_WARNING) exit_state=STATE_WARNING;
     tempstr_append(", loop detect disable");
   }
  }
 }
/*
 if (RegValue&0x2){ // ...trunk fault
   if (exit_state < STATE_WARNING) exit_state=STATE_WARNING;
   tempstr_append(", fault trunk group detected");
 }
*/
 // processing vlan status
 if (CheckVlan||Check1qVlan){
  if (VlanGetFault){
    if (exit_state < STATE_WARNING) exit_state=STATE_WARNING;
    tempstr_append(", can not get VLAN status");
  }else{
    if (!(vlan_status&0x1)){ // ...VLAN disable
      if (exit_state < STATE_WARNING) exit_state=STATE_WARNING;
      tempstr_append(", VLAN disable");
    }else{
      if (Check1qVlan){ // ... dot1qVLAN disable
       if (!(vlan_status&0x10)){
        if (exit_state < STATE_WARNING) exit_state=STATE_WARNING;
        tempstr_append(", .1q_VLAN disable");
       }
      }
    }
  }
 }
 // processing ports status
 if (CheckPortsUp){
  if (PortsGetFault){
    if (exit_state < STATE_WARNING) exit_state=STATE_WARNING;
    tempstr_append(", can not get ports status");
  }else{
   for(i=1;i<=switchtypes[switchtype].num_ports;i++){
    if (port_list[i-1]){
     if (r.ch[map_port_number_from_logical_to_physical(i)]&0x10){
      port_list[i-1]=0;
     }else{
       PortsDownDet++;
     }
    }
   }
   if (PortsDownDet){
    if (exit_state < STATE_WARNING) exit_state=STATE_WARNING;
    unsigned is_first_port = 1;
    for(i=1;i<=switchtypes[switchtype].num_ports;i++){
     if (port_list[i-1]) {
      tempstr_append(is_first_port ? ", port(s) down: %u" : ",%u",i);
      is_first_port = 0;
     }
    }
   }
  }
 }
 print_quit(exit_state,Temp);
 exit(6);
}

