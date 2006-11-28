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

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include "../lib/libcli.h"
#include "rrcp_io.h"
#include "rrcp_config.h"
#include "rrcp_switches.h"
#include "rrcp_cli_cmd_show.h"

#define MODE_CONFIG_INT		10

int cmd_config_int(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc < 1){
	cli_print(cli, "Specify an interface to configure");
	return CLI_OK;
    }
    if (strcmp(argv[0], "?") == 0){
	int i;
	char s1[30],s2[30];
	for(i=0;i<switchtypes[switchtype].num_ports;i+=2){
	    cli_print(cli,"%-19s %-19s",rrcp_config_get_portname(s1, sizeof(s1), i+1, i),rrcp_config_get_portname(s2, sizeof(s2), i+2, i+1));
	}
	cli_print(cli,"<%d-%d> - reference interface by its number",1,i);
	return CLI_OK;
    }else{
	char *a=argv[0];
	int int_num=0;
	char s[10];
	if ((strlen(a)>0)&&('0'<=(a[strlen(a)-1]))&&((a[strlen(a)-1])<='9'))
	    int_num+=a[strlen(a)-1]-'0';
	if ((strlen(a)>1)&&('0'<=(a[strlen(a)-2]))&&((a[strlen(a)-2])<='9'))
	    int_num+=10*(a[strlen(a)-2]-'0');
	if (int_num>0 && int_num<=switchtypes[switchtype].num_ports){
	    sprintf(s,"0/%d",int_num);
	    cli_set_configmode(cli, MODE_CONFIG, NULL);
	    cli_set_configmode(cli, MODE_CONFIG_INT, s);
	}else{
	    cli_print(cli, "Unknown interface %s", argv[0]);
	}
    }
    return CLI_OK;
}

int cmd_config_int_shutdown(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "<CR>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	int port,port_phys;
	port=atoi(strrchr(cli->modestring,'/')+1);
	port_phys=map_port_number_from_logical_to_physical(port);
	if (strcasecmp(command,"shutdown")==0) swconfig.port_disable.bitmap|=(1<<port_phys);
	if (strcasecmp(command,"no shutdown")==0) swconfig.port_disable.bitmap&=(~(1<<port_phys));
    }
    return CLI_OK;
}

int cmd_config_int_switchport(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "<CR>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	cli_print(cli, "%% Not implemented yet.");
    }
    return CLI_OK;
}

int cmd_rate_limit(struct cli_def *cli, char *command, char *argv[], int argc)
{
    int rate=-1;
    if (strncasecmp("no ", command,3)==0){
	rate=0;
    }else{
	int i;
	if (argc>0) {
	    for (i=0;i<8;i++){
		if (strcasecmp(argv[0],bandwidth_text[i])==0){
		    rate=i;
		}
	    }
	}
    }
    if (rate<0){    
	if ( (argc>0) && (strcmp(argv[0], "?")!=0) ) {
	    cli_print(cli, "%% Invalid or unsupported bandwidth specification. List of available follows:");
	}
	cli_print(cli, "  128K 256K 512K 1M 2M 4M 8M 100M");
	return CLI_OK;
    }else{
	int port,port_phys;
	port=atoi(strrchr(cli->modestring,'/')+1);
	port_phys=map_port_number_from_logical_to_physical(port);
	if (strcasestr(command," input")){
	    swconfig.bandwidth.rxtx[port_phys].rx=rate;
	}else{
	    swconfig.bandwidth.rxtx[port_phys].tx=rate;
	}
    }
    return CLI_OK;
}

int cmd_config_int_mac_learning(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "<CR>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	cli_print(cli, "%% Not implemented yet.");
    }
    return CLI_OK;
}

int cmd_config_int_rrcp(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "<CR>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	cli_print(cli, "%% Not implemented yet.");
    }
    return CLI_OK;
}

int cmd_config_int_mls(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "<CR>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	cli_print(cli, "%% Not implemented yet.");
    }
    return CLI_OK;
}

int cmd_config_int_speed_duplex(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "<CR>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	cli_print(cli, "%% Not implemented yet.");
    }
    return CLI_OK;
}

int cmd_config_int_exit(struct cli_def *cli, char *command, char *argv[], int argc)
{
	cli_set_configmode(cli, MODE_CONFIG, NULL);
	return CLI_OK;
}

