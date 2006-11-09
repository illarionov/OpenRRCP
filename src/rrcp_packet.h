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

#include <sys/types.h>

#define ETH_ALEN 6
struct rrcp_packet_t
{
  u_int8_t  ether_dhost[ETH_ALEN];      /* destination eth addr */
  u_int8_t  ether_shost[ETH_ALEN];      /* source ether addr    */
  u_int16_t ether_type;                 /* must be 0x8899 */
  u_int8_t  rrcp_proto;			/* must be 0x01         */
  u_int8_t  rrcp_opcode:7;              /* 0x00 = hello, 0x01 = get, 0x02 = set */
  u_int8_t  rrcp_isreply:1;             /* 0 = request to switch, 1 = reply from switch */
  u_int16_t rrcp_authkey;		/* 0x2379 by default */
  u_int16_t rrcp_reg_addr;		/* register address */
  u_int32_t rrcp_reg_data;		/* register data */
  u_int32_t stub;
};

struct rrcp_helloreply_packet_t
{
  u_int8_t  ether_dhost[ETH_ALEN];      /* destination eth addr */
  u_int8_t  ether_shost[ETH_ALEN];      /* source ether addr    */
  u_int16_t ether_type;                 /* must be 0x8899 */
  u_int8_t  rrcp_proto;			/* must be 0x01         */
  u_int8_t  rrcp_opcode:7;              /* 0x00 = hello, 0x01 = get, 0x02 = set */
  u_int8_t  rrcp_isreply:1;             /* 0 = request to switch, 1 = reply from switch */
  u_int16_t rrcp_authkey;		/* 0x2379 by default */
  u_int8_t  rrcp_downlink_port;		/*  */
  u_int8_t  rrcp_uplink_port;		/*  */
  u_int8_t  rrcp_uplink_mac[ETH_ALEN];  /*  */
  u_int16_t rrcp_chip_id;		/*  */
  u_int32_t rrcp_vendor_id;		/*  */
  u_int32_t stub;
};
