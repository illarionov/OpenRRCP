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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#ifdef __linux__
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#else
#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>
#include <pcap.h>
#include <err.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef htonl
#include <arpa/inet.h>
#endif

#include <errno.h>
#include "rrcp_packet.h"
#include "rrcp_io.h"
#include "rrcp_config.h"
#include "rrcp_switches.h"

extern uint32_t *register_mask[];
char ifname[128] = "";
uint16_t authkey = 0x2379;
unsigned char my_mac[6] = {0x00, 0x00, 0x11, 0x22, 0x33, 0x44};
unsigned char dest_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned char mac_bcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

unsigned int switchtype;
int s,s_rec,s_send;

#ifdef __linux__
struct sockaddr_ll sockaddr_rec,sockaddr_send;
#else
pcap_t                   *handle;
const u_char		 *rcvbuf;
char errbuf[PCAP_ERRBUF_SIZE];
struct bpf_program filter;
char filter_app[512];
#endif
char ErrIOMsg[512];

// takes logical port number as printed on switch' case (1,2,3...)
// returns physical port number (0,1,2...) or -1 if this device has no such logical port
int map_port_number_from_logical_to_physical(int port){
    return switchtypes[switchtype].port_order[port-1]-1;
}

int map_port_number_from_physical_to_logical(int port){
    int i;
    for (i=1;i<=switchtypes[switchtype].num_ports;i++){
	if(port==(switchtypes[switchtype].port_order[i-1]-1)){
	    return i;
	}
    }
    return -1;
}

