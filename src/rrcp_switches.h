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

//#include <sys/types.h>
#include <stdint.h>

//chip types
#define unknown 0
#define rtl8326 1
#define rtl8316b 2
#define rtl8318 3
#define rtl8324 4
#define rtl8316bp 5
#define rtl8324p 6

struct switchtype_t {
	const char		*vendor;
	const char		*model;
	const char		*hw_rev;
	uint32_t	r_vendor_id[3];
	uint16_t	r_chip_id;
	const char		*chip_name;
	unsigned int	chip_id;
	unsigned int	num_ports;
	const unsigned int	*port_order;
        const int		*reg2eeprom;
        const int		*regdefval;
	const char		*shortname;
	const char		*description;
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

extern const uint32_t chipname_n;
extern char* chipnames [7];

extern int switchtype;
extern const uint32_t switchtype_n;
extern struct switchtype_t switchtypes[];

//uint16_t rrcp_switch_autodetect_chip(void);

//uint16_t rrcp_switch_autodetect(void);

uint16_t rrcp_autodetect_switch_chip_eeprom(uint8_t *switch_type, uint8_t *chip_type, t_eeprom_type *eeprom_type);

int rrcp_get_switch_id_by_short_name(const char *str);

