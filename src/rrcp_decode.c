/*
    This file is part of OpenRRCP

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

/*
   Reads a raw binary file, created by tcpdump or windump, and shortly deciphers the 
   basic RRCP commands. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <pcap.h>
#include "rrcp_packet.h"

#define ETH_TYPE_RRCP   0x8899          /* Realtek RRCP Protocol */

#define SET 1
#define GET 0

static pcap_t 			*handle;
int unsigned 			 count,verbose;
char 				 progname[32];

static void usage(void){
 fprintf(stderr, "Usage: \"%s <file>\"",progname);
 fprintf(stderr, " where <file> - raw binary file, created by tcpdump\nor windump. Look the description of an option \"-w\" in the documentation at the\ncorresponding program.\n");
 exit(2);
}

void print_val_217(int mode, uint16_t data){
 union {
   struct t_reg217{
     uint16_t
     address:8,
     chip_select:3,
     operation:1,
     status:1,
     fail:1,
     reserved:2;
   } value;
     uint16_t raw;
 } reg217;

 reg217.raw=data;
 printf("      Address: 0x%02x, chip: %u, ", reg217.value.address, reg217.value.chip_select);

 if (mode==SET) { printf("set mode to %s\n",(reg217.value.operation)?"Read":"Write"); }
 else { printf("status: %s %s %s\n",
               (reg217.value.operation)?"Read":"Write",
               (reg217.value.status)?"Busy":"Idle",
               (reg217.value.fail)?"Fail":"Success");
      }
}

void print_val_218(int mode,uint16_t data){
 union {
  struct t_reg218{
    uint16_t
    written:8,
    read:8;
  } data;
    uint16_t raw;
 } reg218;

 reg218.raw=data;
 if (mode==SET) { printf("      Data written to device 0x%02x\n", reg218.data.written);}
 else { printf("      Data read from device 0x%02x\n", reg218.data.read);}
}

int main(int argc, char *argv[]){
 char *pTemp;
 char errbuf[PCAP_ERRBUF_SIZE];
 struct pcap_pkthdr pkt_h;
 struct rrcp_packet_t pktr;
 const  u_char *rcvbuf;
 
 count=0;
 verbose=0;

 if ((pTemp=strrchr(argv[0],'/'))!=NULL) strncpy(progname,pTemp+1,sizeof(progname));
 else strncpy(progname,argv[0],sizeof(progname));

 if (argc == 1) usage();
 
 if ((handle = pcap_open_offline(argv[1], errbuf))== NULL) {
   fprintf(stderr, "pcap_open_offline: %s", errbuf);
   exit(2);
 }

 while ((rcvbuf=pcap_next(handle, &pkt_h)) != NULL) {
  memcpy(&pktr,rcvbuf,(pkt_h.caplen>sizeof(pktr)?sizeof(pktr):pkt_h.caplen));
  count++;
  if (pktr.ether_type!=htons(ETH_TYPE_RRCP)) continue;
 
  if ((pktr.rrcp_reg_addr == 0x217)||(pktr.rrcp_reg_addr == 0x218)) printf (" ");
  else printf ("!");

  printf("From:%02x:%02x:%02x:%02x:%02x:%02x", 
         pktr.ether_shost[0], pktr.ether_shost[1], pktr.ether_shost[2],
         pktr.ether_shost[3], pktr.ether_shost[4], pktr.ether_shost[5]);
  printf(" to:%02x:%02x:%02x:%02x:%02x:%02x", 
         pktr.ether_dhost[0], pktr.ether_dhost[1], pktr.ether_dhost[2],
         pktr.ether_dhost[3], pktr.ether_dhost[4], pktr.ether_dhost[5]);
 
  if (pktr.rrcp_proto != 1) {
   printf(" Proto %02x\n",pktr.rrcp_proto);
   continue;
  }
 
  switch(pktr.rrcp_opcode){
   case 0:
         if (pktr.rrcp_isreply) printf(" Hello reply\n");
         else printf(" Hello\n"); 
	 break;
   case 1: // get
         if (pktr.rrcp_isreply) { 
           printf(" Get reply reg(0x%04x)=0x%08x\n",pktr.rrcp_reg_addr, pktr.rrcp_reg_data); 
           if (pktr.rrcp_reg_addr == 0x217) print_val_217(GET,(uint16_t)pktr.rrcp_reg_data);
           if (pktr.rrcp_reg_addr == 0x218) print_val_218(GET,(uint16_t)pktr.rrcp_reg_data);
         }
         else printf(" Get reg 0x%04x\n",pktr.rrcp_reg_addr);
         break;
   case 2: // set
         if (pktr.rrcp_isreply) printf(" Set reply reg(0x%04x)=0x%08x\n",pktr.rrcp_reg_addr, pktr.rrcp_reg_data);
         else { 
          printf(" Set reg(0x%04x)=0x%08x\n",pktr.rrcp_reg_addr, pktr.rrcp_reg_data);
          if (pktr.rrcp_reg_addr == 0x217) print_val_217(SET,(u_int16_t)pktr.rrcp_reg_data);
          if (pktr.rrcp_reg_addr == 0x218) print_val_218(SET,(u_int16_t)pktr.rrcp_reg_data);
         }
         break;
   default:
         printf(" Unk. opcode %2x",pktr.rrcp_opcode);
  }
  printf("\n");
 }
 
 printf("Got %u packets\n",count);
 pcap_close(handle);
 exit(0);
}
