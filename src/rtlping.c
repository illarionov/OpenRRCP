/*
    This file is part of OpenRRCP

    Copyright (c) 2009 Alexey Illarionov <littlesavage@rambler.ru>

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

*/

#include <sys/time.h>
#include <netinet/in.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "rrcp_packet.h"
#include "rrcp_io.h"
#include "rrcp_lib.h"
#include "rrcp_config.h"
#include "rrcp_switches.h"

#define MAX_PING_COUNT 10000
#define MIN_PACKET_INTERVAL 20 /* ms */
#define DEFAULT_PACKET_INTERVAL	 1000

extern char ErrIOMsg[];
extern char ifname[];
extern uint16_t authkey;

static int interrupted = 0;
static char Sw_id_str[50];

void rtlping_usage()
{
   fprintf(stdout, "Usage: rtlping [-hq] [-c count]\n\
               [-i wait] [-m mode] [[authkey-]xx:xx:xx:xx:xx:xx@]if-name\n");
   fflush(stdout);

   return;
}

void rtlping_sig_handler(int sig)
{
   interrupted = 1;
}

void rtlping_help()
{
   rtlping_usage();
   fprintf(stdout, "\
-c, --count=COUNT\n    Number of packets to send, default: unlimited\n\
-i, --wait=WAIT\n    Wait between sending each packet, ms, default: 1000\n\
-m, --mode=MODE\n    Ping mode: rrcp, rep, or rrcprep, default: rrcp \n\
    rrcp - Use RRCP (Realtek Remote Control Protocol)\n\
    rep - Use REP (Realtek Echo Protocol)\n\
    rrcprep - Use REP if device not responded to RRCP\n\
-q, --quiet\n    Quiet output.\n");

   return;
}

const char *switch_id_str(char *res,
      int size,
      const uint8_t *macaddr,
      const uint16_t *authkp, /*in network byte order if not NULL */
      const char *ifn)
{
   uint16_t authk;

   if (!res || !size)
      res = Sw_id_str, size = sizeof(Sw_id_str);

   if (!macaddr)
      macaddr = dest_mac;

   if (!authkp)
      authk = htons(authkey);
   else
      authk = *authkp;

   if (!ifn)
      ifn = ifname;

   if (authk == RRCP_DEFAULT_AUTHKEY)
      snprintf(res, size, "%.2hhx:%.2hhx:%.2hhx:%.2hhx:%.2hhx:%.2hhx@%s",
	    macaddr[0],macaddr[1],macaddr[2],
	    macaddr[3],macaddr[4],macaddr[5],
	    ifn);
   else
      snprintf(res, size, "%.4x-%.2hhx:%.2hhx:%.2hhx:%.2hhx:%.2hhx:%.2hhx@%s",
	    ntohs(authk),
	    macaddr[0],macaddr[1],macaddr[2],
	    macaddr[3],macaddr[4],macaddr[5],
	    ifn);
   res[size-1]='\0';

   return res;
}




