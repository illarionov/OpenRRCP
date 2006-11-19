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

#include <string.h>
#include "../lib/libcli.h"
#include "rrcp_config.h"
#include "rrcp_switches.h"
#include "rrcp_cli_cmd_show.h"

#define MODE_CONFIG_INT		10

int cmd_rate_limit(struct cli_def *cli, char *command, char *argv[], int argc)
{
    int rate=0;
    if (strncasecmp("no ", command,3)==0){
	rate=100;
    }else if ((argc==0)||(strcmp(argv[0], "?") == 0))
    {
	cli_print(cli, "  128K 256K 512K 1M 2M 4M 8M 100M");
	return CLI_OK;
    }
    return CLI_OK;
}

int cmd_config_int(struct cli_def *cli, char *command, char *argv[], int argc)
{
	if (argc < 1)
	{
		cli_print(cli, "Specify an interface to configure");
		return CLI_OK;
	}
	if (strcmp(argv[0], "?") == 0)
	{
		cli_print(cli, "  Fa0/1");
		return CLI_OK;
	}
	else if (strcasecmp(argv[0], "fa0/1") == 0)
	{
		cli_set_configmode(cli, MODE_CONFIG_INT, "0/1");
	}
	else
		cli_print(cli, "Unknown interface %s", argv[0]);
	return CLI_OK;
}

int cmd_config_version(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc==1){
	if (strcmp(switchtypes[switchtype].chip_name,argv[0])!=0){
	    cli_print(cli, "%%SYS-4-CONFIG_MISMATCH: Configuration from chip '%s' may not be correctly understood on current chip '%s'", argv[0], switchtypes[switchtype].chip_name);
	}
    }
    return CLI_OK;
}

int cmd_config_mac_aging(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc==1){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <0>        Enter 0 to disable aging");
	    cli_print(cli, "  <12|300>   Aging time in seconds");
	}else if (strcmp(argv[0],"0")==0){
	    swconfig.alt.s.alt_config.mac_aging_disable=1;
	}else if (strcmp(argv[0],"12")==0){
	    swconfig.alt.s.alt_config.mac_aging_disable=0;
	    swconfig.alt.s.alt_config.mac_aging_fast=1;
	}else if (strcmp(argv[0],"300")==0){
	    swconfig.alt.s.alt_config.mac_aging_disable=0;
	    swconfig.alt.s.alt_config.mac_aging_fast=0;
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }
    return CLI_OK;
}

int cmd_config_rrcp(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	cli_print(cli, "%% Invalid input detected.");
    }else{
	if (strcasecmp(command,"rrcp enable")==0)    swconfig.rrcp_config.config.rrcp_disable=0;
	if (strcasecmp(command,"no rrcp enable")==0) swconfig.rrcp_config.config.rrcp_disable=1;
	if (strcasecmp(command,"rrcp echo enable")==0)    swconfig.rrcp_config.config.echo_disable=0;
	if (strcasecmp(command,"no rrcp echo enable")==0) swconfig.rrcp_config.config.echo_disable=1;
	if (strcasecmp(command,"rrcp loop-detect enable")==0)    swconfig.rrcp_config.config.loop_enable=1;
	if (strcasecmp(command,"no rrcp loop-detect enable")==0) swconfig.rrcp_config.config.loop_enable=0;
    }
    return CLI_OK;
}

int cmd_config_int_exit(struct cli_def *cli, char *command, char *argv[], int argc)
{
	cli_set_configmode(cli, MODE_CONFIG, NULL);
	return CLI_OK;
}

int cmd_dummytest(struct cli_def *cli, char *command, char *argv[], int argc)
{
    int i;
    cli_print(cli, "called %s with \"%s\"", __FUNCTION__, command);
    cli_print(cli, "%d arguments:", argc);
    for (i = 0; i < argc; i++)
    {
	cli_print(cli, "	%s", argv[i]);
    }
    return CLI_OK;
}

