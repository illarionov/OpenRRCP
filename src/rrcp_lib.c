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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int str_portlist_to_array_by_value(char *list,int *arr,unsigned int arrlen){
    short int i,k;
    char *s,*c,*n;
    char *d[16];
    unsigned int st,sp;
    int arrptr=0;

    s=list;
    for (i=0;i<strlen(s);i++){  //check allowed symbols
	if ( ((s[i] >= '0') && (s[i] <= '9')) || (s[i] == ',') || (s[i] == '-') ) continue;
	arr[arrptr]=-1;
	return(1);
    }
    while(*s){
	bzero(d,sizeof(d));
	// parsing
	if ( (c=strchr(s,',')) != NULL ) { k=c-s; n=c+1; }
	else { k=strlen(s); n=s+k; }
	if (k >= sizeof(d)) {
	    arr[arrptr]=-1;
	    return(1);
	}
	memcpy(d,s,k);s=n;
	// range of ports or one port?
	if (strchr((char *)d,'-')!=NULL){
	    // range
	    if (sscanf((char *)d,"%u-%u",&st,&sp) != 2) {
		arr[arrptr]=-1;
		return(2);
	    }
	    if ( !st || !sp || (st > sp) ) {
		arr[arrptr]=-1;
		return(3);
	    }
	    for (i=st;i<=sp;i++) { arr[arrptr++]=i; }
	}else{
	    // one port
	    st=(unsigned int)strtoul((char *)d, (char **)NULL, 10);
	    if ( !st ) {
		arr[arrptr]=-1;
		return(3);
	    }
	    arr[arrptr++]=st;
	}
    }
    arr[arrptr]=-1;
    return(0);
}
