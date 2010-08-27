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

    Some support can be found at: http://openrrcp.org.ru/
*/

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include "../lib/libcli.h"
#include "rrcp_lib.h"
#include "rrcp_io.h"
#include "rrcp_config.h"
#include "rrcp_switches.h"
#include "rrcp_cli_cmd_show.h"
#include "rrcp_cli_cmd_config.h"

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
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	int port,port_phys;
	port=atoi(strrchr(cli->modestring,'/')+1);
	port_phys=map_port_number_from_logical_to_physical(port);
	if (strcasecmp(command,"shutdown")==0){
	    swconfig.port_disable.bitmap |= (1<<port_phys);
	    rtl83xx_setreg16(0x0608+0,swconfig.port_disable.raw[0]);
	    rtl83xx_setreg16(0x0608+1,swconfig.port_disable.raw[1]);
	}else if (strcasecmp(command,"no shutdown")==0){
	    swconfig.port_disable.bitmap &= (~(1<<port_phys));
	    rtl83xx_setreg16(0x0608+0,swconfig.port_disable.raw[0]);
	    rtl83xx_setreg16(0x0608+1,swconfig.port_disable.raw[1]);
	}else
	    cli_print(cli, "Internal error on command '%s'",command);
    }
    return CLI_OK;
}

int cmd_config_int_switchport(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	int i,port,port_phys;
	if (!swconfig.vlan.s.config.dot1q){
	    cli_print(cli, "%% Cannot set up switchport properties in current vlan mode.");
	    cli_print(cli, "%% Please, issue 'vlan dot1q' in global config mode first.");
	}
	port=atoi(strrchr(cli->modestring,'/')+1);
	port_phys=map_port_number_from_logical_to_physical(port);
	if (strcasecmp(command,"switchport mode trunk")==0){
	    for(i=0;i<32;i++){
                swconfig.vlan_entry.bitmap[i]&=~(1<<port_phys);
            }
	    swconfig.vlan_port_insert_vid.bitmap |= (1<<port_phys);
	    swconfig.vlan_port_output_tag.bitmap |= (1<<port_phys);
	    rrcp_config_commit_vlan_to_switch();
	}else if (strcasecmp(command,"switchport mode access")==0){
	    for(i=0;i<32;i++){
                swconfig.vlan_entry.bitmap[i]&=~(1<<port_phys);
            }
	    swconfig.vlan_port_insert_vid.bitmap &= (~(1<<port_phys));
	    swconfig.vlan_port_output_tag.bitmap &= (~(1<<port_phys));
	    rrcp_config_commit_vlan_to_switch();
	}else{
	    cli_print(cli, "Internal error on command '%s'",command);
	}
	return CLI_OK;
    }
    return CLI_OK;
}

int cmd_config_int_switchport_access_vlan(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <1-4094>  VLAN ID of the VLAN when this port is in access mode");
	}else{
	    int port,port_phys;
	    if (!swconfig.vlan.s.config.dot1q){
		cli_print(cli, "%% Cannot set up switchport properties in current vlan mode.");
		cli_print(cli, "%% Please, issue 'vlan dot1q' in global config mode first.");
	    }
	    port=atoi(strrchr(cli->modestring,'/')+1);
	    port_phys=map_port_number_from_logical_to_physical(port);
	    int vi=0;

	    vi=find_or_create_vlan_index_by_vid(atoi(argv[0]));
	    if (vi>=0){
		int i;
		swconfig.vlan.s.port_vlan_index[port_phys]=vi;
		for(i=0;i<32;i++){
		    swconfig.vlan_entry.bitmap[i]&=~(1<<port_phys);
		}
		swconfig.vlan_entry.bitmap[vi]|=1<<port_phys;
		rrcp_config_commit_vlan_to_switch();
	    }else{
		cli_print(cli, "%% Too many VLANs: This switch only supports 32.");
	    }
	}
	return CLI_OK;
    }else{
	cli_print(cli, "%% Invalid input detected.");
	return CLI_OK;
    }
}

