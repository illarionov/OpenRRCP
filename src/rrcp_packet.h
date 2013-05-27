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

#ifdef __linux__
#include <endian.h>
#else
#include <sys/endian.h>
#define __BYTE_ORDER _BYTE_ORDER
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#endif

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#define RTL_ETHER_TYPE	      (htons(0x8899))
#define RRCP_DEFAULT_AUTHKEY  (htons(0x2379))

#define RTL_RRCP_PROTO	0x01
#define RTL_REP_PROTO	0x02
#define RTL_LOOPDETECT_PROTO	0x03

#define RRCP_OPCODE_HELLO	0x00
#define RRCP_OPCODE_GET		0x01
#define RRCP_OPCODE_SET		0x02

struct rrcp_packet_t
{
  uint8_t  ether_dhost[ETH_ALEN];      /* destination eth addr */
  uint8_t  ether_shost[ETH_ALEN];      /* source ether addr    */
  uint16_t ether_type;                 /* must be 0x8899 */
  uint8_t  rrcp_proto;			/* must be 0x01         */
#if __BYTE_ORDER == __BIG_ENDIAN
  uint8_t  rrcp_isreply:1;             /* 0 = request to switch, 1 = reply from switch */
  uint8_t  rrcp_opcode:7;              /* 0x00 = hello, 0x01 = get, 0x02 = set */
#else
  uint8_t  rrcp_opcode:7;              /* 0x00 = hello, 0x01 = get, 0x02 = set */
  uint8_t  rrcp_isreply:1;             /* 0 = request to switch, 1 = reply from switch */
#endif
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
#if __BYTE_ORDER == __BIG_ENDIAN
  uint8_t  rrcp_isreply:1;             /* 0 = request to switch, 1 = reply from switch */
  uint8_t  rrcp_opcode:7;              /* 0x00 = hello, 0x01 = get, 0x02 = set */
#else
  uint8_t  rrcp_opcode:7;              /* 0x00 = hello, 0x01 = get, 0x02 = set */
  uint8_t  rrcp_isreply:1;             /* 0 = request to switch, 1 = reply from switch */
#endif
  uint16_t rrcp_authkey;		/* 0x2379 by default */
  uint8_t  rrcp_downlink_port;		/*  */
  uint8_t  rrcp_uplink_port;		/*  */
  uint8_t  rrcp_uplink_mac[ETH_ALEN];  /*  */
  uint16_t rrcp_chip_id;		/*  */
  uint32_t rrcp_vendor_id;		/*  */
  uint32_t stub;
};

int init_rrcp_hello_packet(
    struct rrcp_packet_t *t,
    const uint8_t *src,
    const uint8_t *dst,
    const uint16_t *authkey);

int init_rrcp_get_packet(
    struct rrcp_packet_t *t,
    const uint8_t *src,
    const uint8_t *dst,
    const uint16_t *authkey,
    const unsigned reg_addr);

int init_rrcp_set_packet(
    struct rrcp_packet_t *t,
    const uint8_t *src,
    const uint8_t *dst,
    const uint16_t *authkey,
    const unsigned reg_addr,
    const unsigned reg_data);

int init_rep_packet(
    struct rrcp_packet_t *t,
    const uint8_t *src);

int is_rtl_packet(const void *pkt, size_t len);

