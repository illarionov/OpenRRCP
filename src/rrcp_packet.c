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

#include <netinet/in.h>
#include <stdlib.h>
#include "rrcp_packet.h"

static int init_rrcp_packet(
    struct rrcp_packet_t *t,
    unsigned proto,
    unsigned opcode,
    const uint8_t *src,
    const uint8_t *dst,
    const uint16_t *authkey,
    const unsigned *reg_addr,
    const unsigned *reg_data
)
{
   unsigned i;
   for (i=0; i < sizeof(t->ether_dhost); i++)
      t->ether_dhost[i] = dst ? dst[i] : 0xff;
   for (i=0; i < sizeof(t->ether_shost); i++)
      t->ether_shost[i] = src ? src[i] : 0x00;
   t->ether_type = RTL_ETHER_TYPE;
   t->rrcp_proto = proto;
   t->rrcp_opcode = (proto == RTL_REP_PROTO ? 0 : opcode);
   t->rrcp_isreply = 0;
   if (proto != RTL_REP_PROTO)
      t->rrcp_authkey=htons(authkey ? authkey[0]&0xffff : RRCP_DEFAULT_AUTHKEY);
   else
      t->rrcp_authkey=0;
   t->rrcp_reg_addr = reg_addr ? reg_addr[0]&0x0000ffff: 0;
   t->rrcp_reg_data = reg_data ? reg_data[0]&0xffffffff: 0;
   t->cookie1 = rand();
   t->cookie2 = rand();

   return 0;
}

int init_rrcp_hello_packet(
    struct rrcp_packet_t *t,
    const uint8_t *src,
    const uint8_t *dst,
    const uint16_t *authkey)
{
   return init_rrcp_packet(t, RTL_RRCP_PROTO,
	 RRCP_OPCODE_HELLO, src, dst, authkey, NULL, NULL);
}

int init_rrcp_get_packet(
    struct rrcp_packet_t *t,
    const uint8_t *src,
    const uint8_t *dst,
    const uint16_t *authkey,
    unsigned reg_addr)
{
   return init_rrcp_packet(t, RTL_RRCP_PROTO,
	 RRCP_OPCODE_GET, src, dst, authkey, &reg_addr, NULL);
}

int init_rrcp_set_packet(
    struct rrcp_packet_t *t,
    const uint8_t *src,
    const uint8_t *dst,
    const uint16_t *authkey,
    unsigned reg_addr,
    unsigned reg_data)
{
   return init_rrcp_packet(t, RTL_RRCP_PROTO,
	 RRCP_OPCODE_SET, src, dst, authkey, &reg_addr, &reg_data);
}

int init_rep_packet(
    struct rrcp_packet_t *t,
    const uint8_t *src)
{
   return init_rrcp_packet(t, RTL_REP_PROTO,
	 RRCP_OPCODE_HELLO, src, NULL, NULL, NULL, NULL);
}


int is_rtl_packet(const void *pkt, size_t len)
{
   struct rrcp_packet_t *p;
   p = (struct rrcp_packet_t *)pkt;
   if (len <= 14
	|| (p->ether_type!=RTL_ETHER_TYPE))
      return 0;

   switch (p->rrcp_proto) {
      case RTL_RRCP_PROTO:
	 if ((p->rrcp_opcode != RRCP_OPCODE_HELLO)
	       && (p->rrcp_opcode != RRCP_OPCODE_SET)
	       && (p->rrcp_opcode != RRCP_OPCODE_GET))
	    return 0;
	 break;
      case RTL_REP_PROTO:
	 if (p->rrcp_opcode != 0)
	    return 0;
	 break;
      case RTL_LOOPDETECT_PROTO:
	 break;
      default:
	 return 0;
   }

   return 1;
}