int cmd_config_int_switchport_trunk_native_vlan(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <1-4094>  VLAN ID of the native VLAN when this port is in trunking mode");
	}else{
	    int port,port_phys;
	    if (!swconfig.vlan.s.config.dot1q){
		cli_print(cli, "%% Cannot set up switchport properties in current vlan mode.");
		cli_print(cli, "%% Please, issue 'vlan dot1q' in global config mode first.");
	    }
	    port=atoi(strrchr(cli->modestring,'/')+1);
	    port_phys=map_port_number_from_logical_to_physical(port);
	    int vi=0;

	    vi=find_or_create_vlan_index_by_vid(atoi(argv[0]));
	    if (vi>=0){
		swconfig.vlan.s.port_vlan_index[port_phys]=vi;
		rrcp_config_commit_vlan_to_switch();
	    }else{
		cli_print(cli, "%% Too many VLANs: This switch only supports 32.");
		return CLI_ERROR;
	    }
	}
	return CLI_OK;
    }else{
	cli_print(cli, "%% Invalid input detected.");
	return CLI_OK;
    }
}

int cmd_config_int_switchport_trunk_allowed_vlan(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  WORD    VLAN IDs of the allowed VLANs when this port is in trunking mode");
	    cli_print(cli, "  all     all VLANs");
	    cli_print(cli, "  none    no VLANs");
	}else{
	    int port,port_phys;
	    if (!swconfig.vlan.s.config.dot1q){
		cli_print(cli, "%% Cannot set up switchport properties in current vlan mode.");
		cli_print(cli, "%% Please, issue 'vlan dot1q' in global config mode first.");
	    }
	    port=atoi(strrchr(cli->modestring,'/')+1);
	    port_phys=map_port_number_from_logical_to_physical(port);
	    int vidlist[32];
	    int i,vi;

	    if (str_portlist_to_array_by_value(argv[0],vidlist,32)==0){
		for (i=0;(i<32)&&(vidlist[i]>0)&&(vidlist[i]<4095);i++){
		    vi=find_or_create_vlan_index_by_vid(vidlist[i]);
//		    printf("%d - %d - %d\n",i,vidlist[i],vi);
		    if (vi>=0){
			swconfig.vlan_entry.bitmap[vi]|=(1<<port_phys);
		    }else{
			cli_print(cli, "%% Too many VLANs: This swith only supports 32.");
			return CLI_ERROR;
		    }
		}
		rrcp_config_commit_vlan_to_switch();
	    }else{
		cli_print(cli, "%% Invalid input detected: Cannot recognize port list.");
		return CLI_ERROR;
	    }
	}
	return CLI_OK;
    }else{
	cli_print(cli, "%% Invalid input detected: Specify port list.");
	return CLI_ERROR;
    }
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
	rtl83xx_setreg16(0x020a+((port_phys/2)&0x0f),swconfig.bandwidth.raw[((port_phys/2)&0x0f)]);
    }
    return CLI_OK;
}

int cmd_config_int_mac_learning(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	int port,port_phys;
	port=atoi(strrchr(cli->modestring,'/')+1);
	port_phys=map_port_number_from_logical_to_physical(port);
	if (strcasecmp(command,"mac-learn disable")==0) {
	    swconfig.alt_mask.mask |= (1<<port_phys);
	} else if ((strcasecmp(command,"mac-learn enable")==0)||
	         (strcasecmp(command,"no mac-learn disable")==0)) {
	    swconfig.alt_mask.mask &= (~(1<<port_phys));
	} else {
	    cli_print(cli, "Internal error on command '%s'",command);
	    return CLI_ERROR;
	}
	rtl83xx_setreg16(0x0301+((port_phys/16)&3),swconfig.alt_mask.raw[((port_phys/16)&3)]);
    }
    return CLI_OK;
}

