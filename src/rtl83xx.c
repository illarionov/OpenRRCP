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
#include <netinet/in.h>

#ifndef __linux__
#include <sys/socket.h>
#include <net/if.h>
#endif

#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#include "rrcp_packet.h"
#include "rrcp_io.h"
#include "rrcp_lib.h"
#include "rrcp_config.h"
#include "rrcp_switches.h"

#define OPENRRCP_VERSION  "0.2.2"

#define DEFAULT_CAPTURE_MAC_TIME_SEC 30

int myPid = 0;
extern char ErrIOMsg[];
char ErrMsg[512];
extern int out_xml;

static int break_capture_mac;

void sigHandler(int sig)
{
    printf("Timeout! (no switch with rtl83xx chip found?)\n");
    kill( myPid, SIGKILL );
    exit( 0 );
}

void sigMacCaptHandler(int sig)
{
   break_capture_mac = 1;
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

/*
 * PARAMETERS:
 *     idx_list - vlan index entry list. i.e. "1,2,5-10"
 *     max_cnt  - maximum allowed values in list. 0 - unlimited
 * RETURN VALUES:
 *    < 0 - on error, error in ErrMsg
 *    first value - on success
 */
static int check_vlan_entry_index_list(const char *idx_list, int max_cnt)
{
   int idx;
   int cnt;
   int first;
   struct t_str_number_list list;

   if ((idx_list == NULL)
	 || (idx_list[0] == '\0')) {
      snprintf(ErrMsg, sizeof(ErrMsg), "Incorrect VLAN index\n");
      return -1;
   }

   cnt = str_number_list_init(idx_list, &list);
   if ((cnt <= 0) || (max_cnt && (cnt > max_cnt))) {
      snprintf(ErrMsg, sizeof(ErrMsg),
	    "Incorrect VLAN index: %s\n", idx_list);
      return -2;
   }

   first = -1;
   for (cnt=0; str_number_list_get_next(&list, &idx) == 0; cnt++) {
      if (idx < 0 || idx > 31) {
	 snprintf(ErrMsg, sizeof(ErrMsg),
	       "Incorrect VLAN index: %i\n"
	       "This hardware can store only 32 indexes (0-31).\n"
	       ,idx);
	 return -3;
      }
      if (cnt == 0)
	 first = idx;
   }

   return first;
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

void print_port_link_status_xml(int port_no, int enabled, unsigned char encoded_status, int loopdetect){
    struct t_rtl83xx_port_link_status stat;

    memcpy(&stat,&encoded_status,1);
    printf("    <ifEntry ifIndex=\"%d\" ",port_no);
    printf("ifAdminStatus=\"%s\" ",enabled ? "1" : "2");
    if (stat.link){
        switch (stat.speed){
        case 0:
            printf("ifSpeed=\"10000000\" ");
            break;
        case 1:
            printf("ifSpeed=\"100000000\" ");
            break;
        case 2:
            printf("ifSpeed=\"1000000000\" ");
            break;
        case 3:
            printf("ifSpeed=\"0\" ");
            break;
        }
    } else {
        printf("ifSpeed=\"0\" ");
    }
    printf("ifOperStatus=\"%s\" ",stat.link ? "1" : "2");

    switch(loopdetect){
        case 2:
            printf("ifLoopbackStatus=\"2\" ");
            break;
        case 1:
            printf("ifLoopbackStatus=\"1\" ");
            break;
        default:
            break;
    }
    printf("/>\n");
}

void print_link_status_xml(unsigned short int *arr){
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

    printf("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
    printf("<ifTable>\n");
    for(i=1;i<=switchtypes[switchtype].num_ports;i++){
        if (arr) { if (!*(arr+i-1)){ continue;} }
    print_port_link_status_xml(
        i,
        !((u.signlelong>>(map_port_number_from_logical_to_physical(i)))&1),
        r.ch[map_port_number_from_logical_to_physical(i)],
                (EnLoopDet)?((port_loop_status>>(map_port_number_from_logical_to_physical(i)))&0x1)+1:0);
    }
    printf("</ifTable>\n");
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

void print_counters_crc(unsigned short int *arr){
    int i,port_tr;
    for (i=0;i<switchtypes[switchtype].num_ports/2;i++){
    rtl83xx_setreg16(0x0700+i,0x0820);//read rx byte, tx byte, CRC error packet
    }
    printf("port              RX          TX        CRC\n");
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

void print_router_port(unsigned short int *arr){
    int i,port_tr;
    unsigned long reg_val;
    reg_val = (unsigned long)rtl83xx_readreg32(0x0309);
    printf("port          Status\n");
    for (i=1;i<=switchtypes[switchtype].num_ports;i++){
        if (arr) { if (!*(arr+i-1)){ continue;} }
        port_tr=map_port_number_from_logical_to_physical(i);
        if ((reg_val >> port_tr) & 0x0001) {
            printf("%s/%-2d: Router port\n", ifname,i);
        } else {
            printf("%s/%-2d: Normal port\n", ifname,i);
        }
    }
    return;
}

void print_cable_diagnostics(unsigned short int *arr) {
   uint8_t fport;
   unsigned i, port_tr;
   struct cable_diagnostic_result res;
   char s[20];

   if (rrcp_io_probe_switch_for_facing_switch_port(dest_mac, (uint8_t *)&fport) < 0) {
      printf("Switch not responded\n");
      return;
   }

   printf("PORT        PAIR_1TO2_RX 	 PAIR_3TO6_TX\n");
    for (i=1;i<=switchtypes[switchtype].num_ports;i++){
        if ((arr != NULL)
	   && (arr[i-1] == 0))
	continue;

	port_tr=map_port_number_from_logical_to_physical(i);
	printf("%s/%-2d: ", ifname, i);
	if (port_tr == fport)
	   puts("Facing port");
	else if (cable_diagnostic(port_tr, &res) < 0)
	   puts("The PHY can't support Cable Diagnostic");
	 else {
	    if (res.pair1to2_status <= 0)
	       printf("%-20s", cablestatus2str(res.pair1to2_status));
	    else {
	       snprintf(s, sizeof(s), "%s at %d.%d m.",
		     cablestatus2str(res.pair1to2_status),
		     res.pair1to2_distance_025m / 4,
		     (res.pair1to2_distance_025m % 4) * 25);
	       printf("%-20s", s);
	    }
	    putchar(' ');
	    if (res.pair3to6_status <= 0)
	       printf("%-20s", cablestatus2str(res.pair3to6_status));
	    else {
	       snprintf(s, sizeof(s), "%s at %d.%d m.",
		     cablestatus2str(res.pair3to6_status),
		     res.pair3to6_distance_025m / 4,
		     (res.pair3to6_distance_025m % 4) * 25);
	       printf("%-20s", s);
	    }
	    putchar('\n');
	 }
    }
    return;
}

void print_cable_diagnostics_xml(unsigned short int *arr) {
   uint8_t fport;
   unsigned i, port_tr;
   struct cable_diagnostic_result res;

   if (rrcp_io_probe_switch_for_facing_switch_port(dest_mac, (uint8_t *)&fport) < 0) {
      printf("Switch not responded\n");
      return;
   }

   printf("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
   printf("<cableDiagTable>\n");
    for (i=1;i<=switchtypes[switchtype].num_ports;i++){
        if ((arr != NULL)
       && (arr[i-1] == 0))
    continue;

    port_tr=map_port_number_from_logical_to_physical(i);
    printf("    <cableDiagEntry PortIndex=\"%d\" PortType=\"fastEthernet\" ", i);
    if (port_tr == fport)
       printf("Pair2Status=\"facing\" Pair2Length=\"0\" Pair3Status=\"facing\" Pair3Length=\"0\" />\n");
    else if (cable_diagnostic(port_tr, &res) < 0)
       printf("Pair2Status=\"other\" Pair2Length=\"0\" Pair3Status=\"other\" Pair3Length=\"0\" />\n");
     else {
        printf("Pair2Status=\"%s\" ", cablestatus2str(res.pair1to2_status));
        if (res.pair1to2_status <= 0)
            printf("Pair2Length=\"0\" ");
        else {
            printf("Pair2Length=\"%d.%d\" ", res.pair1to2_distance_025m / 4, (res.pair1to2_distance_025m % 4) * 25);
        }
        printf("Pair3Status=\"%s\" ", cablestatus2str(res.pair3to6_status));
        if (res.pair3to6_status <= 0)
           printf("Pair3Length=\"0\" ");
        else {
           printf("Pair3Length=\"%d.%d\" ", res.pair3to6_distance_025m / 4, (res.pair3to6_distance_025m % 4) * 25);
        }
        printf("/>\n");
     }
    }
    printf("</cableDiagTable>\n");
    return;
}

void do_ping(void){
    int r=rtl83xx_ping(0,0);
	printf("%02x:%02x:%02x:%02x:%02x:%02x %sresponded\n",
			dest_mac[0],
			dest_mac[1],
			dest_mac[2],
			dest_mac[3],
			dest_mac[4],
			dest_mac[5],
			r>0 ? "":"not ");
    fflush(stdout);
    _exit(r<=0);
}

static void print_port_phy_autoneg_advertisement(
      uint16_t val,
      const char *header)
{
   printf("%s:%s%s%s%s%s%s%s%s%s%s\n",
	 header,
	 val & 0x8000 ? " NextPage" : "",
	 val & 0x4000 ? " Ack" : "",
	 val & 0x2000 ? " RemoteFault" : "",
	 val & 0x0800 ? " AsymmetricFlowControl" : "",
	 val & 0x0400 ? " FlowControl" : "",
	 val & 0x0200 ? " 100Base-T4" : "",
	 val & 0x0100 ? " 100BaseTX-FD" : "",
	 val & 0x0080 ? " 100Base-TX" : "",
	 val & 0x0040 ? " 10Base-T-FD" : "",
	 val & 0x0020 ? " 10Base-T" : ""
	 );
}

void print_port_phy_status(const unsigned short int *arr) {
   unsigned i;
   uint16_t r0, r1, r2, r3, r4, r5, r6;
   unsigned autoneg_completed;

   for (i=1;i<=switchtypes[switchtype].num_ports;i++){
      int phy_port;

      phy_port = map_port_from_logical_to_phy(i);
      if ((phy_port < 0)
	    || ((arr != NULL) && (arr[i-1] == 0)))
	 continue;

      printf("Interface %s/%-2d:\n", ifname, i);

       /* PHY identifier 1, 2 */
      if ((phy_read(phy_port, 2, &r2) != 0)
	    || (phy_read(phy_port, 3, &r3) != 0)) {
	 printf("PHY timeout\n");
	 continue;
      }

      if ((r2 != 0x001c) || (r3 != 0xc881)) {
	 uint8_t oui[3];
	 oui[0] = (r2 & 0x003f) << 2;
	 oui[1] = (r2 & 0x3fc0) >> 6;
	 oui[2] = ((r2 & 0xc000) >> 14) | ((r3 & 0xfc00)>>8);
	 /* oui = (t2<<2)|((t3 & 0xfc00)<<8); */
	 printf("Unsupported PHY. OUI: %02hhX-%02hhX-%02hhX. Model: %02hhX, "
	       "revision: %u \n",
	       oui[2], oui[1], oui[0],
	       (r3 >> 4) & 0xcf,
	       r3 & 0x000f);
	 continue;
      }

      if ((phy_read(phy_port, 0, &r0) != 0)
	   || (phy_read(phy_port, 1, &r1) != 0)
	   || (phy_read(phy_port, 4, &r4) != 0)
	   || (phy_read(phy_port, 5, &r5) != 0)
	   || (phy_read(phy_port, 6, &r6) != 0)
	    ) {
	 printf("PHY timeout\n");
	 continue;
      }

      /* PHY Status register */
      autoneg_completed = r1 & 0x20;

      /* PHY Control Register */
      printf(
	    "   Power Down / Isolate / TXD to RXD Loopback: %s / %s / %s\n"
	    "   Auto-negotiation: %s, %s, %s-%s\n",
	    r0 & 0x0800 ? "yes" : "no", /* Power down */
	    r0 & 0x0400 ? "yes" : "no", /* Isolate */
	    r0 & 0x4000 ? "yes" : "no", /* Loopback */
	    r0 & 0x1000 ? "enabled" : "disabled", /* Autoneg */
	    autoneg_completed ? "completed" : "not completed",
	    r0 & 0x2000 ? "100Mbps" : "10Mbps", /* Autoneg result speed*/
	       r0 & 0x0100 ? "FD" : "HD" /* Autoneg result duplex */
	    );

      if (autoneg_completed) {
	 /* PHY Auto-Negotiation Advertisement */
	 print_port_phy_autoneg_advertisement(r4, "   Local auto-negotiation advertisement");
	 /* PHY Auto-Negotiation Link Partner Ability register*/
	 print_port_phy_autoneg_advertisement(r5, "   Remote auto-negotiation advertisement");
	 /* PHY Auto-Negotiation Expansion */
	 printf("   Remote auto-negotiation able: %s\n", r6 & 0x001 ? "yes" : "no");
      }
   } /* for */

   return;
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

void do_vlan_leaky(int mode,int state)
{
    swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
    switch (mode){
        case 1: // arp
            swconfig.vlan.s.config.arp_leaky = state;
            break;
        case 2: // multicast
            swconfig.vlan.s.config.multicast_leaky = state;
            break;
        case 3: // unicast
            swconfig.vlan.s.config.unicast_leaky = state;
            break;
    }
    rtl83xx_setreg16(0x030b,swconfig.vlan.raw[0]);
}

void do_vlan_drop(int mode,int state)
{
    swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
    switch (mode){
        case 1: // untagged_frames
            swconfig.vlan.s.config.drop_untagged_frames = state;
        case 2: // invalid_vid
            swconfig.vlan.s.config.ingress_filtering = state;
    }
    rtl83xx_setreg16(0x030b,swconfig.vlan.raw[0]);
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

void do_port_phy_disable(unsigned short int *arr,unsigned short int val){
   unsigned i;
   uint16_t t1, t2;

   for(i=1;i<=switchtypes[switchtype].num_ports;i++){
      int phy_port;

      phy_port = map_port_from_logical_to_phy(i);
      if ((phy_port < 0)
	    || (arr[i-1] == 0))
	 continue;

      if (phy_read(phy_port, 0, &t1) != 0)
	 continue;

      if (val)
	 t2 = t1 & (uint16_t)0xf7ff; /* normal operation */
      else
	 t2 = t1 | (uint16_t)0x0800; /* power down */

      if (t2 != t1)
	 phy_write(phy_port, 0, t2);
   }

   return;
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

static int do_vlan_table_config(unsigned short int *arr, const char *idx_list, unsigned short int val)
{
   int idx;
   struct t_str_number_list list;

   if (check_vlan_entry_index_list(idx_list, 0) < 0)
      return -1;

   swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
   if (!swconfig.vlan.s.config.dot1q) {
      printf("WARNING: vlan dot1q mode not enabled\n");
   }

   str_number_list_init(idx_list, &list);
   while (str_number_list_get_next(&list, &idx) == 0)
	do_32bit_reg_action(arr,val,0x31d+3*idx);

   return 0;
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

static int do_vlan_index_config(const char *idx_list, const char *vid)
{
   unsigned u_vid;
   int idx;
   char *endptr;
   struct t_str_number_list list;

   /* Parse index list */
   if (check_vlan_entry_index_list(idx_list, 0) < 0)
      return -1;

   /* Parse VID */
   if ((vid == NULL)
	 || (vid[0] == '\0')) {
      snprintf(ErrMsg, sizeof(ErrMsg), "Incorrect VID\n");
      return -2;
   }

   u_vid = (unsigned)strtoul(vid, &endptr, 10);
   if ((endptr[0] != '\0')
	 || (u_vid > 0xfff)) {
      snprintf(ErrMsg, sizeof(ErrMsg),
	    "Incorrect VID: \"%s\"\n"
	    "VID can be only between 0-4095. "
	    "Values 0, 1 and 4095 are not recommended.\n"
	    ,vid);
      return -2;
   }

   if ((u_vid == 0) || (u_vid == 1) || (u_vid == 0xfff)) {
      printf("WARNING: VID %d is not recommended.\n", u_vid);
   }

   swconfig.vlan.raw[0]=rtl83xx_readreg16(0x030b);
   if (!swconfig.vlan.s.config.dot1q) {
      printf("WARNING: vlan dot1q mode not enabled\n");
   }

   str_number_list_init(idx_list, &list);
   while (str_number_list_get_next(&list, &idx) == 0)
      rtl83xx_setreg16(0x031d+3*idx+2, u_vid & 0xfff);

   return 0;
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

void do_mls_qos_trust(unsigned dscp, unsigned cos, unsigned mask){
   unsigned changed;

   if ((mask & 0x03) == 0)
      return;

   dscp = dscp ? 1 : 0;
   cos = cos ? 1 : 0;
   changed = 0;

   swconfig.qos_config.raw=rtl83xx_readreg16(0x0400);
   if ((mask & 0x01)
	 && (swconfig.qos_config.config.dscp_enable != dscp)) {
      swconfig.qos_config.config.dscp_enable = dscp;
      changed = 1;
   }
   if ((mask & 0x02)
	 && (swconfig.qos_config.config.cos_enable != cos)) {
      swconfig.qos_config.config.cos_enable = cos;
      changed = 1;
   }

   if (changed)
      rtl83xx_setreg16(0x0400,swconfig.qos_config.raw);
}

int do_wrr_queue(const char *ratio) {
   unsigned r;

   if (ratio == NULL)
      return -1;
   else if (strcmp("4:1",ratio)==0)
      r = 0;
   else if (strcmp("8:1",ratio)==0)
      r = 1;
   else if (strcmp("16:1",ratio)==0)
      r = 2;
   else if (strcmp("1:0",ratio)==0)
      r = 3;
   else
      return -1;

   swconfig.qos_config.raw=rtl83xx_readreg16(0x0400);
   if (swconfig.qos_config.config.wrr_ratio != r) {
      swconfig.qos_config.config.wrr_ratio = r;
      rtl83xx_setreg16(0x0400,swconfig.qos_config.raw);
      return 1;
   }

   return 0;
}

int do_show_version() {
   unsigned numports;
   int8_t fport;
   uint8_t detected_switch_t, chip_t;
   t_eeprom_type eeprom;
   char s[32];

   if (rrcp_io_probe_switch_for_facing_switch_port(dest_mac, (uint8_t *)&fport) < 0) {
      printf("Switch not responded\n");
      return -1;
   }

   eeprom = 0;
   rrcp_autodetect_switch_chip_eeprom(&detected_switch_t, &chip_t, &eeprom);

   if (switchtype != detected_switch_t) {
      if (switchtype < 0 || switchtype >= switchtype_n)
	 switchtype = detected_switch_t;

      if (switchtypes[switchtype].chip_id != switchtypes[detected_switch_t].chip_id) 
	 printf("WARNING: detected different chipset: %s\n",
	       chipnames[switchtypes[detected_switch_t].chip_id] );

      if (switchtypes[switchtype].num_ports!=switchtypes[detected_switch_t].num_ports)
	 printf("WARNING: detected different number of ports: %u\n",
	       switchtypes[detected_switch_t].num_ports);
   }

   numports = switchtypes[switchtype].num_ports;
   if (fport < 0 || fport >= numports) {
      fport = -1;
      s[0] = '?',s[1]='\0';
   }else {
      rrcp_config_get_portname(s, sizeof(s),
	 map_port_number_from_physical_to_logical(fport),
	 fport);
   }


   printf("OpenRRCP, Version %s\n"
	  "http://openrrcp.org.ru/\n"
	  "Licensed under terms of GPL\n"
	  "http://www.gnu.org/licenses/gpl.html#SEC1\n",
	  OPENRRCP_VERSION
	  );

   printf("\n%d FastEthernet/IEEE 802.3 interface(s)\n",
	 numports > 24 ? 24 : numports);
   if (numports > 24)
      printf("%d Gigabit Ethernet/IEEE 802.3 interface(s)\n", numports - 24);

   printf("\nBase ethernet MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n"
	  "Vendor: %s\n"
	  "Model: %s\n"
	  "Chip: %s\n"
	  "Description: %s\n"
	  "EEPROM: %s\n"
	  "802.1Q support: %s\n"
	  "IGMP support: %s\n"
	  "Facing host interface: %s\n"
	  "Facing switch interface: %s\n",
	  dest_mac[0], dest_mac[1], dest_mac[2],
	  dest_mac[3], dest_mac[4], dest_mac[5],
	  switchtypes[switchtype].vendor,
	  switchtypes[switchtype].model,
	  chipnames[switchtypes[switchtype].chip_id],
	  switchtypes[switchtype].description,
	  eeprom_type_text[eeprom],
	  (switchtypes[switchtype].chip_id==rtl8326) ? "No/Buggy" : "Yes",
	  (switchtypes[switchtype].chip_id==rtl8326) ? "v1" : "v1, v2",
	  ifname,
	  s
	 );

   return 0;
}

void do_igmp_snooping(int is_enabled){
    swconfig.alt_igmp_snooping.raw=rtl83xx_readreg16(0x0308);
    swconfig.alt_igmp_snooping.config.en_igmp_snooping=(is_enabled!=0)&0x1;
    rtl83xx_setreg16(0x0308,swconfig.alt_igmp_snooping.raw);
}

int do_capture_mac_address(const char *iface_list, int time_sec)
{
   uint8_t fport;
   uint16_t alt_conf_300, learn_ctrl_301, learn_ctrl_302;
   uint16_t new_alt_conf_300, new_learn_ctrl_301, new_learn_ctrl_302;
   uint32_t old_macs[1024][3];
   int old_macs_count, i;
   uint32_t tmp;
   uint32_t reg302_mask;
   unsigned cnt;
   int ifacenum;
   struct timeval starttime, curtime, maxtime;
   struct t_str_number_list list;

   cnt = str_number_list_init(iface_list, &list);
   if (cnt <= 0 || cnt > 30) {
      snprintf(ErrMsg, sizeof(ErrMsg),
	    "Incorrect Interface list: %s\n", iface_list);
      return -2;
   }

   for (; str_number_list_get_next(&list, &ifacenum) == 0;) {
      if (ifacenum < 1 || ifacenum > switchtypes[switchtype].num_ports) {
	 snprintf(ErrMsg, sizeof(ErrMsg),
	       "Mailformed interface number: %i in interface list: %s\n",
	       ifacenum, iface_list);
	 return -1;
      }
   }

   if (rrcp_io_probe_switch_for_facing_switch_port(dest_mac, (uint8_t *)&fport) < 0) {
      snprintf(ErrMsg, sizeof(ErrMsg), "Switch not responded\n");
      return -1;
   }

   fputs("Reading address lookup table configuration registers\n", stdout);
   reg302_mask = switchtypes[switchtype].num_ports > 16;
   if ( rtl83xx_readreg32_(0x0300, &tmp)) {
      snprintf(ErrMsg, sizeof(ErrMsg),
	    "Cannot read address learning control register %x\n", 0x0300);
      return -1;
   }
   alt_conf_300  = (uint16_t)tmp;

   if ( rtl83xx_readreg32_(0x0301, &tmp)) {
      snprintf(ErrMsg, sizeof(ErrMsg),
	    "Cannot read address learning control register %x\n", 0x0301);
      return -1;
   }
   learn_ctrl_301 = (uint16_t)tmp;

   if (reg302_mask) {
      if (rtl83xx_readreg32_(0x0302, &tmp)) {
	 snprintf(ErrMsg, sizeof(ErrMsg),
	       "Cannot read address learning control register %x\n", 0x0302);
	 return -1;
      }
      learn_ctrl_302 = (uint16_t)tmp;
   }else
      learn_ctrl_302 = 0;

   /* Signals */
   break_capture_mac=0;
   signal(SIGHUP, sigMacCaptHandler);
   signal(SIGINT, sigMacCaptHandler);
   signal(SIGTERM, sigMacCaptHandler);

   printf("Turning off mac-learning on interfaces: %s. "
	 "Turning on 12 seconds MAC aging\n", iface_list);

   tmp = 0;
   str_number_list_init(iface_list, &list);
   for (; str_number_list_get_next(&list, &ifacenum) == 0;) {
      int f;
      f = map_port_number_from_logical_to_physical(ifacenum);
      if (f == fport)
	 printf("Skipping facing port %i\n", ifacenum);
      else
	 tmp |= (uint32_t)1 << f;
   }

   new_learn_ctrl_301 = tmp & 0xffff;
   new_learn_ctrl_302 = (tmp >> 16) & 0xffff;
   new_alt_conf_300 = (alt_conf_300 & 0xfffe) | 0x02;

   rtl83xx_setreg16(0x0300,new_alt_conf_300);
   rtl83xx_setreg16(0x0301,new_learn_ctrl_301);
   if (reg302_mask && !break_capture_mac)
      rtl83xx_setreg16(0x0302,new_learn_ctrl_302);

   /* Reset status */
   rtl83xx_readreg32_(0x0306, &tmp);
   if (break_capture_mac) goto restore_capture_status;
   rtl83xx_readreg32_(0x0305, &tmp);
   if (break_capture_mac) goto restore_capture_status;
   rtl83xx_readreg32_(0x0304, &tmp);
   if (break_capture_mac) goto restore_capture_status;
   rtl83xx_readreg32_(0x0303, &tmp);
   if (break_capture_mac) goto restore_capture_status;

   printf("Capturing mac addresses for %u seconds\n", time_sec);
   if (gettimeofday(&starttime, NULL) < 0)
       goto restore_capture_status;

   maxtime = starttime;
   maxtime.tv_sec += time_sec;

   engage_timeout(time_sec+5);

   memset(old_macs, 0xff, sizeof(old_macs));
   for (old_macs_count = 0; (break_capture_mac==0) && (old_macs_count < 1024); usleep(300000)) {
      unsigned if2;
      uint32_t unk_sa_status;
      uint32_t new_mac[3];

      if (gettimeofday(&curtime, NULL) < 0)
	  break;
      if (timercmp(&curtime, &maxtime, >=)) /* Timeout */
	 break;

      if (rtl83xx_readreg32_(0x0306, &unk_sa_status)){
	 printf(
	       "Cannot read Unknown SA capture status register\n");
	 continue;
      }

      /*
       * Bit 5: 0 - Idle (old status),
       * 1 - New unknown SA detected (autocleared)
       *
       */
      if ((unk_sa_status & 0x20) == 0)
	 continue;

      /* Read sequence: 0x0305->0x0304->0x0303 */
      if (rtl83xx_readreg32_(0x0305, &new_mac[2])
	    || rtl83xx_readreg32_(0x0304, &new_mac[1])
	    || rtl83xx_readreg32_(0x0303, &new_mac[0])) {
	 printf("Cannot read Unknown SA capture registers\n");
	 continue;
      }

      for (i = 0; i < old_macs_count; i++) {
          if ((old_macs[i][0] == new_mac[0])
                && (old_macs[i][1] == new_mac[1])
                && (old_macs[i][2] == new_mac[2]))
          {
              break;
          }
      }
      if (i != old_macs_count) continue;

      if2 = map_port_number_from_physical_to_logical(unk_sa_status & 0x1f);
      printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x Port: %u\n",
	       new_mac[0] & 0xff,
	       new_mac[0] >> 8 & 0xff,
	       new_mac[1] & 0xff,
	       new_mac[1] >> 8 & 0xff,
	       new_mac[2] & 0xff,
	       new_mac[2] >> 8 & 0xff,
	       if2
	       );
      old_macs[old_macs_count][0] = new_mac[0];
      old_macs[old_macs_count][1] = new_mac[1];
      old_macs[old_macs_count][2] = new_mac[2];
      old_macs_count++;
   } /* for */

restore_capture_status:
   signal(SIGHUP, SIG_DFL);
   signal(SIGINT, SIG_DFL);
   signal(SIGTERM, SIG_DFL);
   fputs("Restoring mac-learning configuration\n", stdout);
   rtl83xx_setreg16(0x0300,alt_conf_300);
   rtl83xx_setreg16(0x0301,learn_ctrl_301);
   if (reg302_mask)
      rtl83xx_setreg16(0x0302,learn_ctrl_302);

   return 0;
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
   printf("Usage: rtl8316b [[authkey-]xx:xx:xx:xx:xx:xx@]if-name <command> [<argument>]\n"
	 "       rtl8324 ----\"\"----\n"
	 "       rtl8326 ----\"\"----\n"
	 "       rtl83xx_dlink_des1016d ----\"\"----\n"
	 "       rtl83xx_dlink_des1024d_c1 ----\"\"----\n"
	 "       rtl83xx_dlink_des1024d_d1 ----\"\"----\n"
	 "       rtl83xx_compex_ps2216 ----\"\"----\n"
	 "       rtl83xx_compex_ps2216_6d ----\"\"----\n"
	 "       rtl83xx_compex_ps2216_6e ----\"\"----\n"
	 "       rtl83xx_ovislink_fsh2402gt ----\"\"----\n"
	 "       rtl83xx_zyxel_es116p ----\"\"----\n"
	 "       rtl83xx_compex_sds1224 ----\"\"----\n"
	 "       rtl83xx_signamax_065-7531a ----\"\"----\n"
	 "       rtl83xx_repotec_g3224x ----\"\"----\n"
	 "       rtl83xx_edimax_es-3116p ----\"\"----\n"
	 "       rtl83xx help\n"
	 " where command may be:\n"
	 " detect                                - detect switch hardware\n"
	 " show running-config                   - show current switch config\n"
	 " show interface [<list ports>]         - print link status for ports\n"
	 " show interface [<list ports>] summary - print port rx/tx counters\n"
	 " show interface [<list ports>] cable-diagnostics - cabe diagnostics\n"
	 " show interface [<list ports>] phy-status        - PHY status\n"
	 " capture interface <list ports> mac-address [<time-sec>] - capure mac-addresses\n"
	 " show vlan [vid <id>]                  - show low-level vlan confg\n"
	 " show version                          - system hardware and software status\n"
	 " scan [verbose] [retries <number>]     - scan network for rrcp-enabled switches\n"
	 " reboot [soft|hard]                    - initiate switch reboot\n"
	 " config interface [<list ports>] ...   - port(s) configuration\n"
	 " --\"\"-- [no] shutdown                  - enable/disable specified port(s)\n"
	 " --\"\"-- [no] phy-shutdown              - enable/disable specified port(s) on PHY\n"
	 " --\"\"-- speed <arg> [duplex <arg>]     - set speed/duplex on specified port(s)\n"
	 " --\"\"-- flow-control <arg>             - set flow-control on specified port(s)\n"
	 " --\"\"-- rate-limit [input|output] <arg> - set bandwidth on specified port(s)\n"
	 " --\"\"-- mac-address learning enable|disable - enable/disable MAC-learning on port(s)\n"
	 " --\"\"-- rrcp enable|disable            - enable/disable rrcp on specified ports\n"
	 " --\"\"-- mls qos cos 0|7                - set port priority\n"
	 " --\"\"-- trunk enable|disable           - enable/disable per-port VID inserting\n"
	 " --\"\"-- tag remove|insert-high|insert-all|none - VLAN output port priority-tagging control\n"
	 " --\"\"-- index <idx>                    - set port VLAN table index assignment\n"
	 " config rrcp enable|disable            - global rrcp enable|disable\n"
	 " config rrcp echo enable|disable       - rrcp echo (REP) enable|disable\n"
	 " config rrcp loop-detect enable|disable - network loop detect enable|disable\n"
	 " config rrcp authkey <hex-value>       - set new authkey\n"
	 " config vlan disable                   - disable all VLAN support\n"
	 " config vlan mode portbased|dot1q      - enable specified VLAN support\n"
	 " config vlan template-load portbased|dot1qtree - load specified template\n"
	 " config vlan clear                     - clear vlan table (all ports in vlan 1)\n"
	 " config vlan add port <port-list> index <idx-list> - add port(s) in VLAN table on index\n"
	 " config vlan delete port <port-list> index <idx-list> - delete port(s) from VLAN table on index\n"
	 " config vlan index <idx-list> vid <vid> - set VLAN ID for index in VLAN table\n"
         " config [no] vlan leaky arp|multicast|unicast - Allow certain type of packets to be switched beetween VLANs\n"
         " config [no] vlan drop untagged_frames|invalid_vid - Allow dropping certain types of non-conforming packets\n"
	 " config mac-address <mac>              - set a new <mac> address to switch and reboot\n"
	 " config mac-address-table aging-time|drop-unknown <arg>  - address lookup table control\n"
	 " config flowcontrol dot3x enable|disable - globally disable full duplex flow control (802.3x pause)\n"
	 " config flowcontrol backpressure enable|disable - globally disable/enable half duplex back pressure ability\n"
	 " config flowcontrol ondemand-disable enable|disable - disable/enable flow control ability auto turn off for QoS\n"
	 " config [no] storm-control broadcast relaxed|strict - broadcast storm control\n"
	 " config [no] storm-control multicast   - multicast storm control\n"
	 " config [no] monitor [input|output] source interface <list ports> destination interface <port>\n"
	 " config [no] igmp-snooping\n"
	 " config [no] mls qos trust dscp|cos    - Quality-of-Service trust configuration \n"
	 " config wrr-queue ratio 4:1|8:1|16:1|1:0 - configure default high-priority vs. low-priority traffic ratio\n"
	 " config no wrr-queue                   - set 1:0 priority queue (high priority queue first always)\n"
	 " ping                                  - test if switch is responding\n"
	 " write memory                          - save current config to EEPROM\n"
	 " write defaults                        - save to EEPROM default values\n"
	 );
        return;
}

void print_supported_switches() {
   int id;
   printf("Supported switches:\n");
   for (id=0; id < switchtype_n; id++) {
      if (switchtypes[id].shortname)
	 printf("%-5i %-20s %s\n",
	       id,
	       switchtypes[id].shortname,
	       switchtypes[id].description ? switchtypes[id].description : "");
   }
   printf("\n");
}

int main(int argc, char **argv){
    unsigned int x[6];
    unsigned int ak;
    int i, r;
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
    char *cmd_level_1[]={"show","config","scan","reload","reboot","write","ping","detect","capture",""}; 
    char *show_sub_cmd[]={"running-config","startup-config","interface","vlan","version",""};
    char *scan_sub_cmd[]={"verbose","retries",""};
    char *show_sub_cmd_l2[]={"full","verbose",""};
    char *show_sub_cmd_l3[]={"summary","cable-diagnostics","phy-status","xml","xml-cable-diagnostics","crc-summary","router-port",""};
    char *show_sub_cmd_l4[]={"id",""};
    char *reset_sub_cmd[]={"soft","hard",""};
    char *write_sub_cmd[]={"memory","eeprom","defaults",""};
    char *config_sub_cmd_l1[]={"interface","rrcp","vlan","mac-address","mac-address-table","flowcontrol","storm-control","monitor","vendor-id","igmp-snooping", "mls", "wrr-queue", ""};
    char *config_intf_sub_cmd_l1[]={"no","shutdown","speed","duplex","rate-limit","mac-address","rrcp","mls","flow-control","trunk","tag","index","phy-shutdown",""};
    char *config_duplex[]={"half","full",""};
    char *config_rate[]={"100m","128k","256k","512k","1m","2m","4m","8m","input","output",""};
    char *config_mac_learn[]={"learning","disable","enable",""};
    char *config_port_qos[]={"7","0","qos","cos",""};
    char *config_port_flow[]={"none","asym2remote","symmetric","asym2local",""};
    char *config_rrcp[]={"disable","enable","echo","loop-detect","authkey",""};
    char *config_alt[]={"aging-time","unknown-destination",""};
    char *config_alt_time[]={"0","12","300",""};
    char *config_alt_dest[]={"drop","pass",""};
    char *config_vlan[]={"disable","transparent","clear","mode","template-load","add","delete","index","leaky","drop",""};
    char *config_vlan_mode[]={"portbased","dot1q",""};
    char *config_vlan_tmpl[]={"portbased","dot1qtree",""};
    char *config_vlan_port[]={"port","index",""};
    char *config_vlan_idx[]={"vid",""};
    char *config_vlan_leaky[]={"arp","multicast","unicast",""};
    char *config_vlan_drop[]={"untagged_frames","invalid_vid",""};
    char *config_flowc[]={"dot3x","backpressure","ondemand-disable",""};
    char *config_storm[]={"broadcast","multicast",""};
    char *config_storm_br[]={"relaxed","strict",""};
    char *config_monitor[]={"interface","source","destination","input","output",""};
    char *config_tagcontrol[]={"remove","insert-high","insert-all","none",""};

    switchtype = -1;
    {
       const char *base;
       base = basename(argv[0]);
       /* rtl83xx */
       if (base && strcmp(base, "rtl83xx") == 0) {
	  if ( argc < 3 || (strcmp(argv[1], "help") == 0)) {
	     print_usage();
	     putchar('\n');
	     print_supported_switches();
	     exit(0);
	  }else {
	     /* rtl83xx iface scan */
	     if ((argc >= 3)
		   && ((strcmp(argv[2],"scan") == 0)
			|| (strcmp(argv[2],"detect") == 0))) {
		switchtype = 0;
	     }else {
		/* rtl83xx switch addr command ... */
		switchtype = rrcp_get_switch_id_by_short_name(argv[1]);
		argv[1] = argv[0];
		argv++;
		argc--;
	     }
	  }
       /* rtl83xx_... */
       }else {
	  const struct {
	     unsigned swtype;
	     const char *argv0;
	  } switches[] = {
	     {0,  "rtl8326"   },
	     {1,  "rtl8316b"  },
	     {2,  "rtl8316bp" },
	     {3,  "rtl8318"   },
	     {4,  "rtl8324"   },
	     {5,  "rtl8324p"  },
	     {6,  "rtl83xx_asus_gigax_1024p" },
	     {7,  "rtl83xx_compex_sds1224"   },
	     {8,  "rtl83xx_compex_ps2216"    },
	     {9,  "rtl83xx_compex_ps2216_6d" },
	     {10, "rtl83xx_compex_ps2216_6e" },
	     {11, "rtl83xx_dlink_des1016d"   },
	     {12, "rtl83xx_dlink_des1024d_b1"},
	     {13, "rtl83xx_dlink_des1024d_c1"},
	     {14, "rtl83xx_edimax_es-3116p"  },
	     {15, "rtl83xx_ovislink_fsh2402gt"},
	     {16, "rtl83xx_repotec_g3224x"    },
	     {17, "rtl83xx_signamax_065-7531a"},
	     {18, "rtl83xx_zyxel_es116p"      },
	     {19, "rtl83xx_ovislink_fsh2402gt" },
	     {-1,NULL}
	  };
	  unsigned i;
	  for (i=0; switches[i].argv0; i++)
	     if (strcmp(base, switches[i].argv0)==0) {
		switchtype = switches[i].swtype;
		break;
	     }
       }
    }

    if (argc<3){
        print_usage();
	exit(0);
    }

    /* allow scan command to be run without specifying chip/switch */
    if (strcmp(argv[2],"scan")==0) {
       switchtype = 0;
       if (parse_switch_id(argv[1]) < 0)
	  strcpy(ifname,argv[1]);
    }else {
       if (switchtype < 0) {
	  printf("Unknown switch name\n\n");
	  print_supported_switches();
	  exit(0);
       }
       if (parse_switch_id(argv[1]) < 0) {
	  printf("%s: malformed switch reference: '%s'\n",argv[0],argv[1]);
	  printf("Usage: %s [[authkey-]xx:xx:xx:xx:xx:xx@]if-name <command> [<argument>]\n", argv[0]);
	  exit(0);
       }
    }

    if (if_nametoindex(ifname)<=0){
	printf("%s: no such interface: %s\n",argv[0],ifname);
	exit(0);
    }

    if ((argc > 4 && strncmp(argv[4],"xml",3)==0) || (argc > 5 && strncmp(argv[5],"xml",3)==0)) {
        out_xml = 1;
    } else {
        if (argc<2 || strcmp(argv[2],"scan")!=0){
            printf("! rtl83xx: trying to reach %d-port \"%s %s\" switch at %s\n",
                switchtypes[switchtype].num_ports,
                switchtypes[switchtype].vendor,
                switchtypes[switchtype].model,
                argv[1]);
        }
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
                          switch (get_cmd_num(argv[4+shift],-1,NULL,&show_sub_cmd_l3[0])) {
			     case 0: /* summary */
				print_counters(p_port_list);
				break;
			     case 1: /* cable-diagnostics */
				print_cable_diagnostics(p_port_list);
				break;
			     case 2: /* phy-status */
				print_port_phy_status(p_port_list);
				break;
                             case 3: /* xml */
                                print_link_status_xml(p_port_list);
                                break;
                             case 4: /* xml-cable-diagnostics */
                                print_cable_diagnostics_xml(p_port_list);
                                break;
                             case 5: /* crc-summary */
                                print_counters_crc(p_port_list);
                                break;
                             case 6: /* router-port */
                                print_router_port(p_port_list);
                                break;
			     default:
				print_unknown(argv[4+shift],&show_sub_cmd_l3[0]);
				break;
			  }
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
		   case 4: /* version */
			  exit(do_show_version() < 0 ? -1 : 0);
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
				    {
				       char *sub_cmd_l2[] = {"shutdown","phy-shutdown",""};

				       check_argc(argc,5+shift,NULL,&sub_cmd_l2[0]);
				       switch (compare_command(argv[6+shift],&sub_cmd_l2[0])) {
					  case 0: /* shutdown */
					     do_port_disable(&port_list[0],1);
					     break;
					  case 1: /* phy-shutdown */
					     do_port_phy_disable(&port_list[0],1);
					     break;
				       }
				       exit(0);
				    }
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

					subcmd = check_vlan_entry_index_list(argv[6+shift], 1);
					if (subcmd < 0) {
					   fputs(ErrMsg, stdout);
					   exit(1);
					}
					do_vlan_index_ctrl(&port_list[0],subcmd);
					exit(0);
                                 case 12: /* phy-shutdown */
                                        do_port_phy_disable(&port_list[0],0);
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
		     {
			  int vlan_delete_cmd = 1;
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
					vlan_delete_cmd = 0;
					/* FALLTHROUGH */
                                 case 6: // delete
					for(i=4;i<=7;i++)
					   check_argc(argc,i+shift,NULL,&config_vlan_port[0]);
					if(strcmp(argv[5+shift],config_vlan_port[0]))
					   print_unknown(argv[5+shift],&config_vlan_port[0]);

					if (str_portlist_to_array(argv[6+shift], &port_list[0], switchtypes[switchtype].num_ports)!=0){
					   printf("Incorrect list of ports: \"%s\"\n",argv[6+shift]);
					   exit(1);
					}
					if(strcmp(argv[7+shift],config_vlan_port[1])) print_unknown(argv[7+shift],&config_vlan_port[0]);
					if (do_vlan_table_config(&port_list[0],argv[8+shift], vlan_delete_cmd) != 0) {
					   fputs(ErrMsg, stdout);
					   exit(1);
					}
					exit(0);
				 case 7: // index
					for(i=4;i<=6;i++) check_argc(argc,i+shift,NULL,&config_vlan_idx[0]);

					if (do_vlan_index_config(argv[5+shift], argv[7+shift]) != 0) {
					   fputs(ErrMsg, stdout);
					   exit(1);
					}
					exit(0);
                                 case 8: // leaky
                                        check_argc(argc,4+shift,NULL,&config_vlan_leaky[0]);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&config_vlan_leaky[0]);
                                        do_vlan_leaky(subcmd+1,!negate);
                                        exit(0);
                                 case 9: // drop
                                        check_argc(argc,4+shift,NULL,&config_vlan_drop[0]);
                                        subcmd=get_cmd_num(argv[5+shift],-1,NULL,&config_vlan_drop[0]);
                                        do_vlan_drop(subcmd+1,!negate);
                                        exit(0);
                                 default:
                                         print_unknown(argv[4+shift],&config_vlan[0]);
                          } /* switch */
		     } /* case 2 */
                   case 3: // mac-address
                          check_argc(argc,3+shift,"MAC address needed\n",NULL);
   	                  if ((sscanf(argv[4+shift], "%02x:%02x:%02x:%02x:%02x:%02x",x,x+1,x+2,x+3,x+4,x+5)!=6)&&
	                      (sscanf(argv[4+shift], "%02x%02x.%02x%02x.%02x%02x",x,x+1,x+2,x+3,x+4,x+5)!=6)){
   		              printf("malformed mac address: '%s'!\n",argv[4+shift]);
                              exit(1);
                          }
		          for (i=0;i<6;i++){
                              for (r = 0; r < 10 && do_write_eeprom_byte(0x12+i,(unsigned char)x[i]); r++) {
                                  printf("Can't write 0x%02x to EEPROM 0x%03x\n",(unsigned char)x[i],0x12+i);
                                  usleep(100000);
                              }
                              if (r > 9) {
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
		   case 10: // mls
			  {
			     unsigned mask, i;
			     char *qos_cmd[] = {"qos", ""};
			     char *trust_cmd[] = {"trust", ""};
			     char *dscp_cos_cmd[] = {"dscp", "cos", ""};

			     check_argc(argc, 3+shift, NULL, &qos_cmd[0]);
			     get_cmd_num(argv[4+shift], -1, NULL, &qos_cmd[0]);
			     check_argc(argc, 4+shift, NULL, &trust_cmd[0]);
			     get_cmd_num(argv[5+shift], -1, NULL, &trust_cmd[0]);
			     mask = 0;
			     check_argc(argc, 5+shift, NULL, &dscp_cos_cmd[0]);
			     for (i = 6+shift; i < argc; i++) {
				switch (get_cmd_num(argv[i],-1, NULL, &dscp_cos_cmd[0])) {
				   case 0:
				      mask |= 0x01;
				      break;
				   case 1:
				      mask |= 0x02;
				      break;
				   default:
				      /* UNREACHABLE */
				      print_unknown(argv[i],&dscp_cos_cmd[0]);
				}
			     }
			     do_mls_qos_trust(!negate, !negate, mask);
			     exit(0);
			  }
		   case 11: // wrr-queue
			  if (negate)
			     do_wrr_queue("1:0");
			  else {
			     char *ratio_cmd[] = {"ratio", ""};
			     char *weight_cmd[] = {"4:1","8:1","16:1","1:0",""};

			     check_argc(argc, 3+shift, NULL, &ratio_cmd[0]);
			     get_cmd_num(argv[4+shift], -1, NULL, &ratio_cmd[0]);
			     check_argc(argc, 4+shift, NULL, &weight_cmd[0]);
			     get_cmd_num(argv[5+shift], -1, NULL, &weight_cmd[0]);
			     do_wrr_queue(argv[5+shift]);
			  }
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
	 case 7: /* detect */
		{
		   uint8_t switch_t, chip_t;
		   t_eeprom_type eeprom;

		   eeprom = 0;
		   rrcp_autodetect_switch_chip_eeprom(&switch_t, &chip_t, &eeprom);
		   printf("Detected: %s (%s). Chip: %s; EEPROM: %s\n",
			 switchtypes[switch_t].description,
			 switchtypes[switch_t].shortname,
			 chipnames[switchtypes[switch_t].chip_id],
			 eeprom_type_text[eeprom]);
		   exit(0);
		}
	 case 8: /* capture */
		{
		   char *interface_cmd[] = {"interface", ""};
		   char *macaddr_cmd[] = {"mac-address", ""};
                   int capture_mac_time_sec = DEFAULT_CAPTURE_MAC_TIME_SEC;

		   check_argc(argc, 3+shift, NULL, &interface_cmd[0]);
		   check_argc(argc, 4+shift, "Interface number nedded\n", NULL);
		   get_cmd_num(argv[5+shift], -1, NULL, &macaddr_cmd[0]);
                   if (argc > 6+shift) {
                       if (sscanf(argv[6+shift], "%i",&capture_mac_time_sec) != 1) {
                           printf("Incorrect time sec: \"%s\"\n",argv[6+shift]);
                           exit(0);
                       }
                   }

		   if (do_capture_mac_address(argv[4+shift], capture_mac_time_sec) != 0) {
		      fputs(ErrMsg, stdout);
		      exit(1);
		   }
		   exit(0);
		}
         default:
                print_usage();
    }
    exit(0);
}