#ifdef __linux__
int rtl83xx_prepare(){
    struct ifreq ifr;
    struct timeval time;
    struct timezone timez;

    timez.tz_minuteswest=0;
    timez.tz_dsttime=0;
    gettimeofday(&time, &timez);
    srand(time.tv_sec+time.tv_usec);
    
    memset(&ErrIOMsg, 0x00, sizeof(ErrIOMsg)); 
    s_rec = socket(PF_PACKET, SOCK_RAW, htons(0x8899));
    if (s_rec == -1) {
       strncpy(ErrIOMsg,"Can't create raw socket for recieve! ",sizeof(ErrIOMsg));
       if (errno) 
           snprintf(ErrIOMsg+strlen(ErrIOMsg),sizeof(ErrIOMsg),"%s.",strerror(errno));
       return(1);
    }
    s_send = socket(PF_PACKET, SOCK_RAW, htons(0x8899));
    if (s_send == -1) { 
       strncpy(ErrIOMsg,"Can't create raw socket for send! ",sizeof(ErrIOMsg));
       if (errno)
           snprintf(ErrIOMsg+strlen(ErrIOMsg),sizeof(ErrIOMsg),"%s.",strerror(errno));
       return(1);
    }

    sockaddr_send.sll_family   = PF_PACKET;	
    sockaddr_send.sll_protocol = htons(0x8899);
    sockaddr_send.sll_ifindex  = if_nametoindex(ifname);
    sockaddr_send.sll_hatype   = ARPHRD_ETHER;
    sockaddr_send.sll_pkttype  = PACKET_OTHERHOST;
    sockaddr_send.sll_halen    = 6;

    memcpy(&sockaddr_rec,&sockaddr_send,sizeof(sockaddr_rec));

    /*Get source MAC*/
    memset(&ifr, 0x00, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if(ioctl(s_rec, SIOCGIFHWADDR, &ifr) < 0){
       strncpy(ErrIOMsg,"Fail to get hw addr! ",sizeof(ErrIOMsg));
       if (errno) 
           snprintf(ErrIOMsg+strlen(ErrIOMsg),sizeof(ErrIOMsg),"%s.",strerror(errno));
       return(1);
    }
    memcpy(my_mac,ifr.ifr_hwaddr.sa_data,6);
    return(0);
}

//send to wire
ssize_t sock_send_(void *ptr, int size){
    int i,res;
    for (i=0;i<3;i++){
	res=sendto(s_send, ptr, size, 0, (struct sockaddr*)&sockaddr_send, sizeof(sockaddr_send));
	if (res!=-1) return(res);
	usleep(10000+40000*i);
    }
    return(res);
}

//recieve from wire, returns length
int sock_rec(void *ptr, int size, int waittick){
    int i,res=0,len=0;
    for (i=0;i<10;i++){
	res=recvfrom(s_rec, ptr, size, MSG_DONTWAIT, (struct sockaddr*)&sockaddr_rec, (unsigned int *)&len);
	if (res==-1){
	    usleep(waittick);
	}else{
	    break;
	}
    }
    return len;
}

#else
static int get_link_addr(const char *iface, unsigned char *haddr, size_t size) {
   struct ifaddrs *ifss, *ifs;
   int res;

   if (getifaddrs(&ifss) < 0)
      return errno;

   res=-1;

   for (ifs=ifss; ifs; ifs=ifs->ifa_next) {
      if (ifs->ifa_name == NULL)
	 continue;

      if (strcmp(iface, ifs->ifa_name) == 0) {
	 struct sockaddr_dl *sa;
	 /* Interface found */
	 res=-2;
	 if ((ifs->ifa_addr == NULL)
	       || (ifs->ifa_addr->sa_family != PF_LINK))
	    continue;

	 sa = (struct sockaddr_dl *)ifs->ifa_addr;

	 if (size >= sa->sdl_alen) {
	    unsigned i;
	    unsigned char *p;
	    /* MAC found */
	    res=0;
	    for (p=haddr, i=0; i < sa->sdl_alen; i++, p++)
	       *p = (unsigned char)sa->sdl_data[sa->sdl_nlen + i];
	 }else
	    res=-3;
	 break;
      }
   }
   freeifaddrs(ifss);
   return res;
}

int rtl83xx_prepare(){
    struct timeval time;
    struct timezone timez;

    timez.tz_minuteswest=0;
    timez.tz_dsttime=0;
    gettimeofday(&time, &timez);
    srand(time.tv_sec+time.tv_usec);

    {
       int res;
       res = get_link_addr(ifname, my_mac, sizeof(my_mac));
       if (res == -1) {
	  snprintf(ErrIOMsg,sizeof(ErrIOMsg),"Can't open interface %s.",ifname);
	  return(1);
       }else if (res < 0) {
	  snprintf(ErrIOMsg,sizeof(ErrIOMsg),"Fail to get hw addr");
	  return(1);
       }else if (res > 0) {
	  snprintf(ErrIOMsg,sizeof(ErrIOMsg),
		"Attempt to retrieve interface list failed: %s",
		strerror(errno));
	  return(1);
       }
    }
    memset(filter_app,0,sizeof(filter_app));
    snprintf(filter_app, sizeof(filter_app),
	  "ether proto 0x8899 and not ether src %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
	  my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);
    if ((handle = pcap_open_live(ifname, 128, 0, 3, errbuf))== NULL) {
       snprintf(ErrIOMsg,sizeof(ErrIOMsg),"pcap_open_live get error: %s", errbuf); 
       return(1);
    }
#if defined(BSD) && defined(BIOCIMMEDIATE)
    {
       int on = 1;
       if (ioctl(pcap_fileno(handle), BIOCIMMEDIATE, &on) < 0) {
          snprintf(ErrIOMsg,sizeof(ErrIOMsg),"Can't enable immediate mode on BPF device");
          return(1);
       }
    }
#endif
    pcap_setdirection(handle, PCAP_D_IN);

    if (pcap_compile(handle, &filter, filter_app, 1, 0) < 0) {
       snprintf(ErrIOMsg,sizeof(ErrIOMsg), "Bad pcap filter: %s", pcap_geterr(handle));
       return(1);
    }

    if (pcap_setfilter(handle, &filter) < 0) {
       snprintf(ErrIOMsg,sizeof(ErrIOMsg), "Bad pcap filter: %s", pcap_geterr(handle));
       return(1);
    }

    return(0);
}

//send to wire
ssize_t sock_send_(void *ptr, int size){
    int i,res;
    for (i=0;i<3;i++){
        res=pcap_inject(handle, ptr, size);
	if (res>0) return(res);
	usleep(10000+40000*i);
    }
    return(res);
}

//recieve from wire, returns length
int sock_rec(void *ptr, int size, int waittick){
    int len=0;
    int i=0;
    struct pcap_pkthdr pkt_h;

    while(i++ < 11){
     rcvbuf=pcap_next(handle,&pkt_h);
     if (rcvbuf!=NULL) break;
     usleep(waittick);
    }
    if (rcvbuf!=NULL){
     if (pkt_h.caplen > size) len=size;
     else len=pkt_h.caplen;
     memcpy(ptr,rcvbuf,len);
    }
    return len;
}
#endif

ssize_t sock_send(void *ptr, int size){
ssize_t res;
    if ((res=sock_send_(ptr,size)) < 0){
        printf("can't sendto!");
        _exit(1);
    }
    return(res);
}

int istr00mac(const unsigned char *buf){
int i,res;
 res=0;
 for(i=0;i<6;i++) res=+buf[i];
 return(res);
}

void rtl83xx_scan(int verbose, int retries){
    typedef struct sw_reply SW_REPLY;
    struct sw_reply { 
     struct rrcp_helloreply_packet_t pktr;
     SW_REPLY *prev;
    };
    int len = 0;
    int cnt_h_replies = 0;
    int cnt_r_replies = 0;
    int f_cnt = 0;
    int rep;
    int uplink = 0;
    int i;
    struct rrcp_packet_t pkt;
    struct rrcp_helloreply_packet_t pktr;
    SW_REPLY *hello_reply=NULL;
    SW_REPLY *rep_reply=NULL;
    SW_REPLY *current=NULL;
    SW_REPLY *next_rep_reply;
    SW_REPLY *reply4up;
    unsigned char *hidden_mac=NULL;


    memset(&pkt, 0, sizeof(pkt));
    memcpy(pkt.ether_dhost,mac_bcast,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);

/* scan based on Hello packets */
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x00;
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(authkey);

    for (i=1;i<=retries;i++){
	sock_send(&pkt, sizeof(pkt));
	usleep(1700*i);
    }

    f_cnt=0;
    while(1){
	memset(&pktr,0,sizeof(pktr));
	len=sock_rec(&pktr, sizeof(pktr),5000);
	if (len>14 && (memcmp(pktr.ether_dhost,my_mac,6)==0) &&
		pktr.ether_type==htons(0x8899) &&
		pktr.rrcp_proto==0x01 &&
		pktr.rrcp_opcode==0x00 &&
		pktr.rrcp_isreply==1 &&
		pktr.rrcp_authkey==htons(authkey)){
	    // do we already know this switch?
	    reply4up=hello_reply;
	    while (reply4up != NULL){
        	if ((memcmp(pktr.ether_shost,reply4up->pktr.ether_shost,6)==0) &&
        	    (memcmp(pktr.rrcp_uplink_mac,reply4up->pktr.rrcp_uplink_mac,6)==0) &&
        	    pktr.rrcp_downlink_port==reply4up->pktr.rrcp_downlink_port &&
        	    pktr.rrcp_uplink_port==reply4up->pktr.rrcp_uplink_port ){
            	    break;
                }
                current=reply4up->prev;
                reply4up=current;
            }
	    // only if we don't know this switch
	    if (reply4up == NULL){
		current=hello_reply;
        	if ((hello_reply=malloc(sizeof(SW_REPLY)))==NULL) {
		    printf("Out of memory!\n");
		    _exit(1);
		}
        	hello_reply->prev=current;
		memcpy(&hello_reply->pktr,&pktr,sizeof(pktr));
		cnt_h_replies++;
		f_cnt=0;
	    }
	}else{
	    if (len==0){
		f_cnt+=20; //for no traffic age counter fast
	    }else{
		f_cnt++; //for foreigh traffic age slowly
	    }
	}
	if (f_cnt>50){
	    break;
	}
    }

/* scan based on REP packets */
    current=NULL;
    pkt.rrcp_proto=0x02;
    pkt.rrcp_opcode=0x00;
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=0x0000;

    for (i=1;i<=retries;i++){
	sock_send(&pkt, sizeof(pkt));
	usleep(1700*i);
    }

    f_cnt=0;
    while(1){
	memset(&pktr,0,sizeof(pktr));
	len=sock_rec(&pktr, sizeof(pktr),5000);
	if (len>14 && (memcmp(pktr.ether_dhost,my_mac,6)==0) &&
		pktr.ether_type==htons(0x8899) &&
		pktr.rrcp_proto==0x02 &&
		pktr.rrcp_opcode==0x00 &&
		pktr.rrcp_isreply==1) {
	    // do we already know this switch?
	    reply4up=rep_reply;
	    while (reply4up != NULL){
        	if (memcmp(pktr.ether_shost,reply4up->pktr.ether_shost,6)==0){
            	    break;
                }
                current=reply4up->prev;
                reply4up=current;
            }
	    // only if we don't know this switch
	    if (reply4up == NULL){
		current=rep_reply;
		if ((rep_reply=malloc(sizeof(SW_REPLY)))==NULL) {
		    printf("Out of memory!\n");
		    _exit(1);
		}
		rep_reply->prev=current;
		memcpy(&rep_reply->pktr,&pktr,sizeof(pktr));
		cnt_r_replies++;
		f_cnt=0;
	    }
	}else{
	    if (len==0){
		f_cnt+=20; //for no traffic age counter fast
	    }else{
		f_cnt++; //for foreigh traffic age slowly
	    }
	}
	if (f_cnt>50){
	    break;
	}
    }

/* print result */
    if (cnt_h_replies || cnt_r_replies){
       if (verbose){
  	   printf("  switch MAC/port        via MAC/port      vendor_id/chip_id REP\n");
       }else{
           printf("  switch MAC      Hello REP\n");
       }
    }else{
       printf("No switch(es) found.\n");
       return;
    }
    if (cnt_h_replies){
       // search "hidden" devices from uplinks info
       hidden_mac=malloc((cnt_h_replies+1)*6);
       if (hidden_mac != NULL){ 
         // collect all uplinks
         bzero(hidden_mac,(cnt_h_replies+1)*6);
         reply4up=hello_reply;
         while (reply4up != NULL){
           if (istr00mac(reply4up->pktr.rrcp_uplink_mac)) memcpy(&hidden_mac[uplink++*6],&reply4up->pktr.rrcp_uplink_mac,6);
           current=reply4up->prev;
           reply4up=current;
         }
         // filter known mac from devices, who answer on Hello-packet
         for(i=0;i<uplink;i++){
            reply4up=hello_reply;
            while (reply4up != NULL){
              if (memcmp(&reply4up->pktr.ether_shost,&hidden_mac[i*6],6)==0){
               bzero(&hidden_mac[i*6],6);
               break;
              }
              current=reply4up->prev;
              reply4up=current;
            }
         }
         // filter known mac from devices, who answer on REP-packet
         if (cnt_r_replies){
           for(i=0;i<uplink;i++){
              reply4up=rep_reply;
              while (reply4up != NULL){
                if (istr00mac(&hidden_mac[i*6])){
                  if (memcmp(&reply4up->pktr.ether_shost,&hidden_mac[i*6],6)==0){
                   bzero(&hidden_mac[i*6],6);
                   break;
                  }
                }
                current=reply4up->prev;
                reply4up=current;
              }
            }
         }
       } // end of search "hidden" devices

       while (hello_reply != NULL){
                // We have REP reply from this switch?
                next_rep_reply=NULL;
                current=rep_reply;
                rep=0;
                while (current != NULL){
                 if (memcmp(&hello_reply->pktr.ether_shost,&current->pktr.ether_shost,6)==0){
                    rep++; 
                    if (next_rep_reply == NULL){
                      rep_reply=rep_reply->prev;
                    }else{
                      next_rep_reply->prev=current->prev;
                    }
                    free(current);
                    break;
                 }else{
                  next_rep_reply=current;
                  current=current->prev;
                 }
                }

		printf("%02x:%02x:%02x:%02x:%02x:%02x",
			hello_reply->pktr.ether_shost[0],
			hello_reply->pktr.ether_shost[1],
			hello_reply->pktr.ether_shost[2],
			hello_reply->pktr.ether_shost[3],
			hello_reply->pktr.ether_shost[4],
			hello_reply->pktr.ether_shost[5]);
		if (verbose){
                    printf("/%-2d",map_port_number_from_physical_to_logical(hello_reply->pktr.rrcp_downlink_port));
		    if (istr00mac(hello_reply->pktr.rrcp_uplink_mac)){
 			printf(" %02x:%02x:%02x:%02x:%02x:%02x/%-2d",
				hello_reply->pktr.rrcp_uplink_mac[0],
				hello_reply->pktr.rrcp_uplink_mac[1],
				hello_reply->pktr.rrcp_uplink_mac[2],
				hello_reply->pktr.rrcp_uplink_mac[3],
				hello_reply->pktr.rrcp_uplink_mac[4],
				hello_reply->pktr.rrcp_uplink_mac[5],
				map_port_number_from_physical_to_logical(hello_reply->pktr.rrcp_uplink_port));
		     }else{
 			printf("                     ");
		     }
 		     printf(" 0x%08x/0x%04x  %s\n",
			hello_reply->pktr.rrcp_vendor_id,
			hello_reply->pktr.rrcp_chip_id,
                        (rep)?"Yes":"No");
		}else{
		    printf("   +    %c\n",(rep)?'+':'-');
		}
            current=hello_reply->prev;
            free(hello_reply);
            hello_reply=current;
       }
    }

    if (cnt_r_replies){
       while (rep_reply != NULL){
                printf("%02x:%02x:%02x:%02x:%02x:%02x",
			rep_reply->pktr.ether_shost[0],
			rep_reply->pktr.ether_shost[1],
			rep_reply->pktr.ether_shost[2],
			rep_reply->pktr.ether_shost[3],
			rep_reply->pktr.ether_shost[4],
			rep_reply->pktr.ether_shost[5]);
                if (verbose) printf("    No hello-reply, RRCP disabled?          Yes\n");
                else printf("   -    +\n");
            current=rep_reply->prev;
            free(rep_reply);
            rep_reply=current;
       }
    }

   if (uplink){ //print info about "hidden devices"
     for(i=0;i<uplink;i++){
       if (istr00mac(&hidden_mac[i*6])){
         printf("%02x:%02x:%02x:%02x:%02x:%02x",
                hidden_mac[i*6],
                hidden_mac[i*6+1],
                hidden_mac[i*6+2],
                hidden_mac[i*6+3],
                hidden_mac[i*6+4],
                hidden_mac[i*6+5]);
         if (verbose) printf("    No hello-reply, RRCP disabled?          No\n");
         else printf("   -    -\n");
       }
     }
   }
   if (hidden_mac != NULL) free(hidden_mac);
}

int rrcp_io_probe_switch_for_facing_switch_port(uint8_t *switch_mac_address, uint8_t *facing_switch_port_phys){
    typedef struct sw_reply SW_REPLY;
    int len = 0;
    int i;
    struct rrcp_packet_t pkt;
    struct rrcp_helloreply_packet_t pktr;

    memcpy(pkt.ether_dhost,switch_mac_address,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);

    /* scan with Hello packet */
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x00;
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(authkey);

    sock_send(&pkt, sizeof(pkt));

    for (i=0;i<10;i++){
	usleep(100+5000*i);
	memset(&pktr,0,sizeof(pktr));
	len=sock_rec(&pktr, sizeof(pktr),5000);
	if (len >14 &&
    	    (memcmp(pktr.ether_dhost,my_mac,6)==0)&&
	    pktr.ether_type==htons(0x8899) &&
	    pktr.rrcp_proto==0x01 &&
	    pktr.rrcp_opcode==0x00 &&
	    pktr.rrcp_isreply==1 &&
	    pktr.rrcp_authkey==htons(authkey)){
	    *facing_switch_port_phys=(int)pktr.rrcp_downlink_port;
	    break;
	}
    }
    if (i>=10){
	*facing_switch_port_phys=-1;
	return -1;
    }
    return 0;
}

/*
 * read a register and return status of operation
 * 0 - no error
 * 1 - send error
 * 2 - receive error
 */
int rtl83xx_readreg32_(uint16_t regno,uint32_t *regval){
    int len = 0;
    int i;
    struct rrcp_packet_t pkt,pktr;

    memcpy(pkt.ether_dhost,dest_mac,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x01;//0=hello; 1=get; 2=set
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(authkey);
    pkt.rrcp_reg_addr=regno;
    pkt.rrcp_reg_data=0;
    pkt.cookie1=rand();
    pkt.cookie2=rand();

    for(i=0;i<30;i++){
        if (sock_send_(&pkt, sizeof(pkt)) < 0) return(1);
	usleep(100+5000*i);
	memset(&pktr,0,sizeof(pktr));
	len=sock_rec(&pktr, sizeof(pktr),100);
	if (len >14 &&
	    (memcmp(pktr.ether_dhost,my_mac,6)==0)&&
	    pktr.ether_type==htons(0x8899) &&
	    pktr.rrcp_proto==0x01 &&
	    pktr.rrcp_opcode==0x01 &&
	    pktr.rrcp_isreply==1 &&
	    pktr.rrcp_authkey==htons(authkey)&&
	    pktr.rrcp_reg_addr==regno){
                *regval=pktr.rrcp_reg_data;
                return(0);
	}
    }
    return(2);
}

/*
 *  wrapper for compatibility with previously writed code
 */
uint32_t rtl83xx_readreg32(uint16_t regno){
uint32_t regvalue;
int res;
  res=rtl83xx_readreg32_(regno,&regvalue);
  switch (res){
        case 1:
              printf("Can't send\n");
              exit(1);
              ;;
        case 2:
              printf("can't read register 0x%04x! Switch is down?\n",regno);
              exit(1);
              ;;
        default:
              ;;
  }
  return(regvalue);
}

uint16_t rtl83xx_readreg16(uint16_t regno){
    return (uint16_t)rtl83xx_readreg32(regno);
}

uint32_t get_register_mask(uint16_t regnum,int mode){
 int i=0;

 while ( register_mask[switchtypes[switchtype].chip_id][i] < 0x10000){
    if ( (regnum >= register_mask[switchtypes[switchtype].chip_id][i]) && 
         (regnum < (register_mask[switchtypes[switchtype].chip_id][i]+register_mask[switchtypes[switchtype].chip_id][i+3]))){
           return(register_mask[switchtypes[switchtype].chip_id][i+1+mode]);
    }
    if (regnum < register_mask[switchtypes[switchtype].chip_id][i]) {
      return(0);
    }
    i+=4;
 }
 return(0);
}

void rtl83xx_setreg32(uint16_t regno, uint32_t regval){
    int cnt = 0;
    struct rrcp_packet_t pkt;
    uint16_t prev_auth=0;
    uint32_t mask=0xffff;

    memcpy(pkt.ether_dhost,dest_mac,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x02;//0=hello; 1=get; 2=set
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(authkey);
    pkt.rrcp_reg_addr=regno;
    pkt.rrcp_reg_data=regval;
    pkt.cookie1=rand();
    pkt.cookie2=rand();
/*
    if (regno==0x0500){
	mask=0x7fff;
    }else if (regno==0x0217){
	mask=0x0fff;
    }else if (regno==0x0218){
	mask=0x00ff;
    }
*/
    mask=get_register_mask(regno,GetWriteMask);
    if (!mask) { printf("Register %03x not exists or read-only\n",regno); return;}

    for (cnt=0;cnt<3;cnt++){
	sock_send(&pkt, sizeof(pkt));
        if (!regno) return; // because register 0 self clearing 
        if (regno == 0x209) { // special hack for new authkey
           prev_auth=authkey; authkey=(uint16_t)regval; 
        } 
	if ((rtl83xx_readreg32(regno) & mask)==(regval & mask)){
	    return;
	}
        if (regno == 0x209) { authkey=prev_auth; } // revert authkey if unfinished change 
    }
    printf("can't set register 0x%04x to value 0x%04x (read value is 0x%04x)\n",regno,regval,rtl83xx_readreg32(regno));
//    _exit(0);
}

void rtl83xx_setreg16(uint16_t regno, uint16_t regval){
    rtl83xx_setreg32(regno, (uint32_t) regval);
}

int wait_eeprom(){
    int i;
    uint16_t res;

    for(i=1;i<=20;i++){
	res=rtl83xx_readreg16(0x217);
	if ((res&0x1000) == 0) return(res);
	usleep(7500*i);
    }
    return(0xffff);
}

/* 
   Concerning algorithm of work with EEPROM the documentation contains a number of discrepancies.
   Correct data are resulted in tables 22 and 23 chapters 8.26.2 of description RTL8316b.

   register 0217h:
      bit 0-7  : EEPROM address
      bit 8-10 : Chip select (EEPROM = 0)
      bit 11   : Operation Read(1)/Write(0)
      bit 12   : Status Busy(1)/Idle(0)
      bit 13   : Operation success status Fail(1)/Success(0)
      bit 14-15 : reserved
   
   register 0218h:
      bit 0-7  : data written to EEPROM
      bit 8-15 : data read from EEPROM
*/

int eeprom_write(uint16_t addr,uint8_t data){
    rtl83xx_setreg16(0x218,((uint16_t)data)&0xff);
    rtl83xx_setreg16(0x217,addr&0x7ff);
    if ((wait_eeprom()&0x2000)!=0){
	return 1;
    }
    return 0;
}

int eeprom_read(uint16_t addr,uint8_t *data){
    uint16_t tmp;

    data[0]=0;
    rtl83xx_setreg16(0x217,(addr&0x7ff)|0x800);
    if ((wait_eeprom()&0x2000)!=0){
	return 1;
    }
    tmp=rtl83xx_readreg16(0x218);
    data[0]=(unsigned char)(tmp>>8);
    return 0;
}

int phy_wait(){
    int i;
    uint16_t res;

    for(i=0;i<10;i++){
	res=rtl83xx_readreg16(0x500);
	if ((res&0x8000) == 0) return(res);
	    usleep(5000);
    }
    return(0xffff);
}

int phy_read(uint16_t phy_portno, uint8_t phy_regno, uint16_t *data){
    uint16_t tmp;

    rtl83xx_setreg16(0x500,((phy_portno&0x01f)<<5)|(phy_regno&0x01f));
    if ((phy_wait()&0x8000)!=0){
	return 1;
    }
    tmp=rtl83xx_readreg16(0x502);
    data[0]=tmp;
//    printf("PHY_READ(%d,%d)=0x%04x\n",phy_portno,phy_regno,(int)tmp);
    return 0;
}

int phy_write(uint16_t phy_portno, uint8_t phy_regno, uint16_t data){
//    printf("PHY_WRITE(%d,%d)=0x%04x\n",phy_portno,phy_regno,data);
    rtl83xx_setreg16(0x501,data);
    rtl83xx_setreg16(0x500,((phy_portno&0x01f)<<5)|(phy_regno&0x01f)|1<<14);
    if ((phy_wait()&0x8000)!=0){
	return 1;
    }
    return 0;
}

//returns 1 if got response
uint32_t rtl83xx_ping(void){
    int i,len = 0;
    struct rrcp_packet_t pkt,pktr;

    memcpy(pkt.ether_dhost,dest_mac,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x00;//0=hello; 1=get; 2=set
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(authkey);

    sock_send(&pkt, sizeof(pkt));

    for(i=0;i<20;i++){
	usleep(100+1000*i);
	memset(&pktr,0,sizeof(pktr));
	len=sock_rec(&pktr, sizeof(pktr),100);
	if (len >14 &&
	    (memcmp(pktr.ether_dhost,my_mac,6)==0)&&
	    pktr.ether_type==htons(0x8899) &&
	    pktr.rrcp_proto==0x01 &&
	    pktr.rrcp_opcode==0x00 &&
	    pktr.rrcp_isreply==1 &&
	    pktr.rrcp_authkey==htons(authkey)){
	    return 1;
	}
    }
    return 0;
}

void do_write_memory(){
 do_write_eeprom_all(1);
}

void do_write_eeprom_defaults(){
 do_write_eeprom_all(0);
}

void do_write_eeprom_all(int mode){
 int i=0;
 int k=0;
 int regnum,l;
 uint16_t addr,data;

 while (switchtypes[switchtype].reg2eeprom[i] > -1){
   for (k=0;k<switchtypes[switchtype].reg2eeprom[i+2];k++){
     if (mode) { data=rtl83xx_readreg16(switchtypes[switchtype].reg2eeprom[i]+k);}
     else{
       regnum=switchtypes[switchtype].reg2eeprom[i]+k;
       l=data=0;
       while (switchtypes[switchtype].regdefval[l] > -1){
         if ( (regnum >= switchtypes[switchtype].regdefval[l]) && (regnum < (switchtypes[switchtype].regdefval[l]+switchtypes[switchtype].regdefval[l+2]))){
           data=switchtypes[switchtype].regdefval[l+1];
           break;
         }
         l+=3;
       }
     }
     addr=(uint16_t)switchtypes[switchtype].reg2eeprom[i+1]+k*2;
//    printf("0x%04x -> 0x%04x\n",switchtypes[switchtype].reg2eeprom[i]+k,addr);
     if ( eeprom_write(addr,(uint8_t)(data&0x00ff)) ||
          eeprom_write(addr+1,(uint8_t)(data>>8))  ) {
      usleep(100000);
      if ( eeprom_write(addr,(uint8_t)(data&0x00ff)) ||
       eeprom_write(addr+1,(uint8_t)(data>>8))  ) {
       printf("Can't write register 0x%04x to EEPROM 0x%03x\n",switchtypes[switchtype].reg2eeprom[i]+k,addr);
       exit(1);
      }
     }
     // else printf("Success write register N0x%04x to EEPROM 0x%03x-0x%03x value 0x%04x\n",switchtypes[switchtype].reg2eeprom[i]+k,addr-1,addr,data);
   }
   i+=3;
 }
}

//hardware-specific register mappings
int map_reg_to_eeprom(int switch_reg_no){
    int i;

    for (i=0;switchtypes[switchtype].reg2eeprom[i] > -1;i+=3){
	if ((switchtypes[switchtype].reg2eeprom[i]<=switch_reg_no) &&
	    (switchtypes[switchtype].reg2eeprom[i]+
	     switchtypes[switchtype].reg2eeprom[i+2]>switch_reg_no)){
	     return (switchtypes[switchtype].reg2eeprom[i+1]+(switch_reg_no-switchtypes[switchtype].reg2eeprom[i]));
	}
    }
    return -1;
}