int cmd_config_int_rrcp(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	int port,port_phys;
	port=atoi(strrchr(cli->modestring,'/')+1);
	port_phys=map_port_number_from_logical_to_physical(port);
	if (strcasecmp(command,"rrcp enable")==0) {
	    swconfig.rrcp_byport_disable.bitmap &= (~(1<<port_phys));
	} else if (strcasecmp(command,"no rrcp enable")==0) {
	    swconfig.rrcp_byport_disable.bitmap |= (1<<port_phys);
	} else {
	    cli_print(cli, "Internal error on command '%s'",command);
	    return CLI_ERROR;
	}
	rtl83xx_setreg16(0x0201+((port_phys/16)&3),swconfig.rrcp_byport_disable.raw[((port_phys/16)&3)]);
    }
    return CLI_OK;
}

int cmd_config_int_mls(struct cli_def *cli, char *command, char *argv[], int argc)
{
    int port,port_phys;
    port=atoi(strrchr(cli->modestring,'/')+1);
    port_phys=map_port_number_from_logical_to_physical(port);
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "0 - Set CoS value in incoming packets to 0 (low prioriry)");
	    cli_print(cli, "7 - Set CoS value in incoming packets to 7 (high prioriry)");
	    return CLI_OK;
	}else if (strcmp(argv[0],"0")==0){
	    swconfig.qos_port_priority.bitmap &= (~(1<<port_phys));
	}else if (strcmp(argv[0],"7")==0){
	    swconfig.qos_port_priority.bitmap |= (1<<port_phys);
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	    return CLI_ERROR;
	}
	rtl83xx_setreg16(0x0401,swconfig.qos_port_priority.raw[0]);
	rtl83xx_setreg16(0x0402,swconfig.qos_port_priority.raw[1]);
    }else{
	cli_print(cli, "%% Specify new CoS value (0 or 7)");
    }
    return CLI_OK;
}

