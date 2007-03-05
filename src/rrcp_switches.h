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

//#include <sys/types.h>
#include <stdint.h>

//chip types
#define unknown 0
#define rtl8316b 1
#define rtl8326 2

struct switchtype_t {
	char		*vendor;
	char		*model;
	char		*hw_rev;
	char		*chip_name;
	unsigned int	chip_id;
	unsigned int	num_ports;
	unsigned int	port_order[27];
};

struct t_rtl83xx_port_link_status {
	uint8_t	speed:2,
  		duplex:1,
  		reserved:1,
  		link:1,
  		flow_control:1,
  		asy_pause:1,
  		auto_negotiation:1;
};

union t_vlan_port_vlan {
        uint8_t  index[26];
        uint16_t raw[13];
};

union t_vlan_port_output_tag {
        uint64_t bitmap;
        uint16_t raw[4];
};

union t_vlan_entry {
        uint32_t bitmap[32];
        uint16_t raw[64];
};

union t_vlan_port_insert_vid {
        uint32_t bitmap;
        uint16_t raw[2];
};

union t_rrcp_status {
        struct {
            uint16_t  low;
            uint16_t  high;
        } doubleshort;
        uint32_t signlelong;
} ;

extern const uint32_t chipname_n;
extern char* chipnames [3];

extern uint32_t switchtype;
extern const uint32_t switchtype_n;
extern struct switchtype_t switchtypes[6];

uint16_t rrcp_switch_autodetect_chip(void);

uint16_t rrcp_switch_autodetect(void);
