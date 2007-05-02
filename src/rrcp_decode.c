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
#include <unistd.h>
#include <err.h>
#include <pcap.h>
#include "rrcp_packet.h"

#define ETH_TYPE_RRCP   0x8899          /* Realtek RRCP Protocol */

#define SET 1
#define GET 0

static pcap_t *handle;
int unsigned   count,verbose,debug;
char 	       progname[32];

static void usage(void){
 fprintf(stderr, "Usage: \"%s [-p] -r <file>\"\n",progname);
 fprintf(stderr, "       \"%s [-p] [xx:xx:xx:xx:xx:xx@]ifname\"\n",progname);
 fprintf(stderr, " <file> - raw binary file, created by tcpdumpor windump. Look the description\n");
 fprintf(stderr, "          of an option \"-w\" in the documentation at the corresponding program.\n");
 fprintf(stderr, " ifname - interface to capture packets\n");
 fprintf(stderr, " xx:xx:xx:xx:xx:xx - MAC address\n");
 fprintf(stderr, " -p - put the interface into promiscuous mode.\n");
// fprintf(stderr, " -v - enable verbose output\n");
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
 int c;
 char *file_name=NULL;
 int promisc=0;
 int switchtype=-1;
 unsigned int x[6];
 struct bpf_program filter;
 bpf_u_int32 mask;
 bpf_u_int32 net;
 char filter_app[512];
 char ifname[128];

 count=0;
 verbose=0;
 debug=0;

 if ((pTemp=strrchr(argv[0],'/'))!=NULL) strncpy(progname,pTemp+1,sizeof(progname));
 else strncpy(progname,argv[0],sizeof(progname));

 while ((c = getopt(argc, argv, "dpvr:t:h?")) != -1) {
  switch (c) {
   case 'd':
             debug++;
             break;
   case 'p':
             promisc++;
             break;
   case 'v':
             verbose++;
             break;
   case 'r':
             file_name = optarg;
             break;
   case 't':
             switchtype=atoi(optarg);
             break;
   default:
             usage();
             break;
  }
 }

 bzero(ifname,sizeof(ifname));
 bzero(filter_app,sizeof(filter_app));

 if (file_name){
   if ((handle = pcap_open_offline(file_name, errbuf))== NULL) {
     errx(2, "pcap_open_offline: %s", errbuf);
   }
 }else{
    if (optind < argc) {
        if (sscanf(argv[optind], "%x:%x:%x:%x:%x:%x@%s",x,x+1,x+2,x+3,x+4,x+5,ifname)==7){
            snprintf(filter_app, sizeof(filter_app)-1, "ether host %02x:%02x:%02x:%02x:%02x:%02x and ether proto 0x%x",*x,*(x+1),*(x+2),*(x+3),*(x+4),*(x+5),ETH_TYPE_RRCP); 
        }else{
            strncpy(ifname,argv[optind],sizeof(ifname)-1);
            snprintf(filter_app, sizeof(filter_app), "ether proto 0x%x", ETH_TYPE_RRCP);
        }
        optind++;
    }else 
        usage();

    if (debug) printf("Filter: %s\n",filter_app);
    if (pcap_lookupnet(ifname, &net, &mask, errbuf) < 0) { net = 0; mask = 0; }
    if ((handle = pcap_open_live(ifname, 128, promisc, 100, errbuf)) == NULL)
      errx(2, "pcap_open_live: %s", errbuf);
#if defined(BSD) && defined(BIOCIMMEDIATE)
    {
      int on = 1;
      if (ioctl(pcap_fileno(handle), BIOCIMMEDIATE, &on) < 0) errx(2, "BIOCIMMEDIATE");
    }
#endif
    if ( (pcap_compile(handle, &filter, filter_app, 1, mask) < 0) ||
         (pcap_setfilter(handle, &filter) < 0)) 
      errx(2, "bad pcap filter: %s", pcap_geterr(handle));
  }

 for(;;){

  rcvbuf=pcap_next(handle, &pkt_h);
  if (rcvbuf == NULL ){ 
   if (file_name) break;
   else { usleep(100000); continue;}
  }

  memcpy(&pktr,rcvbuf,(pkt_h.caplen>sizeof(pktr)?sizeof(pktr):pkt_h.caplen));
  if (pktr.ether_type!=htons(ETH_TYPE_RRCP)) continue;
  count++;
 
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
