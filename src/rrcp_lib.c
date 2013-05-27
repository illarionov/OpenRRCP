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

    Some support can be found at: http://openrrcp.org.ru/
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rrcp_packet.h"
#include "rrcp_lib.h"

extern char ifname[];
extern uint16_t authkey;
extern unsigned char dest_mac[];

#define IFNAME_SIZE 128

int str_portlist_to_array(char *list,unsigned short int *arr,unsigned int arrlen){
    short int i,k;
    char *s,*c,*n;
    char *d[16];
    unsigned int st,sp;

    s=list;
    for (i=0;i<arrlen;i++) { *(arr+i)=0; }
    for (i=0;i<strlen(s);i++){  //check allowed symbols
	if ( ((s[i] >= '0') && (s[i] <= '9')) || (s[i] == ',') || (s[i] == '-') ) continue;
	return(1);
    }
    while(*s){
	bzero(d,sizeof(d));
	// parsing
	if ( (c=strchr(s,',')) != NULL ) { k=c-s; n=c+1; }
	else { k=strlen(s); n=s+k; }
	if (k >= sizeof(d)) return(1);
	memcpy(d,s,k);s=n;
	// range of ports or one port?
	if (strchr((char *)d,'-')!=NULL){
	    // range
	    if (sscanf((char *)d,"%u-%u",&st,&sp) != 2) return(2);
	    if ( !st || !sp || (st > sp) || (st > arrlen) || (sp > arrlen) ) return(3);
	    for (i=st;i<=sp;i++) { *(arr+i-1)=1; }
	}else{
	    // one port
	    st=(unsigned int)strtoul((char *)d, (char **)NULL, 10);
	    if ( !st || (st > arrlen) ) return(3);
	    *(arr+st-1)=1;
	}
    }
    return(0);
}

int parse_switch_id(const char *str) {
   unsigned i, key;
   unsigned char x[6];
   char ifn[128];

   if (sscanf(str, "%4x-%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx@%128s",&key,x,x+1,x+2,x+3,x+4,x+5,ifn)==8){
	 authkey=(uint16_t)key;
   }else if (sscanf(str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx@%128s",x,x+1,x+2,x+3,x+4,x+5,ifn)==7){
   }else
      return -1;

    ifn[sizeof(ifn)-1] = '\0';
    strncpy(ifname, ifn, IFNAME_SIZE);
    ifname[IFNAME_SIZE-1]='\0';

    for (i=0;i<6;i++) dest_mac[i]=x[i];

    return 0;
}


int str_number_list_init(const char *str, struct t_str_number_list *list)
{
   int err;
   int val;
   int cnt;

   list->list = str;
   list->cur_p = str;
   list->last_val=0;
   list->range_max=0;

   err=0;
   cnt=0;

   while ((err = str_number_list_get_next(list, &val))==0)
      cnt++;

   if (err < 0)
      return -1;

   list->cur_p = str;
   list->last_val=0;
   list->range_max=0;

   return cnt;
}

int str_number_list_get_next(struct t_str_number_list *list, int *val)
{
   char *endp;
   int first, last;

   if (list->last_val < list->range_max) {
      *val = ++list->last_val;
      return 0;
   }

   if (*list->cur_p == '\0')
      return 1;

   first = strtoul(list->cur_p, &endp, 10);

   /* no digits, syntax error */
   if (endp == list->cur_p)
      return -1;

   switch (*endp) {
      case ',':
	 /* one, not last element */
	 endp++;
	 if (*endp=='\0')
	    return -1;
	 /* last element */
      case '\0':
	 list->range_max = first;
	 break;
      case '-':
	 /* range */
	 list->cur_p = ++endp;
	 last = strtoul(list->cur_p, &endp, 10);

	 if (endp == list->cur_p)
	    return -1;

	 if (*endp == ',')
	    endp++;
	 else if (*endp != '\0')
	    return -1;

	 if (first >= last)
	    return -1;

	 list->range_max = last;
	 break;
      default:
	 /* syntax error */
	 return -1;
	 break;
   }

   list->cur_p = endp;
   list->last_val = *val = first;

   return 0;
}

