/*
    This file is part of OpenRRCP

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

#ifdef RTL83XX
#include "../lib/fake-libcli.h"
#else
#include "../lib/libcli.h"
#endif

int cmd_show_version(struct cli_def *cli, char *command, char *argv[], int argc);
int cmd_show_config(struct cli_def *cli, char *command, char *argv[], int argc);
#ifdef RTL83XX
int cmd_show_interfaces(struct cli_def *cli, char *command, char *argv[], int argc);
int cmd_show_ip_igmp_snooping(struct cli_def *cli, char *command, char *argv[], int argc);
int cmd_show_switch_register(struct cli_def *cli, char *command, char *argv[], int argc);
int cmd_show_eeprom_register(struct cli_def *cli, char *command, char *argv[], int argc);
int cmd_show_phy_register(struct cli_def *cli, char *command, char *argv[], int argc);
#else
void cmd_show_register_commands(struct cli_def *cli);
#endif
