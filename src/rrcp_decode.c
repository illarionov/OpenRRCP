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

    Some support can be found at: http://openrrcp.org.ru/
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
#include "rrcp_io.h"
#include "rrcp_config.h"
#include "rrcp_switches.h"

#define ETH_TYPE_RRCP   0x8899          /* Realtek RRCP Protocol */

#define GET       0
#define SET       1
#define GET_REPLY 2
#define SET_REPLY 3

static pcap_t *handle;
int unsigned   count,verbose,debug;
char 	       progname[32];
void         (*rrcp_decode[0x1000])(int, uint16_t, uint32_t);

void init_decoder(void);

static void usage(void){
 fprintf(stderr, "Usage: \"%s [-p] [-v [-v]] -r <file>\"\n",progname);
 fprintf(stderr, "       \"%s [-p] [-v [-v]] [xx:xx:xx:xx:xx:xx@]ifname\"\n",progname);
 fprintf(stderr, " <file> - raw binary file, created by tcpdumpor windump. Look the description\n");
 fprintf(stderr, "          of an option \"-w\" in the documentation at the corresponding program.\n");
 fprintf(stderr, " ifname - interface to capture packets\n");
 fprintf(stderr, " xx:xx:xx:xx:xx:xx - MAC address\n");
 fprintf(stderr, " -p - put the interface into promiscuous mode.\n");
 fprintf(stderr, " -v - be verbose (and more verbose)\n");
 exit(2);
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
 int switchtype=4;
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

 init_decoder();
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
         if (pktr.rrcp_isreply){ 
           printf(" Get reply reg(0x%04x)=0x%08x\n",pktr.rrcp_reg_addr, pktr.rrcp_reg_data); 
           if (verbose) (rrcp_decode[pktr.rrcp_reg_addr])(GET_REPLY,pktr.rrcp_reg_addr, pktr.rrcp_reg_data);
         }else{
           printf(" Get reg 0x%04x\n",pktr.rrcp_reg_addr);
           if (verbose) (rrcp_decode[pktr.rrcp_reg_addr])(GET,pktr.rrcp_reg_addr, pktr.rrcp_reg_data);
         }
         break;
   case 2: // set
         if (pktr.rrcp_isreply) printf(" Set reply reg(0x%04x)=0x%08x\n",pktr.rrcp_reg_addr, pktr.rrcp_reg_data);
         else { 
          printf(" Set reg(0x%04x)=0x%08x\n",pktr.rrcp_reg_addr, pktr.rrcp_reg_data);
          if (verbose) (rrcp_decode[pktr.rrcp_reg_addr])(SET,pktr.rrcp_reg_addr, pktr.rrcp_reg_data);
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

void print_reg(int mode, uint16_t addr, uint32_t data){
//  printf("\n");
  return;
}

void print_val_000(int mode, uint16_t addr, uint16_t data){
 printf("      System Reset Control Register \n");
 if (verbose < 2) return;
 switch(mode){
   case SET: 
            if (data==1) printf("      Software reboot\n");
            else if (data==2) printf("      Hardware reboot\n");
            else printf("Incorrect value\n");
            return;
   default:
            printf ("Abnormal operation. This register is self-clearning\n");
 }
}

void print_val_217(int mode, uint16_t addr, uint16_t data){
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

 printf("      EEPROM RW Command Register\n");
 if (verbose < 2) return;
 reg217.raw=(uint16_t)data;
  switch(mode){
   case SET: 
            printf("      Address: 0x%02x, chip: %u", reg217.value.address, reg217.value.chip_select);
            printf(", set mode to %s\n",(reg217.value.operation)?"Read":"Write");
            return;
   case GET_REPLY: 
            printf("      Address: 0x%02x, chip: %u", reg217.value.address, reg217.value.chip_select);
            printf(", status: %s %s %s\n",
                  (reg217.value.operation)?"Read":"Write",
                  (reg217.value.status)?"Busy":"Idle",
                  (reg217.value.fail)?"Fail":"Success");
            return;
   default:
           return;
 }
}

void print_val_218(int mode, uint16_t addr, uint32_t data){
 union {
  struct t_reg218{
    uint16_t
    written:8,
    read:8;
  } data;
    uint16_t raw;
 } reg218;

 printf("      EEPROM RW Data Register\n");
 if (verbose < 2) return;
 reg218.raw=(uint16_t)data;
 switch(mode){
    case SET: 
             printf("      Data written to device 0x%02x\n", reg218.data.written);
             return;
    case GET_REPLY:
             printf("      Data read from device 0x%02x\n", reg218.data.read);
             return;
    default:
             return;
 }
}

void print_val_001(int mode, uint16_t addr, uint32_t data){
  printf("      Switch Parameter Register \n");
}

void print_val_002(int mode, uint16_t addr, uint32_t data){
  if(switchtypes[switchtype].chip_id!=rtl8326)
    printf("      EEPROM Check ID \n");
  else 
    printf("      RX I/O PAD Delay Configuration \n");
}

void print_val_003(int mode, uint16_t addr, uint32_t data){
  printf("      TX I/O PAD Delay Configuration \n");
}

void print_val_004(int mode, uint16_t addr, uint32_t data){
  printf("      General Purpose User Defined I/O Data Register \n");
}

void print_val_005(int mode, uint16_t addr, uint32_t data){
  printf("      LED Display Configuration \n");
}

void print_val_100(int mode, uint16_t addr, uint32_t data){
  printf("      Board Trapping Status Register \n");
}

void print_val_101(int mode, uint16_t addr, uint32_t data){
  printf("      Loop Detect Status Register (32-Bit Register) \n");
}

void print_val_102(int mode, uint16_t addr, uint32_t data){
  printf("      System Fault Indication Register \n");
}

void print_val_200(int mode, uint16_t addr, uint32_t data){
  printf("      Realtek Protocol Control Register \n");
}

void print_val_201(int mode, uint16_t addr, uint32_t data){
  printf("      RRCP Security Mask Configuration Register 0 \n");
}

void print_val_202(int mode, uint16_t addr, uint32_t data){
  printf("      RRCP Security Mask Configuration Register 1 \n");
}

void print_val_203(int mode, uint16_t addr, uint32_t data){
  printf("      Switch MAC ID Register 0 \n");
}

void print_val_204(int mode, uint16_t addr, uint32_t data){
  printf("      Switch MAC ID Register 1 \n");
}

void print_val_205(int mode, uint16_t addr, uint32_t data){
  printf("      Switch MAC ID Register 2 \n");
}

void print_val_206(int mode, uint16_t addr, uint32_t data){
  printf("      Chip Model ID \n");
}

void print_val_207(int mode, uint16_t addr, uint32_t data){
  printf("      System Vendor ID Register 0 \n");
}

void print_val_208(int mode, uint16_t addr, uint32_t data){
  printf("      System Vendor ID Register 1 \n");
}

void print_val_209(int mode, uint16_t addr, uint32_t data){
  printf("      RRCP Authentication Key Configuration Register \n");
}

void print_val_20A(int mode, uint16_t addr, uint32_t data){
  printf("      Port 0, 1 Bandwidth Control Register \n");
}

void print_val_211(int mode, uint16_t addr, uint32_t data){
  printf("      Port 2~15 Bandwidth Control Register \n");
}

void print_val_216(int mode, uint16_t addr, uint32_t data){
  printf("      Port 2~25 Bandwidth Control Register \n");
}

void print_val_21E(int mode, uint16_t addr, uint32_t data){
  printf("      Port Mirror Control Register \n");
}

void print_val_219(int mode, uint16_t addr, uint32_t data){
  printf("      Port Mirror Control Register 0 for P15-P0 \n");
}

void print_val_21B(int mode, uint16_t addr, uint32_t data){
  printf("      RX Mirror Port Register 0 for P15-P0 \n");
}

void print_val_21D(int mode, uint16_t addr, uint32_t data){
  printf("      TX Mirror Port Register 0 for P15-P0 \n");
}

void print_val_300(int mode, uint16_t addr, uint32_t data){
  printf("      ALT Configuration Register \n");
}

void print_val_301(int mode, uint16_t addr, uint32_t data){
  printf("      Address Learning Control Register 0 \n");
}

void print_val_302(int mode, uint16_t addr, uint32_t data){
  printf("      Address Learning Control Register 1 \n");
}

void print_val_307(int mode, uint16_t addr, uint32_t data){
  printf("      Port Trunking Configuration Register \n");
}

void print_val_308(int mode, uint16_t addr, uint32_t data){
  printf("      IGMP Snooping Control Register \n");
}

void print_val_309(int mode, uint16_t addr, uint32_t data){
  printf("      IP Multicast Router Port Discovery Register (32 bits) \n");
}

void print_val_30B(int mode, uint16_t addr, uint32_t data){
  printf("      VLAN Control Register \n");
}

void print_val_313(int mode, uint16_t addr, uint32_t data){
  printf("      Port VLAN ID Assignment Index Register 0~7 \n");
}

void print_val_318(int mode, uint16_t addr, uint32_t data){
  printf("      Port VLAN ID Assignment Index Register 0~12 \n");
}

void print_val_31A(int mode, uint16_t addr, uint32_t data){
  printf("      VLAN Output Port Priority-Tagging Control Register 0, 1 \n");
}

void print_val_31C(int mode, uint16_t addr, uint32_t data){
  printf("      VLAN Output Port Priority-Tagging Control Register 0, 1, 2, 3 \n");
}

void print_val_37C(int mode, uint16_t addr, uint32_t data){
  printf("      VLAN Tble Configuration Registers \n");
}

void print_val_37E(int mode, uint16_t addr, uint32_t data){
  printf("      Insert PerPort VID (PVID) Enabling Register \n");
}

void print_val_400(int mode, uint16_t addr, uint32_t data){
  printf("      QoS Control Register \n");
}

void print_val_401(int mode, uint16_t addr, uint32_t data){
  printf("      Port Priority Configuration Registers 0 \n");
}

void print_val_402(int mode, uint16_t addr, uint32_t data){
  printf("      Port Priority Configuration Registers 1 \n");
}

void print_val_500(int mode, uint16_t addr, uint32_t data){
  printf("      PHY Access Control Register \n");
}

void print_val_501(int mode, uint16_t addr, uint32_t data){
  printf("      PHY Access Write Data Register \n");
}

void print_val_502(int mode, uint16_t addr, uint32_t data){
  printf("      PHY Access Read Data Register \n");
}

void print_val_607(int mode, uint16_t addr, uint32_t data){
  printf("      Global Port Control Register \n");
}

void print_val_608(int mode, uint16_t addr, uint32_t data){
  printf("      Port Disable Control Register 0 \n");
}

void print_val_609(int mode, uint16_t addr, uint32_t data){
  printf("      Port Disable Control Register 1 \n");
}

void print_val_615(int mode, uint16_t addr, uint32_t data){
  printf("      Port Property Configuration Register 0 ~ 7 \n");
}

void print_val_624(int mode, uint16_t addr, uint32_t data){
  printf("      Port Link Status Register 0 ~ 7 \n");
}

void print_val_707(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter Object Selection Register 0~7 \n");
}

void print_val_70C(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter Object Selection Register 0~12 \n");
}

void print_val_71C(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter 1 Register (RX Counter) (32 bits) \n");
}

void print_val_724(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter 1 Register (RX Counter) (32 bits)\n");
}

void print_val_726(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter 1 Register (RX Counter) (32 bits)\n");
}

void print_val_736(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter 2 Register (TX Counter) (32-bits) \n");
}

void print_val_740(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter 2 Register (TX Counter) (32-bits) \n");
}

void print_val_750(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter 3 Register (Diagnostic Counter) (32-bits) \n");
}

void print_val_75A(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter 3 Register (Diagnostic Counter) (32-bits) \n");
}

void print_val_20B(int mode, uint16_t addr, uint32_t data){
  printf("      Port 2~15 Bandwidth Control Register \n");
}

void print_val_30C(int mode, uint16_t addr, uint32_t data){
  printf("      Port VLAN ID Assignment Index Register 0~7 \n");
}

void print_val_319(int mode, uint16_t addr, uint32_t data){
  printf("      VLAN Output Port Priority-Tagging Control Register 0, 1 \n");
}

void print_val_31D(int mode, uint16_t addr, uint32_t data){
  printf("      VLAN Table Configuration Registers \n");
}

void print_val_37D(int mode, uint16_t addr, uint32_t data){
  printf("      Insert PerPort VID (PVID) Enabling Register \n");
}

void print_val_60A(int mode, uint16_t addr, uint32_t data){
  printf("      Port Property Configuration Register 0 ~ 7 \n");
}

void print_val_619(int mode, uint16_t addr, uint32_t data){
  printf("      Port Link Status Register 0 ~ 7 \n");
}

void print_val_700(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter Object Selection Register 0~7 \n");
}

void print_val_70D(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter 1 Register (RX Counter) (32 bits) \n");
}

void print_val_727(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter 2 Register (TX Counter) (32-bits) \n");
}

void print_val_741(int mode, uint16_t addr, uint32_t data){
  printf("      Port MIB Counter 3 Register (Diagnostic Counter) (32-bits) \n");
}

void init_decoder(){
 int i;
 
 for (i=0;i<0x1000;i++){
   rrcp_decode[i]=(void *)print_reg;
 }
 rrcp_decode[0x000]=(void*)print_val_000;
 rrcp_decode[0x001]=(void*)print_val_001;
 rrcp_decode[0x002]=(void*)print_val_002;
 rrcp_decode[0x003]=(void*)print_val_003;
 rrcp_decode[0x004]=(void*)print_val_004;
 rrcp_decode[0x005]=(void*)print_val_005;
 rrcp_decode[0x100]=(void*)print_val_100;
 rrcp_decode[0x101]=(void*)print_val_101;
 rrcp_decode[0x102]=(void*)print_val_102;
 rrcp_decode[0x200]=(void*)print_val_200;
 rrcp_decode[0x201]=(void*)print_val_201;
 rrcp_decode[0x202]=(void*)print_val_202;
 rrcp_decode[0x203]=(void*)print_val_203;
 rrcp_decode[0x204]=(void*)print_val_204;
 rrcp_decode[0x205]=(void*)print_val_205;
 rrcp_decode[0x206]=(void*)print_val_206;
 rrcp_decode[0x207]=(void*)print_val_207;
 rrcp_decode[0x208]=(void*)print_val_208;
 rrcp_decode[0x209]=(void*)print_val_209;
 rrcp_decode[0x20A]=(void*)print_val_20A;
 rrcp_decode[0x20B]=(void*)print_val_20B;
 rrcp_decode[0x20C]=(void*)print_val_20B;
 rrcp_decode[0x20D]=(void*)print_val_20B;
 rrcp_decode[0x20E]=(void*)print_val_20B;
 rrcp_decode[0x20F]=(void*)print_val_20B;
 rrcp_decode[0x210]=(void*)print_val_20B;
 rrcp_decode[0x211]=(void*)print_val_20B;
 rrcp_decode[0x212]=(void*)print_val_20B;
 rrcp_decode[0x213]=(void*)print_val_20B;
 rrcp_decode[0x214]=(void*)print_val_20B;
 rrcp_decode[0x215]=(void*)print_val_20B;
 rrcp_decode[0x216]=(void*)print_val_20B;
 rrcp_decode[0x217]=(void*)print_val_217;
 rrcp_decode[0x218]=(void*)print_val_218;
 rrcp_decode[0x219]=(void*)print_val_219;
 rrcp_decode[0x21B]=(void*)print_val_21B;
 rrcp_decode[0x21D]=(void*)print_val_21D;
 rrcp_decode[0x21E]=(void*)print_val_21E;
 rrcp_decode[0x300]=(void*)print_val_300;
 rrcp_decode[0x301]=(void*)print_val_301;
 rrcp_decode[0x302]=(void*)print_val_302;
 rrcp_decode[0x307]=(void*)print_val_307;
 rrcp_decode[0x308]=(void*)print_val_308;
 rrcp_decode[0x309]=(void*)print_val_309;
 rrcp_decode[0x30B]=(void*)print_val_30B;
 rrcp_decode[0x30C]=(void*)print_val_30C;
 rrcp_decode[0x30D]=(void*)print_val_30C;
 rrcp_decode[0x30E]=(void*)print_val_30C;
 rrcp_decode[0x30F]=(void*)print_val_30C;
 rrcp_decode[0x310]=(void*)print_val_30C;
 rrcp_decode[0x311]=(void*)print_val_30C;
 rrcp_decode[0x312]=(void*)print_val_30C;
 rrcp_decode[0x313]=(void*)print_val_30C;
 rrcp_decode[0x314]=(void*)print_val_30C;
 rrcp_decode[0x315]=(void*)print_val_30C;
 rrcp_decode[0x316]=(void*)print_val_30C;
 rrcp_decode[0x317]=(void*)print_val_30C;
 rrcp_decode[0x318]=(void*)print_val_30C;
 rrcp_decode[0x319]=(void*)print_val_319;
 rrcp_decode[0x31A]=(void*)print_val_319;
 rrcp_decode[0x31B]=(void*)print_val_319;
 rrcp_decode[0x31C]=(void*)print_val_319;
 rrcp_decode[0x31D]=(void*)print_val_31D;
 rrcp_decode[0x31E]=(void*)print_val_31D;
 rrcp_decode[0x31F]=(void*)print_val_31D;
 rrcp_decode[0x320]=(void*)print_val_31D;
 rrcp_decode[0x321]=(void*)print_val_31D;
 rrcp_decode[0x322]=(void*)print_val_31D;
 rrcp_decode[0x323]=(void*)print_val_31D;
 rrcp_decode[0x324]=(void*)print_val_31D;
 rrcp_decode[0x325]=(void*)print_val_31D;
 rrcp_decode[0x326]=(void*)print_val_31D;
 rrcp_decode[0x327]=(void*)print_val_31D;
 rrcp_decode[0x328]=(void*)print_val_31D;
 rrcp_decode[0x329]=(void*)print_val_31D;
 rrcp_decode[0x32A]=(void*)print_val_31D;
 rrcp_decode[0x32B]=(void*)print_val_31D;
 rrcp_decode[0x32C]=(void*)print_val_31D;
 rrcp_decode[0x32D]=(void*)print_val_31D;
 rrcp_decode[0x32E]=(void*)print_val_31D;
 rrcp_decode[0x32F]=(void*)print_val_31D;
 rrcp_decode[0x330]=(void*)print_val_31D;
 rrcp_decode[0x331]=(void*)print_val_31D;
 rrcp_decode[0x332]=(void*)print_val_31D;
 rrcp_decode[0x333]=(void*)print_val_31D;
 rrcp_decode[0x334]=(void*)print_val_31D;
 rrcp_decode[0x335]=(void*)print_val_31D;
 rrcp_decode[0x336]=(void*)print_val_31D;
 rrcp_decode[0x337]=(void*)print_val_31D;
 rrcp_decode[0x338]=(void*)print_val_31D;
 rrcp_decode[0x339]=(void*)print_val_31D;
 rrcp_decode[0x33A]=(void*)print_val_31D;
 rrcp_decode[0x33B]=(void*)print_val_31D;
 rrcp_decode[0x33C]=(void*)print_val_31D;
 rrcp_decode[0x33D]=(void*)print_val_31D;
 rrcp_decode[0x33E]=(void*)print_val_31D;
 rrcp_decode[0x33F]=(void*)print_val_31D;
 rrcp_decode[0x340]=(void*)print_val_31D;
 rrcp_decode[0x341]=(void*)print_val_31D;
 rrcp_decode[0x342]=(void*)print_val_31D;
 rrcp_decode[0x343]=(void*)print_val_31D;
 rrcp_decode[0x344]=(void*)print_val_31D;
 rrcp_decode[0x345]=(void*)print_val_31D;
 rrcp_decode[0x346]=(void*)print_val_31D;
 rrcp_decode[0x347]=(void*)print_val_31D;
 rrcp_decode[0x348]=(void*)print_val_31D;
 rrcp_decode[0x349]=(void*)print_val_31D;
 rrcp_decode[0x34A]=(void*)print_val_31D;
 rrcp_decode[0x34B]=(void*)print_val_31D;
 rrcp_decode[0x34C]=(void*)print_val_31D;
 rrcp_decode[0x34D]=(void*)print_val_31D;
 rrcp_decode[0x34E]=(void*)print_val_31D;
 rrcp_decode[0x34F]=(void*)print_val_31D;
 rrcp_decode[0x350]=(void*)print_val_31D;
 rrcp_decode[0x351]=(void*)print_val_31D;
 rrcp_decode[0x352]=(void*)print_val_31D;
 rrcp_decode[0x353]=(void*)print_val_31D;
 rrcp_decode[0x354]=(void*)print_val_31D;
 rrcp_decode[0x355]=(void*)print_val_31D;
 rrcp_decode[0x356]=(void*)print_val_31D;
 rrcp_decode[0x357]=(void*)print_val_31D;
 rrcp_decode[0x358]=(void*)print_val_31D;
 rrcp_decode[0x359]=(void*)print_val_31D;
 rrcp_decode[0x35A]=(void*)print_val_31D;
 rrcp_decode[0x35B]=(void*)print_val_31D;
 rrcp_decode[0x35C]=(void*)print_val_31D;
 rrcp_decode[0x35D]=(void*)print_val_31D;
 rrcp_decode[0x35E]=(void*)print_val_31D;
 rrcp_decode[0x35F]=(void*)print_val_31D;
 rrcp_decode[0x360]=(void*)print_val_31D;
 rrcp_decode[0x361]=(void*)print_val_31D;
 rrcp_decode[0x362]=(void*)print_val_31D;
 rrcp_decode[0x363]=(void*)print_val_31D;
 rrcp_decode[0x364]=(void*)print_val_31D;
 rrcp_decode[0x365]=(void*)print_val_31D;
 rrcp_decode[0x366]=(void*)print_val_31D;
 rrcp_decode[0x367]=(void*)print_val_31D;
 rrcp_decode[0x368]=(void*)print_val_31D;
 rrcp_decode[0x369]=(void*)print_val_31D;
 rrcp_decode[0x36A]=(void*)print_val_31D;
 rrcp_decode[0x36B]=(void*)print_val_31D;
 rrcp_decode[0x36C]=(void*)print_val_31D;
 rrcp_decode[0x36D]=(void*)print_val_31D;
 rrcp_decode[0x36E]=(void*)print_val_31D;
 rrcp_decode[0x36F]=(void*)print_val_31D;
 rrcp_decode[0x370]=(void*)print_val_31D;
 rrcp_decode[0x371]=(void*)print_val_31D;
 rrcp_decode[0x372]=(void*)print_val_31D;
 rrcp_decode[0x373]=(void*)print_val_31D;
 rrcp_decode[0x374]=(void*)print_val_31D;
 rrcp_decode[0x375]=(void*)print_val_31D;
 rrcp_decode[0x376]=(void*)print_val_31D;
 rrcp_decode[0x377]=(void*)print_val_31D;
 rrcp_decode[0x378]=(void*)print_val_31D;
 rrcp_decode[0x379]=(void*)print_val_31D;
 rrcp_decode[0x37A]=(void*)print_val_31D;
 rrcp_decode[0x37B]=(void*)print_val_31D;
 rrcp_decode[0x37C]=(void*)print_val_31D;
 rrcp_decode[0x37D]=(void*)print_val_37D;
 rrcp_decode[0x37E]=(void*)print_val_37D;
 rrcp_decode[0x400]=(void*)print_val_400;
 rrcp_decode[0x401]=(void*)print_val_401;
 rrcp_decode[0x402]=(void*)print_val_402;
 rrcp_decode[0x500]=(void*)print_val_500;
 rrcp_decode[0x501]=(void*)print_val_501;
 rrcp_decode[0x502]=(void*)print_val_502;
 rrcp_decode[0x607]=(void*)print_val_607;
 rrcp_decode[0x608]=(void*)print_val_608;
 rrcp_decode[0x609]=(void*)print_val_609;
 rrcp_decode[0x60A]=(void*)print_val_60A;
 rrcp_decode[0x60B]=(void*)print_val_60A;
 rrcp_decode[0x60C]=(void*)print_val_60A;
 rrcp_decode[0x60D]=(void*)print_val_60A;
 rrcp_decode[0x60E]=(void*)print_val_60A;
 rrcp_decode[0x60F]=(void*)print_val_60A;
 rrcp_decode[0x610]=(void*)print_val_60A;
 rrcp_decode[0x611]=(void*)print_val_60A;
 rrcp_decode[0x612]=(void*)print_val_60A;
 rrcp_decode[0x613]=(void*)print_val_60A;
 rrcp_decode[0x614]=(void*)print_val_60A;
 rrcp_decode[0x615]=(void*)print_val_60A;
 rrcp_decode[0x619]=(void*)print_val_619;
 rrcp_decode[0x61A]=(void*)print_val_619;
 rrcp_decode[0x61B]=(void*)print_val_619;
 rrcp_decode[0x61C]=(void*)print_val_619;
 rrcp_decode[0x61D]=(void*)print_val_619;
 rrcp_decode[0x61E]=(void*)print_val_619;
 rrcp_decode[0x61F]=(void*)print_val_619;
 rrcp_decode[0x620]=(void*)print_val_619;
 rrcp_decode[0x621]=(void*)print_val_619;
 rrcp_decode[0x622]=(void*)print_val_619;
 rrcp_decode[0x623]=(void*)print_val_619;
 rrcp_decode[0x624]=(void*)print_val_619;
 rrcp_decode[0x700]=(void*)print_val_700;
 rrcp_decode[0x701]=(void*)print_val_700;
 rrcp_decode[0x702]=(void*)print_val_700;
 rrcp_decode[0x703]=(void*)print_val_700;
 rrcp_decode[0x704]=(void*)print_val_700;
 rrcp_decode[0x705]=(void*)print_val_700;
 rrcp_decode[0x706]=(void*)print_val_700;
 rrcp_decode[0x707]=(void*)print_val_700;
 rrcp_decode[0x708]=(void*)print_val_700;
 rrcp_decode[0x709]=(void*)print_val_700;
 rrcp_decode[0x70A]=(void*)print_val_700;
 rrcp_decode[0x70B]=(void*)print_val_700;
 rrcp_decode[0x70C]=(void*)print_val_700;
 rrcp_decode[0x70D]=(void*)print_val_70D;
 rrcp_decode[0x70E]=(void*)print_val_70D;
 rrcp_decode[0x70F]=(void*)print_val_70D;
 rrcp_decode[0x710]=(void*)print_val_70D;
 rrcp_decode[0x711]=(void*)print_val_70D;
 rrcp_decode[0x712]=(void*)print_val_70D;
 rrcp_decode[0x713]=(void*)print_val_70D;
 rrcp_decode[0x714]=(void*)print_val_70D;
 rrcp_decode[0x715]=(void*)print_val_70D;
 rrcp_decode[0x716]=(void*)print_val_70D;
 rrcp_decode[0x717]=(void*)print_val_70D;
 rrcp_decode[0x718]=(void*)print_val_70D;
 rrcp_decode[0x719]=(void*)print_val_70D;
 rrcp_decode[0x71A]=(void*)print_val_70D;
 rrcp_decode[0x71B]=(void*)print_val_70D;
 rrcp_decode[0x71C]=(void*)print_val_70D;
 rrcp_decode[0x71D]=(void*)print_val_70D;
 rrcp_decode[0x71E]=(void*)print_val_70D;
 rrcp_decode[0x71F]=(void*)print_val_70D;
 rrcp_decode[0x720]=(void*)print_val_70D;
 rrcp_decode[0x721]=(void*)print_val_70D;
 rrcp_decode[0x722]=(void*)print_val_70D;
 rrcp_decode[0x723]=(void*)print_val_70D;
 rrcp_decode[0x724]=(void*)print_val_70D;
 rrcp_decode[0x725]=(void*)print_val_70D;
 rrcp_decode[0x726]=(void*)print_val_726;
 rrcp_decode[0x727]=(void*)print_val_727;
 rrcp_decode[0x728]=(void*)print_val_727;
 rrcp_decode[0x729]=(void*)print_val_727;
 rrcp_decode[0x72A]=(void*)print_val_727;
 rrcp_decode[0x72B]=(void*)print_val_727;
 rrcp_decode[0x72C]=(void*)print_val_727;
 rrcp_decode[0x72D]=(void*)print_val_727;
 rrcp_decode[0x72E]=(void*)print_val_727;
 rrcp_decode[0x72F]=(void*)print_val_727;
 rrcp_decode[0x730]=(void*)print_val_727;
 rrcp_decode[0x731]=(void*)print_val_727;
 rrcp_decode[0x732]=(void*)print_val_727;
 rrcp_decode[0x733]=(void*)print_val_727;
 rrcp_decode[0x734]=(void*)print_val_727;
 rrcp_decode[0x735]=(void*)print_val_727;
 rrcp_decode[0x736]=(void*)print_val_727;
 rrcp_decode[0x737]=(void*)print_val_727;
 rrcp_decode[0x738]=(void*)print_val_727;
 rrcp_decode[0x739]=(void*)print_val_727;
 rrcp_decode[0x73A]=(void*)print_val_727;
 rrcp_decode[0x73B]=(void*)print_val_727;
 rrcp_decode[0x73C]=(void*)print_val_727;
 rrcp_decode[0x73D]=(void*)print_val_727;
 rrcp_decode[0x73E]=(void*)print_val_727;
 rrcp_decode[0x73F]=(void*)print_val_727;
 rrcp_decode[0x740]=(void*)print_val_727;
 rrcp_decode[0x741]=(void*)print_val_741;
 rrcp_decode[0x742]=(void*)print_val_741;
 rrcp_decode[0x743]=(void*)print_val_741;
 rrcp_decode[0x744]=(void*)print_val_741;
 rrcp_decode[0x745]=(void*)print_val_741;
 rrcp_decode[0x746]=(void*)print_val_741;
 rrcp_decode[0x747]=(void*)print_val_741;
 rrcp_decode[0x748]=(void*)print_val_741;
 rrcp_decode[0x749]=(void*)print_val_741;
 rrcp_decode[0x74A]=(void*)print_val_741;
 rrcp_decode[0x74B]=(void*)print_val_741;
 rrcp_decode[0x74C]=(void*)print_val_741;
 rrcp_decode[0x74D]=(void*)print_val_741;
 rrcp_decode[0x74E]=(void*)print_val_741;
 rrcp_decode[0x74F]=(void*)print_val_741;
 rrcp_decode[0x750]=(void*)print_val_741;
 rrcp_decode[0x751]=(void*)print_val_741;
 rrcp_decode[0x752]=(void*)print_val_741;
 rrcp_decode[0x753]=(void*)print_val_741;
 rrcp_decode[0x754]=(void*)print_val_741;
 rrcp_decode[0x755]=(void*)print_val_741;
 rrcp_decode[0x756]=(void*)print_val_741;
 rrcp_decode[0x757]=(void*)print_val_741;
 rrcp_decode[0x758]=(void*)print_val_741;
 rrcp_decode[0x759]=(void*)print_val_741;
 rrcp_decode[0x75A]=(void*)print_val_741;
 return;
}

