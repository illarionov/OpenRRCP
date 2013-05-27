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

#define _GNU_SOURCE

#include <ctype.h>
#include <getopt.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rrcp_config.h"
#include "rrcp_switches.h"
#include "command.h"

t_eeprom_type eeprom_type;
const char *input;
const char *output;
unsigned authkey;
char *mac;

void print_usage(const char *arg0)
{
   if (arg0 == NULL)
      arg0 = "conf2eeprom";

   fprintf(stdout, "Usage: %s [-t type] [-s switch] [-k authkey]\n"
	           "                [-m mac] input output\n", arg0);

   return;
}

void print_help(const char *arg0)
{
   print_usage(arg0);
   fprintf(stdout,
	 "-t type         eeprom type: 2401, 2402, 2404, 2408, 2416\n"
	 "-s switch       switch type\n"
	 "-k authkey      set authkey\n"
	 "-m mac	  set mac-address\n"
	 );

   return;
}

t_eeprom_type parse_eeprom_type(const char *eeprom_type) {
   unsigned u;

   if (eeprom_type==NULL)
      return EEPROM_NONE;

   u = (unsigned)strtoul(eeprom_type,(char **)NULL, 10);
   switch (u) {
      case 2401:
	 return EEPROM_2401;
      case 2402:
	 return EEPROM_2402;
      case 2404:
	 return EEPROM_2404;
      case 2408:
	 return EEPROM_2408;
   }
   return EEPROM_NONE;
}

