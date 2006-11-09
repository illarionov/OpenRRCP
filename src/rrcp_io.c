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

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
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
#include "rrcp_switches.h"

char ifname[128] = "";
unsigned char my_mac[6] = {0x00, 0x00, 0x11, 0x22, 0x33, 0x44};
unsigned char dest_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned char mac_bcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

unsigned int switchtype;

int s,s_rec,s_send;

struct sockaddr_ll sockaddr_rec,sockaddr_send;

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

void rtl83xx_prepare(){
    struct ifreq ifr;

    s_rec = socket(PF_PACKET, SOCK_RAW, htons(0x8899));
    if (s_rec == -1) { printf("can't create raw socket for recieve!\nAre we are running as root (uid=0)?\n"); exit(0); }
    s_send = socket(PF_PACKET, SOCK_RAW, htons(0x8899));
    if (s_send == -1) { printf("can't create raw socket for send!\nAre we are running as root (uid=0)?\n"); exit(0); }

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
	printf("Fail to get hw addr\nAre we are running as root (uid=0)?\n");
	exit(0);
    }
    memcpy(my_mac,ifr.ifr_hwaddr.sa_data,6);
}

//send to wire
ssize_t sock_send(void *ptr, int size){
    int i,res;
    for (i=0;i<3;i++){
	res=sendto(s_send, ptr, size, 0, (struct sockaddr*)&sockaddr_send, sizeof(sockaddr_send));
	if (res!=-1) return res;
	usleep(50000);
    }
    printf("can't sendto!");
    _exit(1);
}

//recieve from wire, returns length
int sock_rec(void *ptr, int size, int waittick){
    int i,res=0,len=0;
    for (i=0;i<10;i++){
	res=recvfrom(s_rec, ptr, size, MSG_DONTWAIT, (struct sockaddr*)&sockaddr_rec, (unsigned int *)&len);
	if (res==-1){
	    usleep(waittick);
	}else{
	    return len;
	}
    }
    printf("can't recvfrom!");
    _exit(1);
}

void rtl83xx_scan(int verbose){
    int len = 0;
    int cnt_replies = 0;
    struct rrcp_packet_t pkt;
    struct rrcp_helloreply_packet_t pktr;

    memcpy(pkt.ether_dhost,mac_bcast,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x00;
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(0x2379);

    sock_send(&pkt, sizeof(pkt));

    if (verbose){
	printf("  switch MAC/port        via MAC/port      vendor_id/chip_id\n");
    }

    usleep(1000);
    while(1){
	memset(&pktr,0,sizeof(pktr));
	len=sock_rec(&pktr, sizeof(pktr),5000);
	if (len >14 &&
	    (memcmp(pktr.ether_dhost,my_mac,6)==0)&&
	    pktr.ether_type==htons(0x8899) &&
	    pktr.rrcp_proto==0x01 &&
	    pktr.rrcp_opcode==0x00 &&
	    pktr.rrcp_isreply==1 &&
	    pktr.rrcp_authkey==htons(0x2379)){
		if (verbose){
		    printf("%02x:%02x:%02x:%02x:%02x:%02x/%-2d %02x:%02x:%02x:%02x:%02x:%02x/%-2d 0x%08x/0x%04x\n",
			pktr.ether_shost[0],
			pktr.ether_shost[1],
			pktr.ether_shost[2],
			pktr.ether_shost[3],
			pktr.ether_shost[4],
			pktr.ether_shost[5],
			map_port_number_from_physical_to_logical(pktr.rrcp_downlink_port),
			pktr.rrcp_uplink_mac[0],
			pktr.rrcp_uplink_mac[1],
			pktr.rrcp_uplink_mac[2],
			pktr.rrcp_uplink_mac[3],
			pktr.rrcp_uplink_mac[4],
			pktr.rrcp_uplink_mac[5],
			map_port_number_from_physical_to_logical(pktr.rrcp_uplink_port),
//			pktr.rrcp_uplink_port,
			pktr.rrcp_vendor_id,
			pktr.rrcp_chip_id);
		}else{
		    printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
			pktr.ether_shost[0],
			pktr.ether_shost[1],
			pktr.ether_shost[2],
			pktr.ether_shost[3],
			pktr.ether_shost[4],
			pktr.ether_shost[5]);
		}
		cnt_replies++;
	}
    }
}

unsigned long rtl83xx_readreg32(unsigned short regno){
    int len = 0;
    struct rrcp_packet_t pkt,pktr;

    memcpy(pkt.ether_dhost,dest_mac,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x01;//0=hello; 1=get; 2=set
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(0x2379);
    pkt.rrcp_reg_addr=regno;
    pkt.rrcp_reg_data=0;

    sock_send(&pkt, sizeof(pkt));

    usleep(100);
    while(1){
	memset(&pktr,0,sizeof(pktr));
	len=sock_rec(&pktr, sizeof(pktr),100);
	if (len >14 &&
	    (memcmp(pktr.ether_dhost,my_mac,6)==0)&&
	    pktr.ether_type==htons(0x8899) &&
	    pktr.rrcp_proto==0x01 &&
	    pktr.rrcp_opcode==0x01 &&
	    pktr.rrcp_isreply==1 &&
	    pktr.rrcp_authkey==htons(0x2379)&&
	    pktr.rrcp_reg_addr==regno){
	        return(pktr.rrcp_reg_data);
	}
    }
}

unsigned short rtl83xx_readreg16(unsigned short regno){
    return (unsigned short)rtl83xx_readreg32(regno);
}

void rtl83xx_setreg16(unsigned short regno, unsigned long regval){
    int cnt = 0;
    struct rrcp_packet_t pkt;

    memcpy(pkt.ether_dhost,dest_mac,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x02;//0=hello; 1=get; 2=set
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(0x2379);
    pkt.rrcp_reg_addr=regno;
    pkt.rrcp_reg_data=regval;

    for (cnt=0;cnt<3;cnt++){
	sock_send(&pkt, sizeof(pkt));
	if (rtl83xx_readreg32(regno)==regval){
	    return;
	}
    }
//    printf("can't set register 0x%04x to value 0x%04lu (read value is 0x%04lu)\n",regno,regval,rtl83xx_readreg32(regno));
//    _exit(0);
}

void rtl83xx_setreg16reg16(unsigned short regno, unsigned short regval){
    rtl83xx_setreg16(regno, (unsigned long) regval);
}