int main(int argc, char **argv)
{
   int ch;
   unsigned cnt, recived;
   long *resp_times;

   struct {
      unsigned max_count;
      long  wait;
#define PING_MODE_RRCP   0
#define PING_MODE_REP    1
#define PING_MODE_RRCP_REP    2
      unsigned mode;
      unsigned quiet;
      unsigned switch_id_initialized;
   } params = {0, DEFAULT_PACKET_INTERVAL, PING_MODE_RRCP, 0, 0};

    static struct option longopts[] = {
       {"count",     required_argument,	  NULL,	'c'},
       {"help",	     no_argument,	  NULL,	'h'},
       {"wait",	     required_argument,	  NULL,	'i'},
       {"mode",	     required_argument,	  NULL,	'm'},
       {"quiet",     no_argument,	  NULL,	'q'},
       {NULL, 0, NULL, 0}
    };

    if (argc < 2) {
       rtlping_usage();
       return -3;
    }

    while ((ch = getopt_long(argc, argv, "c:i:m:qh",
		longopts, NULL)) != -1)
    {
       switch (ch) {
	  case 'c':
             params.max_count = (unsigned)strtoul(optarg,(char **)NULL, 10);
	     if (params.max_count > MAX_PING_COUNT) {
		fprintf(stderr, "maximum allowed count: %i\n", MAX_PING_COUNT);
		return -2;
	     }
	     break;
	  case 'i':
             params.wait = (unsigned)strtoul(optarg,(char **)NULL, 10);
	     if (params.wait < MIN_PACKET_INTERVAL) {
		fprintf(stderr, "minimum wait interval: %i\n", MIN_PACKET_INTERVAL);
		return -2;
	     }
	     break;
	  case 'm':
	     if (strcasecmp(optarg,"rrcp")==0)
		params.mode=PING_MODE_RRCP;
	     else if (strcasecmp(optarg, "rep")==0)
		params.mode=PING_MODE_REP;
	     else if (strcasecmp(optarg, "rrcprep")==0)
		params.mode=PING_MODE_RRCP_REP;
	     else {
		fprintf(stderr, "unknown ping mode: %s\n", optarg);
		return -2;
	     }
	     break;
	  case 'q':
	     params.quiet = 1;
	     break;
	  case 'h':
	     rtlping_help();
	     return -2;
	     break;
	  default:
	     rtlping_usage();
	     return -1;
	     break;
       }
    }
    if (argc > optind) {
       if (parse_switch_id(argv[optind]) < 0) {
	  fprintf(stderr, "%s: malformed switch reference: '%s'\n",
		argv[0],argv[optind]);
	  return -2;
       }else
	  params.switch_id_initialized = 1;
    }
    argc -= optind;
    argv += optind;

    if (!params.switch_id_initialized) {
	  fprintf(stderr, "switch id not defined\n");
	  return -2;
    }

    signal(SIGINT, rtlping_sig_handler);

    if (rtl83xx_prepare()){ fprintf(stderr, "%s\n",ErrIOMsg); return -2;}

    if (params.quiet && (params.max_count > 0)) {
       resp_times = malloc(params.max_count*sizeof(resp_times[0]));
       if (!resp_times) {
	  fprintf(stderr, "No enough memory\n");
	  return -2;
       }
    }else
       resp_times = NULL;

    /* Ping */
    recived = 0;
    if (!params.quiet)
       printf("RTLPING %s\n", switch_id_str(NULL,0, NULL, NULL, NULL));
    for (cnt=0;!interrupted;cnt++) {
       unsigned dup_cnt;
       long resp_time_us, diff_us, remain_us;
       struct timeval starttime, stoptime, tmptime;
       struct rrcp_packet_t *ping_sent;
       struct rrcp_packet_t ping_recv;

       ping_sent = NULL;
       if (params.max_count && cnt >= params.max_count)
	  break;

       gettimeofday(&starttime,NULL);
       switch (params.mode) {
	  case PING_MODE_RRCP:
	     resp_time_us = rtl83xx_ping_ex(params.wait, 0, &ping_sent, &ping_recv);
	     break;
	  case PING_MODE_RRCP_REP:
	     resp_time_us = rtl83xx_ping_ex(params.wait, 0, &ping_sent, &ping_recv);
	     if (resp_time_us <= 0 && !interrupted) {
		gettimeofday(&starttime,NULL);
		resp_time_us = rtl83xx_ping_ex(params.wait, 1, &ping_sent, &ping_recv);
	     }
	     break;
	  case PING_MODE_REP:
	  default:
	     resp_time_us = rtl83xx_ping_ex(params.wait, 1, &ping_sent, &ping_recv);
	     break;
       }
       if (interrupted) {
	  free (ping_sent);
	  break;
       }

       if (resp_time_us > 0)
	  recived++;

       /* Print first reply packet */
       if (!params.quiet && resp_time_us > 0)
	  fprintf(stdout, "%s : [%u] %.3f ms\n",
		switch_id_str(NULL, 0,
		   ping_recv.ether_shost,
		   ping_recv.rrcp_proto==RTL_RRCP_PROTO ? &ping_recv.rrcp_authkey : NULL,
		   ifname),
		cnt, resp_time_us / 1000.0);
       else if (resp_times)
	  resp_times[cnt] = resp_time_us;

       /* Wait for timeout. Print dup reply packets */
       dup_cnt = 0;
       remain_us = 1;
       while (remain_us > 0 && !interrupted) {
	  gettimeofday(&stoptime, NULL);
	  timersub(&stoptime, &starttime, &tmptime);
	  diff_us = tmptime.tv_sec * 1000000 + tmptime.tv_usec;
	  remain_us = params.wait * 1000 - diff_us;

	  if (remain_us > 0) {
	     resp_time_us = rtl83xx_ping_ex(remain_us / 1000 + 1,
		   0, &ping_sent, &ping_recv);

	     if (!params.quiet && resp_time_us > 0) {
		dup_cnt += 1;
		fprintf(stdout, "%s : [%u-%u] ? ms\n",
		      switch_id_str(NULL, 0,
			 ping_recv.ether_shost,
			 ping_recv.rrcp_proto==RTL_RRCP_PROTO ? &ping_recv.rrcp_authkey : NULL,
			 ifname),
		      cnt, dup_cnt);
	     }
	  }
       }
       free(ping_sent);
    }

    if (!resp_times && interrupted) {
       fprintf(stdout, "\n");
       fflush(stdout);
    }

    /* Print statistics */
   if (resp_times) {
      unsigned i;
      fprintf(stdout, "%s :", switch_id_str(NULL, 0, NULL, NULL, NULL));
      for (i=0; i < cnt; i++)
	 if (resp_times[i] > 0)
	    fprintf(stdout, " %.3f", resp_times[i] / 1000.0);
	 else
	    fprintf(stdout, " -");
      fprintf(stdout, "\n");
   }else {
      fprintf(stdout,"--- %s statistics ---\n\
%u packets transmitted, %u packets received,    %.0f%% unanswered\n",
      switch_id_str(NULL, 0, NULL, NULL, NULL),
      cnt, recived, cnt ? 100.0 - 100.0*recived/cnt : 0.0);
   }
   fflush(stdout);
   free(resp_times);

   return 0;
}