size_t eeprom_size_b(t_eeprom_type type) {
   switch (type) {
      case EEPROM_2401:
	 return 1024/8;
      case EEPROM_2402:
	 return 2048/8;
      case EEPROM_2404:
	 return 4096/8;
      case EEPROM_2408:
	 return 8192/8;
      default:
	 break;
   }

   return 0;
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


int load_command_line(int argc, char **argv)
{
   int ch;
   int res;
   int old_optind;

   static struct option longopts[] = {
       {"eeprom",     required_argument,	  NULL,	't'},
       {"switch",     required_argument,	  NULL,	's'},
       {"authkey",    required_argument,	  NULL,	'k'},
       {"mac",	      required_argument,	  NULL,	'm'},
       {"help",	     no_argument,	  NULL,	'h'},
       {NULL, 0, NULL, 0}
    };

   res = 0;
   old_optind = optind;
   /* optreset = 1; */
   optind = 1;


   /* defaults */
   eeprom_type = EEPROM_2408;
   switchtype = 2;
   authkey = 0x2379;
   mac=NULL;

   while ((ch = getopt_long(argc, argv, "t:s:k:m:h",
		longopts, NULL)) != -1) {
       switch (ch) {
	  case 't':
	     eeprom_type=parse_eeprom_type(optarg);
	     if (eeprom_type==EEPROM_NONE) {
		fprintf(stderr,"Unkown EEPROM. Supported: 2401, 2402, 2404, 2408, 2416");
		res = -1;
	     }
	     break;
	  case 's':
	     switchtype = rrcp_get_switch_id_by_short_name(optarg);
	     if (switchtype<0) {
		fprintf(stderr,"Unknown switch");
		print_supported_switches();
	     }
	     break;
	  case 'h':
	     print_help(argv[0]);
	     res = -1;
	     break;
	  case 'k':
	     authkey = strtoul(optarg,NULL, 16);
	     break;
	  case 'm':
	     mac = strdup(optarg);
	     break;
	  default:
	     print_help(argv[0]);
	     res = -2;
	     break;
       }
   }

   if (argc-2 != optind) {
      print_help(argv[0]);
      res = -3;
   }

   input = argv[optind];
   output = argv[optind+1];

   optind = old_optind;
   /* optreset = 1; */

   return res;
}

/* Concatenate cmdprefix and p in cmd.
 * Remove dup spaces and newline*/
int strconcat_nosp(char *dst, size_t size, const char *s1, const char *s2)
{
   const char *s1_p, *s2_p;
   unsigned add_space;
   unsigned dst_idx;

   dst_idx = 0;
   add_space=0;

   /* Skip leading spaces */
   s1_p = s1;
   for(;(*s1_p != '\0') && (isspace(*s1_p));s1_p++);

   /* copy s1 */
   for (; *s1_p; s1_p++){
      if (isspace(*s1_p))
	 add_space=1;
      else {
	 if (add_space) {
	    if (dst_idx+2>=size)
	       break;
	    dst[dst_idx++]=' ';
	    add_space=0;
	 }else {
	    if (dst_idx+1>=size)
	       break;
	 }
	 dst[dst_idx++]=*s1_p;
      }
   }

   if (dst_idx+2>=size) {
      dst[dst_idx]='\0';
      return -1;
   }else
      dst[dst_idx++]=' ';

   /* Skip leading spaces */
   s2_p = s2;
   for(;(*s2_p != '\0') && (isspace(*s2_p));s2_p++);

   /* copy s2 */
   add_space=0;
   for (; *s2_p; s2_p++){
      if (isspace(*s2_p))
	 add_space=1;
      else {
	 if (add_space) {
	    if (dst_idx+2>=size)
	       break;
	    dst[dst_idx++]=' ';
	    add_space=0;
	 }else {
	    if (dst_idx+1>=size)
	       break;
	 }
	 dst[dst_idx++]=*s2_p;
      }
   }

   if (dst_idx+1>=size) {
      dst[dst_idx]='\0';
      return -2;
   }

   /* remove trailing spaces */
   while ((dst_idx>=1)
	 && (isspace(dst[dst_idx-1])))
      dst_idx--;

   dst[dst_idx] = '\0';

   return 0;
}

int main(int argc, char **argv)
{
   FILE *infile, *outfile;
   unsigned strnum;
   uint8_t *eeprom;
   size_t eeprom_size;
   char errstr[80];
   char str[120];
   char cmd[120];
   char cmdprefix[120];
   if (load_command_line(argc, argv))
      return 1;

   eeprom_size=eeprom_size_b(eeprom_type);
   eeprom = malloc(eeprom_size);

   if (eeprom == NULL) {
      free(mac);
      perror("Cannot alocate memory for eeprom");
      return 3;
   }
   if (rrcp_config_eeprom_init(eeprom, eeprom_size, switchtype)) {
      fprintf(stderr, "cannot initialize eeprom\n");
      free(mac);
      free(eeprom);
      return 4;
   }

   infile = fopen(input, "r");
   if (infile==NULL) {
      free(eeprom);
      free(mac);
      perror("Cannot open input file");
      return 2;
   }

   /* Load configuration from file */
   cmdprefix[0]='\0';
   for(strnum=1; fgets(str, sizeof(str), infile) !=NULL; strnum++) {
      char *p;

      /* Skip leading spaces */
      p = str;
      for(;*p && isspace(*p);p++);

      /* Skip comments */
      if ((p[0]=='\0')
	    || (p[0]=='!'))
	 continue;
      if (strcasestr(p, "interface")==p){
	 strcpy(cmdprefix, p);
      }else {
	 strconcat_nosp(cmd, sizeof(cmd), cmdprefix, p);
	 if (cmd_change_eeprom(
		  cmd, eeprom, eeprom_size, switchtype,
		  errstr, sizeof(errstr)))
	    fprintf(stderr,"%s:%u Error: %s; Command: %s\n", input, strnum, errstr, cmd);
      }
   } /* for */

   fclose(infile);

   /* set authkey */
   snprintf(cmd, sizeof(cmd), "rrcp authkey %x", authkey);
   if (cmd_change_eeprom(cmd
	    ,eeprom, eeprom_size, switchtype,
	    errstr, sizeof(errstr)))
      fprintf(stderr,"%s:%u Error: %s; Command: %s\n", input, strnum, errstr, cmd);

   /* set vender, chip */
   if (switchtypes[switchtype].r_vendor_id[0] != 0) {
      snprintf(cmd, sizeof(cmd), "vender %x", switchtypes[switchtype].r_vendor_id[0]);
      if (cmd_change_eeprom(cmd
	       ,eeprom, eeprom_size, switchtype,
	       errstr, sizeof(errstr)))
	 fprintf(stderr,"%s:%u Error: %s; Command: %s\n", input, strnum, errstr, cmd);
   }

   if (switchtypes[switchtype].r_chip_id != 0) {
      snprintf(cmd, sizeof(cmd), "chip %x", switchtypes[switchtype].r_chip_id);
      if (cmd_change_eeprom(cmd
	       ,eeprom, eeprom_size, switchtype,
	       errstr, sizeof(errstr)))
	 fprintf(stderr,"%s:%u Error: %s; Command: %s\n", input, strnum, errstr, cmd);
   }

   /* set mac-address */
   if (mac != NULL) {
      snprintf(cmd, sizeof(cmd), "mac-address %s", mac);
      if (cmd_change_eeprom(cmd
	       ,eeprom, eeprom_size, switchtype,
	       errstr, sizeof(errstr)))
	 fprintf(stderr,"%s:%u Error: %s; Command: %s\n", input, strnum, errstr, cmd);
   }

   /* save eeprom */
   outfile = fopen(output, "w");
   if (outfile==NULL) {
      free(eeprom);
      perror("Cannot open output file");
      return 2;
   }


   if (fwrite(eeprom, eeprom_size, 1, outfile) != 1) {
      free(eeprom);
      fprintf(stderr, "cannot write eeprom\n");
      return 5;
   }

   free(eeprom);
   free(mac);
   fclose(outfile);

   return 0;
}