int cmd_config_int_speed_duplex(struct cli_def *cli, char *command, char *argv[], int argc)
{
    // Note: a software reset of whole switch is required to re-negotiate speed/duplex on any port
    int port,port_phys;
    port=atoi(strrchr(cli->modestring,'/')+1);
    port_phys=map_port_number_from_logical_to_physical(port);

    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	if (strcasecmp(command,"speed 10")==0){
	    swconfig.port_config.config[port_phys].autoneg=0;
	    swconfig.port_config.config[port_phys].media_10half=0;
	    swconfig.port_config.config[port_phys].media_10full=1;
	    swconfig.port_config.config[port_phys].media_100half=0;
	    swconfig.port_config.config[port_phys].media_100full=0;
	    swconfig.port_config.config[port_phys].media_1000full=0;
	}else if (strcasecmp(command,"speed 100")==0){
	    swconfig.port_config.config[port_phys].autoneg=0;
	    swconfig.port_config.config[port_phys].media_10half=0;
	    swconfig.port_config.config[port_phys].media_10full=0;
	    swconfig.port_config.config[port_phys].media_100half=0;
	    swconfig.port_config.config[port_phys].media_100full=1;
	    swconfig.port_config.config[port_phys].media_1000full=0;
	}else if (strcasecmp(command,"speed 1000")==0){
	    char s[32];
	    if (port_phys<24){
		cli_print(cli, "%% Gigabit speed is not supported on port %s",rrcp_config_get_portname(s, sizeof(s), port, port_phys));
	    }
	    swconfig.port_config.config[port_phys].autoneg=0;
	    swconfig.port_config.config[port_phys].media_10half=0;
	    swconfig.port_config.config[port_phys].media_10full=0;
	    swconfig.port_config.config[port_phys].media_100half=0;
	    swconfig.port_config.config[port_phys].media_100full=0;
	    swconfig.port_config.config[port_phys].media_1000full=1;
	}else if (strcasecmp(command,"speed auto")==0){
	    swconfig.port_config.config[port_phys].autoneg=1;
	    swconfig.port_config.config[port_phys].media_10half=1;
	    swconfig.port_config.config[port_phys].media_10full=1;
	    swconfig.port_config.config[port_phys].media_100half=1;
	    swconfig.port_config.config[port_phys].media_100full=1;
	    if (port_phys>=24){
		swconfig.port_config.config[port_phys].media_1000full=1;
	    }
	}else if (strcasestr(command,"duplex")==command){
	    if (swconfig.port_config.config[port_phys].autoneg){
		cli_print(cli, "%% Duplex can not be set until speed is set to non-auto value");
		return CLI_ERROR;
	    }
	    if (strcasecmp(command,"duplex half")==0){
		if (swconfig.port_config.config[port_phys].media_1000full && port_phys>=24){
		    cli_print(cli, "%% Half-duplex Gigabit mode is not supported");
		    return CLI_ERROR;
		}
		if (swconfig.port_config.config[port_phys].media_10full){
		    swconfig.port_config.config[port_phys].media_10full=0;
		    swconfig.port_config.config[port_phys].media_10half=1;
		}
		if (swconfig.port_config.config[port_phys].media_100full){
		    swconfig.port_config.config[port_phys].media_100full=0;
		    swconfig.port_config.config[port_phys].media_100half=1;
		}
	    }else if (strcasecmp(command,"duplex full")==0){
		if (swconfig.port_config.config[port_phys].media_10half){
		    swconfig.port_config.config[port_phys].media_10half=0;
		    swconfig.port_config.config[port_phys].media_10full=1;
		}
		if (swconfig.port_config.config[port_phys].media_100half){
		    swconfig.port_config.config[port_phys].media_100half=0;
		    swconfig.port_config.config[port_phys].media_100full=1;
		}
	    }else{
		cli_print(cli, "Internal error on command '%s'",command);
		return CLI_ERROR;
	    }
	}else{
	    cli_print(cli, "Internal error on command '%s'",command);
	    return CLI_ERROR;
	}
	rtl83xx_setreg16(0x060a+((port_phys/2)&0xf),swconfig.port_config.raw[((port_phys/2)&0xf)]);
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
	switchport=cli_register_command(cli, NULL, "switchport", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set switching mode characteristics");
	switchport_mode=cli_register_command(cli, switchport, "mode", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode of the interface");
	cli_register_command(cli, switchport_mode, "access", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode to ACCESS unconditionally");
	cli_register_command(cli, switchport_mode, "trunk", cmd_config_int_switchport, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking mode to TRUNK unconditionally");  
	switchport_access=cli_register_command(cli, switchport, "access", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set access mode characteristics of the interface");
	cli_register_command(cli, switchport_access, "vlan", cmd_config_int_switchport_access_vlan, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set VLAN when interface is in access mode");
	switchport_trunk=cli_register_command(cli, switchport, "trunk", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking characteristics of the interface");
	switchport_trunk_allowed=cli_register_command(cli, switchport_trunk, "allowed", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set allowed VLAN characteristics when interface is in trunking mode");
	cli_register_command(cli, switchport_trunk_allowed, "vlan", cmd_config_int_switchport_trunk_allowed_vlan, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set allowed VLANs when interface is in trunking mode");
	switchport_trunk_native=cli_register_command(cli, switchport_trunk, "native", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set trunking native characteristics when interface is in trunking mode");
	cli_register_command(cli, switchport_trunk_native, "vlan", cmd_config_int_switchport_trunk_native_vlan, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set native VLAN when interface is in trunking mode");
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
	cli_register_command(cli, mac, "enable", cmd_config_int_mac_learning, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "MAC address learning control");
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
	cli_register_command(cli, speed, "1000", cmd_config_int_speed_duplex, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force 1 Gbps operation");
	cli_register_command(cli, speed, "auto", cmd_config_int_speed_duplex, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Enable AUTO speed configuration");
    }
    {
	struct cli_command *duplex;
	duplex=cli_register_command(cli, NULL, "duplex", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Configure duplex operation");
	cli_register_command(cli, duplex, "full", cmd_config_int_speed_duplex, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force full duplex operation");
	cli_register_command(cli, duplex, "half", cmd_config_int_speed_duplex, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Force half-duplex operation");
    }
    cli_register_command(cli, NULL, "end", cmd_config_end, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Exit from configure mode");
}
