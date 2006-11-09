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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "rrcp_packet.h"
#include "rrcp_io.h"
#include "rrcp_switches.h"
#include "rrcp_config.h"
#include "../lib/libcli.h"


#define CLITEST_PORT		8000

#define MODE_CONFIG_INT		10

int cmd_test(struct cli_def *cli, char *command, char *argv[], int argc)
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

int cmd_rate_limit(struct cli_def *cli, char *command, char *argv[], int argc)
{
    int i,rate=0;
    if (strncasecmp("no ", command,3)==0){
	rate=100;
    }else if ((argc==0)||(strcmp(argv[0], "?") == 0))
    {
	cli_print(cli, "  128K 256K 512K 1M 2M 4M 8M 100M");
	return CLI_OK;
    }
    return CLI_OK;
}

int cmd_set(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc < 2)
    {
	cli_print(cli, "Specify a variable to set");
	return CLI_OK;
    }
    cli_print(cli, "Setting \"%s\" to \"%s\"", argv[0], argv[1]);
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

int cmd_config_int_exit(struct cli_def *cli, char *command, char *argv[], int argc)
{
	cli_set_configmode(cli, MODE_CONFIG, NULL);
	return CLI_OK;
}

int check_auth(char *username, char *password)
{
    if (!strcasecmp(username, "1") && !strcasecmp(password, "1"))
	return 1;
    return 0;
}

int check_enable(char *password)
{
    if (!strcasecmp(password, "1"))
        return 1;
    return 0;
}

void pc(struct cli_def *cli, char *string)
{
	printf("%s\n", string);
}

int main(int argc, char *argv[])
{
    struct cli_command *c,*no;
    struct cli_def *cli;
    int s, x;
    struct sockaddr_in servaddr;
    int on = 1;

    cli = cli_init();
    cli_set_banner(cli, "libcli test environment");
    cli_set_hostname(cli, "router");
    c = cli_register_command(cli, NULL, "show", NULL,  PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
    cli_register_command(cli, c, "configuration", cmd_test, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Contents of Non-Volatile memory");
    cli_register_command(cli, c, "running-config", cmd_test, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Current operating configuration");
    cli_register_command(cli, c, "startup-config", cmd_test, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Contents of startup configuration");

    cli_register_command(cli, NULL, "interface", cmd_config_int, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Configure an interface");

    no=cli_register_command(cli, NULL, "no", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Negate a command or set its defaults");

    cli_register_command(cli, NULL, "shutdown", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Shutdown the selected interface");
    cli_register_command(cli, no, "shutdown", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Shutdown the selected interface");

    {
	struct cli_command *switchport,*switchport_mode,*switchport_access,*switchport_trunk,*switchport_trunk_allowed,*switchport_trunk_native;
	switchport=cli_register_command(cli, NULL, "switchport", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set switching mode characteristics");
	switchport_mode=cli_register_command(cli, switchport, "mode", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode of the interface");
	cli_register_command(cli, switchport_mode, "access", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode to ACCESS unconditionally");
	cli_register_command(cli, switchport_mode, "trunk", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode to TRUNK unconditionally");  
	switchport_access=cli_register_command(cli, switchport, "access", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set access mode characteristics of the interface");
	cli_register_command(cli, switchport, "vlan", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set VLAN when interface is in access mode");
	switchport_trunk=cli_register_command(cli, switchport, "trunk", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking characteristics of the interface");
	switchport_trunk_allowed=cli_register_command(cli, switchport_trunk, "allowed", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set allowed VLAN characteristics when interface is in trunking mode");
	cli_register_command(cli, switchport_trunk_allowed, "vlan", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set allowed VLANs when interface is in trunking mode");
	switchport_trunk_native=cli_register_command(cli, switchport_trunk, "allowed", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking native characteristics when interface is in trunking mode");
	cli_register_command(cli, switchport_trunk_native, "vlan", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set native VLAN when interface is in trunking mode");
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
	struct cli_command *mac,*learning;
	mac=cli_register_command(cli, NULL, "mac", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC interface commands");
	learning=cli_register_command(cli, mac, "learning", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC address learning control");
	cli_register_command(cli, learning, "enable", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable MAC address learning on this port");
	mac=cli_register_command(cli, no, "mac", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC interface commands");
	learning=cli_register_command(cli, mac, "learning", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC address learning control");
	cli_register_command(cli, learning, "enable", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable MAC address learning on this port");
    }
    {
	struct cli_command *rrcp;
	rrcp=cli_register_command(cli, NULL, "rrcp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "RRCP protocol control");
	cli_register_command(cli, rrcp, "enable", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable RRCP protocol on this port");
	rrcp=cli_register_command(cli, no, "rrcp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "RRCP protocol control");
	cli_register_command(cli, rrcp, "enable", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable RRCP protocol on this port");
    }
    {
	struct cli_command *mls,*qos;
	mls=cli_register_command(cli, NULL, "mls", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "mls interface commands");
	qos=cli_register_command(cli, mls, "qos", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "qos command keyword");
	cli_register_command(cli, qos, "cos", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "cos keyword");
    }
    {
	struct cli_command *speed;
	speed=cli_register_command(cli, NULL, "speed", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Configure port speed operation");
	cli_register_command(cli, speed, "10", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force 10 Mbps operation");
	cli_register_command(cli, speed, "100", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force 100 Mbps operation");
	cli_register_command(cli, speed, "auto", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable AUTO speed configuration");
    }
    {
	struct cli_command *duplex;
	duplex=cli_register_command(cli, NULL, "duplex", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Configure duplex operation");
	cli_register_command(cli, duplex, "auto", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable AUTO speed configuration");
	cli_register_command(cli, duplex, "full", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force full duplex operation");
	cli_register_command(cli, duplex, "half", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force half-duplex operation");
    }

//    cli_set_auth_callback(cli, check_auth);
//    cli_set_enable_callback(cli, check_enable);
    // Test reading from a file
    {
	    FILE *fh;

	    if ((fh = fopen("clitest.txt", "r")))
	    {
		    // This sets a callback which just displays the cli_print() text to stdout
		    cli_print_callback(cli, pc);
		    cli_file(cli, fh, PRIVILEGE_UNPRIVILEGED, MODE_EXEC);
		    cli_print_callback(cli, NULL);
		    fclose(fh);
	    }
    }

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
	perror("socket");
	return 1;
    }
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(CLITEST_PORT);
    if (bind(s, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
	perror("bind");
	return 1;
    }

    if (listen(s, 50) < 0)
    {
	perror("listen");
	return 1;
    }

    printf("Listening on port %d\n", CLITEST_PORT);
    while ((x = accept(s, NULL, 0)))
    {
	cli_loop(cli, x);
	close(x);
    }

    cli_done(cli);
    return 0;
}