void cmd_config_register_commands(struct cli_def *cli)
{
    struct cli_command *c,*c2,*c3,*no,*noi;

    no=cli_register_command(cli, NULL, "no", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Negate a command or set its defaults");

    cli_register_command(cli, NULL, "version", cmd_config_version, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "");

    c=cli_register_command(cli, NULL, "mac-address-table", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Configure the MAC address table");
    cli_register_command(cli, c, "aging-time", cmd_config_mac_aging, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Set MAC address table entry maximum age");

    c=cli_register_command(cli, NULL, "rrcp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP configuration subcommand");
    cli_register_command(cli, c, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP");
    c2=cli_register_command(cli, no, "rrcp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP configuration subcommand");
    cli_register_command(cli, c2, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP");

    c3=cli_register_command(cli, c, "echo", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP Echo Protocol configuration subcommand");
    cli_register_command(cli, c3, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP Echo Protocol");
    c3=cli_register_command(cli, c2, "echo", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP Echo Protocol configuration subcommand");
    cli_register_command(cli, c3, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP Echo Protocol");

    c3=cli_register_command(cli, c, "loop-detect", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP-based loop detection configuration subcommand");
    cli_register_command(cli, c3, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP-based loop detection");
    c3=cli_register_command(cli, c2, "loop-detect", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP-based loop detection configuration subcommand");
    cli_register_command(cli, c3, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP-based loop detection");


    cli_register_command(cli, NULL, "interface", cmd_config_int, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Configure an interface");

    // Interface config mode
    noi=cli_register_command(cli, NULL, "no", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Negate a command or set its defaults");
    cli_register_command(cli, NULL, "shutdown", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Shutdown the selected interface");
    cli_register_command(cli, noi, "shutdown", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Shutdown the selected interface");

    {
	struct cli_command *switchport,*switchport_mode,*switchport_access,*switchport_trunk,*switchport_trunk_allowed,*switchport_trunk_native;
	switchport=cli_register_command(cli, NULL, "switchport", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set switching mode characteristics");
	switchport_mode=cli_register_command(cli, switchport, "mode", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode of the interface");
	cli_register_command(cli, switchport_mode, "access", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode to ACCESS unconditionally");
	cli_register_command(cli, switchport_mode, "trunk", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode to TRUNK unconditionally");  
	switchport_access=cli_register_command(cli, switchport, "access", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set access mode characteristics of the interface");
	cli_register_command(cli, switchport, "vlan", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set VLAN when interface is in access mode");
	switchport_trunk=cli_register_command(cli, switchport, "trunk", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking characteristics of the interface");
	switchport_trunk_allowed=cli_register_command(cli, switchport_trunk, "allowed", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set allowed VLAN characteristics when interface is in trunking mode");
	cli_register_command(cli, switchport_trunk_allowed, "vlan", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set allowed VLANs when interface is in trunking mode");
	switchport_trunk_native=cli_register_command(cli, switchport_trunk, "allowed", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking native characteristics when interface is in trunking mode");
	cli_register_command(cli, switchport_trunk_native, "vlan", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set native VLAN when interface is in trunking mode");
    }
    {
	struct cli_command *rate_limit;
	rate_limit=cli_register_command(cli, NULL, "rate-limit", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate Limit");
	cli_register_command(cli, rate_limit, "input", cmd_rate_limit, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate limit on input");
	cli_register_command(cli, rate_limit, "output", cmd_rate_limit, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate limit on output");
	rate_limit=cli_register_command(cli, noi, "rate-limit", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate Limit");
	cli_register_command(cli, rate_limit, "input", cmd_rate_limit, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate limit on input");
	cli_register_command(cli, rate_limit, "output", cmd_rate_limit, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Rate limit on output");
    }
    {
	struct cli_command *mac,*learning;
	mac=cli_register_command(cli, NULL, "mac", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC interface commands");
	learning=cli_register_command(cli, mac, "learning", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC address learning control");
	cli_register_command(cli, learning, "enable", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable MAC address learning on this port");
	mac=cli_register_command(cli, noi, "mac", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC interface commands");
	learning=cli_register_command(cli, mac, "learning", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC address learning control");
	cli_register_command(cli, learning, "enable", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable MAC address learning on this port");
    }
    {
	struct cli_command *rrcp;
	rrcp=cli_register_command(cli, NULL, "rrcp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "RRCP protocol control");
	cli_register_command(cli, rrcp, "enable", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable RRCP protocol on this port");
	rrcp=cli_register_command(cli, noi, "rrcp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "RRCP protocol control");
	cli_register_command(cli, rrcp, "enable", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable RRCP protocol on this port");
    }
    {
	struct cli_command *mls,*qos;
	mls=cli_register_command(cli, NULL, "mls", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "mls interface commands");
	qos=cli_register_command(cli, mls, "qos", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "qos command keyword");
	cli_register_command(cli, qos, "cos", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "cos keyword");
    }
    {
	struct cli_command *speed;
	speed=cli_register_command(cli, NULL, "speed", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Configure port speed operation");
	cli_register_command(cli, speed, "10", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force 10 Mbps operation");
	cli_register_command(cli, speed, "100", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force 100 Mbps operation");
	cli_register_command(cli, speed, "auto", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable AUTO speed configuration");
    }
    {
	struct cli_command *duplex;
	duplex=cli_register_command(cli, NULL, "duplex", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Configure duplex operation");
	cli_register_command(cli, duplex, "auto", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable AUTO speed configuration");
	cli_register_command(cli, duplex, "full", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force full duplex operation");
	cli_register_command(cli, duplex, "half", cmd_dummytest, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force half-duplex operation");
    }
}
