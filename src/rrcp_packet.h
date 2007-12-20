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

#include <stdint.h>

#ifndef ETH_ALEN 
#define ETH_ALEN 6
#endif

struct rrcp_packet_t
{
  uint8_t  ether_dhost[ETH_ALEN];      /* destination eth addr */
  uint8_t  ether_shost[ETH_ALEN];      /* source ether addr    */
  uint16_t ether_type;                 /* must be 0x8899 */
  uint8_t  rrcp_proto;			/* must be 0x01         */
  uint8_t  rrcp_opcode:7;              /* 0x00 = hello, 0x01 = get, 0x02 = set */
  uint8_t  rrcp_isreply:1;             /* 0 = request to switch, 1 = reply from switch */
  uint16_t rrcp_authkey;		/* 0x2379 by default */
  uint16_t rrcp_reg_addr;		/* register address */
  uint32_t rrcp_reg_data;		/* register data */
  uint32_t cookie1;
  uint32_t cookie2;
};

struct rrcp_helloreply_packet_t
{
  uint8_t  ether_dhost[ETH_ALEN];      /* destination eth addr */
  uint8_t  ether_shost[ETH_ALEN];      /* source ether addr    */
  uint16_t ether_type;                 /* must be 0x8899 */
  uint8_t  rrcp_proto;			/* must be 0x01         */
  uint8_t  rrcp_opcode:7;              /* 0x00 = hello, 0x01 = get, 0x02 = set */
  uint8_t  rrcp_isreply:1;             /* 0 = request to switch, 1 = reply from switch */
  uint16_t rrcp_authkey;		/* 0x2379 by default */
  uint8_t  rrcp_downlink_port;		/*  */
  uint8_t  rrcp_uplink_port;		/*  */
  uint8_t  rrcp_uplink_mac[ETH_ALEN];  /*  */
  uint16_t rrcp_chip_id;		/*  */
  uint32_t rrcp_vendor_id;		/*  */
  uint32_t stub;
};