void cmd_config_int_register_commands(struct cli_def *cli)
{
    struct cli_command *no;

    cli_register_command(cli, NULL, "interface", cmd_config_int, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Configure an interface");
    cli_register_command(cli, NULL, "interface", cmd_config_int, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Configure another interface");

    // Interface config mode
    no=cli_register_command(cli, NULL, "no", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Negate a command or set its defaults");
    cli_register_command(cli, NULL, "shutdown", cmd_config_int_shutdown, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Shutdown the selected interface");
    cli_register_command(cli, no, "shutdown", cmd_config_int_shutdown, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Shutdown the selected interface");

    { // will rewrite this later - need to find out how to configure port-based VLANs
	struct cli_command *switchport,*switchport_mode,*switchport_access,*switchport_trunk,*switchport_trunk_allowed,*switchport_trunk_native;
	switchport=cli_register_command(cli, NULL, "switchport", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set switching mode characteristics");
	switchport_mode=cli_register_command(cli, switchport, "mode", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode of the interface");
	cli_register_command(cli, switchport_mode, "access", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode to ACCESS unconditionally");
	cli_register_command(cli, switchport_mode, "trunk", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode to TRUNK unconditionally");  
	switchport_access=cli_register_command(cli, switchport, "access", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set access mode characteristics of the interface");
	cli_register_command(cli, switchport, "vlan", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set VLAN when interface is in access mode");
	switchport_trunk=cli_register_command(cli, switchport, "trunk", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking characteristics of the interface");
	switchport_trunk_allowed=cli_register_command(cli, switchport_trunk, "allowed", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set allowed VLAN characteristics when interface is in trunking mode");
	cli_register_command(cli, switchport_trunk_allowed, "vlan", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set allowed VLANs when interface is in trunking mode");
	switchport_trunk_native=cli_register_command(cli, switchport_trunk, "allowed", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking native characteristics when interface is in trunking mode");
	cli_register_command(cli, switchport_trunk_native, "vlan", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set native VLAN when interface is in trunking mode");
    }
    {
	struct cli_command *rate_limit;
	rate_limit=cli_register_command(cli, NULL, "rate-limit", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate Limit");
	cli_register_command(cli, rate_limit, "input", cmd_rate_limit, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate limit on input");
	cli_register_command(cli, rate_limit, "output", cmd_rate_limit, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate limit on output");
	rate_limit=cli_register_command(cli, no, "rate-limit", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate Limit");
	cli_register_command(cli, rate_limit, "input", cmd_rate_limit, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate limit on input");
	cli_register_command(cli, rate_limit, "output", cmd_rate_limit, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate limit on output");
    }
    {
	struct cli_command *mac;
	mac=cli_register_command(cli, NULL, "mac-learn", cmd_config_int_mac_learning, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC interface commands");
	cli_register_command(cli, mac, "disable", cmd_config_int_mac_learning, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC address learning control");
	mac=cli_register_command(cli, no, "mac-learn", cmd_config_int_mac_learning, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC interface commands");
	cli_register_command(cli, mac, "disable", cmd_config_int_mac_learning, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC address learning control");
    }
    {
	struct cli_command *rrcp;
	rrcp=cli_register_command(cli, NULL, "rrcp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "RRCP protocol control");
	cli_register_command(cli, rrcp, "enable", cmd_config_int_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable RRCP protocol on this port");
	rrcp=cli_register_command(cli, no, "rrcp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "RRCP protocol control");
	cli_register_command(cli, rrcp, "enable", cmd_config_int_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable RRCP protocol on this port");
    }
    {
	struct cli_command *mls,*qos;
	mls=cli_register_command(cli, NULL, "mls", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "mls interface commands");
	qos=cli_register_command(cli, mls, "qos", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Quality-of-Service sub-commands");
	cli_register_command(cli, qos, "cos", cmd_config_int_mls, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Override Class-of-Service field of incoming packet");
    }
    {
	struct cli_command *speed;
	speed=cli_register_command(cli, NULL, "speed", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Configure port speed operation");
	cli_register_command(cli, speed, "10", cmd_config_int_speed_duplex, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force 10 Mbps operation");
	cli_register_command(cli, speed, "100", cmd_config_int_speed_duplex, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force 100 Mbps operation");
	cli_register_command(cli, speed, "auto", cmd_config_int_speed_duplex, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable AUTO speed configuration");
    }
    {
	struct cli_command *duplex;
	duplex=cli_register_command(cli, NULL, "duplex", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Configure duplex operation");
	cli_register_command(cli, duplex, "auto", cmd_config_int_speed_duplex, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable AUTO speed configuration");
	cli_register_command(cli, duplex, "full", cmd_config_int_speed_duplex, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force full duplex operation");
	cli_register_command(cli, duplex, "half", cmd_config_int_speed_duplex, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force half-duplex operation");
    }
}
