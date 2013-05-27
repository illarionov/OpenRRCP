/*
    This file is part of OpenRRCP

    Copyright (c) 2009 Alexey Illarionov <littlesavage@rambler.ru>

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

*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "rrcp_config.h"
#include "rrcp_switches.h"
#include "rrcp_lib.h"

enum t_cmd_enum {
   CMD_NULL,
   CMD_SET_VERSION,
   CMD_SET_HOSTNAME,
   CMD_SET_MAC_ADDRESS_TABLE_AGING_TIME,
   CMD_SET_IP_IGMP_SNOOPING,
   CMD_SET_RRCP,
   CMD_SET_AUTHKEY,
   CMD_SET_VENDER_ID,
   CMD_SET_CHIP_ID,
   CMD_SET_MAC_ADDRESS,
   CMD_SET_LEAKY_VLAN,
   CMD_SET_GLOBAL_VLAN,
   CMD_SET_IP_MLS_QOS,
   CMD_SET_WRR_QUEUE_RATIO,
   CMD_SET_FLOW_CONTROL,
   CMD_SET_STOM_CONTROL_BROADCAST,
   CMD_SET_STORM_CONTROL_MULTICAST,
   CMD_SET_SPANNING_TREE_BPDU_FILTER,
   CMD_SET_INTERFACE_SHUTDOWN,
   CMD_SET_INTERFACE_RATE_LIMIT,
   CMD_SET_INTERFACE_MAC_LEARNING,
   CMD_SET_INTERFACE_RRCP,
   CMD_SET_INTERFACE_PRIORITY,
   CMD_SET_INTERFACE_SPEED,
   CMD_SET_INTERFACE_DUPLEX,
   CMD_SET_INTERFACE_SWITCHPORT_MODE,
   CMD_SET_INTERFACE_ACCESS_VLAN,
   CMD_SET_INTERFACE_TRUNK_NATIVE_VLAN,
   CMD_SET_INTERFACE_TRUNK_ALLOWED_VLAN,
   CMD_END
};

struct t_parsed_cmd {
   enum t_cmd_enum cmd;
   unsigned parsed_args;
   const char *argv[10];
   char errstr[80];
};

struct t_cmdtree_elm {
   const char *token;
   enum t_cmd_enum cmd;
   unsigned arg;
   const struct t_cmdtree_elm *childs;
};

const struct t_cmdtree_elm cmdtree_config[];
const struct t_cmdtree_elm cmdtree_config0[];
const struct t_cmdtree_elm no_args[];

const struct t_cmdtree_elm mac_address_table_args[];
const struct t_cmdtree_elm no_mac_address_table_args[];
const struct t_cmdtree_elm mac_address_table_aging_time_args[];

const struct t_cmdtree_elm ip_args[];
const struct t_cmdtree_elm ip_igmp_args[];

const struct t_cmdtree_elm rrcp_args[];
const struct t_cmdtree_elm no_rrcp_args[];

const struct t_cmdtree_elm vlan_args[];
const struct t_cmdtree_elm no_vlan_args[];
const struct t_cmdtree_elm leaky_vlan_args[];
const struct t_cmdtree_elm vlan_drop_args[];

const struct t_cmdtree_elm mls_args[];
const struct t_cmdtree_elm mls_trust_args[];
const struct t_cmdtree_elm cosdscp_args[];

const struct t_cmdtree_elm wrr_queue_args[];
const struct t_cmdtree_elm no_wrr_queue_args[];
const struct t_cmdtree_elm wrr_queue_ratio_args[];

const struct t_cmdtree_elm flowcontrol_args[];

const struct t_cmdtree_elm stormcontrol_args[];
const struct t_cmdtree_elm no_stormcontrol_args[];
const struct t_cmdtree_elm stormcontrol_broadcast_args[];
const struct t_cmdtree_elm stormcontrol_multicast_args[];

const struct t_cmdtree_elm spanningtree_args[];

const struct t_cmdtree_elm interface_args[];
const struct t_cmdtree_elm interface_n_args[];
const struct t_cmdtree_elm interface_n_no_args[];

const struct t_cmdtree_elm interface_n_rate_limit_args[];
const struct t_cmdtree_elm interface_n_rate_limit_inout_args[];
const struct t_cmdtree_elm interface_n_no_rate_limit_args[];

const struct t_cmdtree_elm interface_n_mls_args[];
const struct t_cmdtree_elm interface_n_mls_qos_args[];
const struct t_cmdtree_elm interface_n_mls_qos_cos_args[];

const struct t_cmdtree_elm interface_n_speed_args[];
const struct t_cmdtree_elm interface_n_duplex_args[];

const struct t_cmdtree_elm enable_args[];
const struct t_cmdtree_elm disable_args[];

const struct t_cmdtree_elm interface_n_switchport_args[];
const struct t_cmdtree_elm interface_n_swicthport_mode_args[];
const struct t_cmdtree_elm interface_n_switchport_access_args[];
const struct t_cmdtree_elm interface_n_switchport_trunk_args[];

#define T_CMDTREE_ELM_NULL {NULL,CMD_NULL,0,NULL}
#define T_CMDTREE_ELM_VARARG(_childs) {"", CMD_NULL, 0, (_childs)}
#define T_CMDTREE_ELM_EOS {" ", CMD_NULL, 0, NULL}

#define IS_CMDTREE_ELM_NULL(_cmd) ((_cmd)->token==NULL)
#define IS_CMDTREE_ELM_VARARG(_cmd) (((_cmd)->token != NULL) && ((_cmd)->token[0]=='\0'))
#define IS_CMDTREE_ELM_EOS(_cmd) (((_cmd)->token != NULL) && ((_cmd)->token[0]==' '))

const struct t_cmdtree_elm cmdtree_config[] = {
   {'\0',CMD_NULL,0,cmdtree_config0},
   T_CMDTREE_ELM_NULL
};

const struct t_cmdtree_elm cmdtree_config0[] =
{
   {"version",       CMD_SET_VERSION,         0, NULL},
   {"hostname",      CMD_SET_HOSTNAME,        0, NULL},
   {"vender",	     CMD_SET_VENDER_ID,       0, NULL},
   {"chip",	     CMD_SET_CHIP_ID,         0, NULL},
   {"mac-address",   CMD_SET_MAC_ADDRESS,     0, NULL},
   {"mac-address-table", CMD_SET_MAC_ADDRESS_TABLE_AGING_TIME, 0, mac_address_table_args},
   {"ip",            CMD_SET_IP_IGMP_SNOOPING,0, ip_args},
   {"rrcp",          CMD_SET_RRCP,            0, rrcp_args},
   {"vlan",          CMD_SET_GLOBAL_VLAN,     0, vlan_args},
   {"mls",           CMD_SET_IP_MLS_QOS,      0, mls_args},
   {"wrr-queue",     CMD_SET_WRR_QUEUE_RATIO, 0, wrr_queue_args},
   {"flowcontrol",   CMD_SET_FLOW_CONTROL,    0, flowcontrol_args},
   {"storm-control",  CMD_NULL,                0, stormcontrol_args},
   {"spanning-tree", CMD_NULL,                0, spanningtree_args},
   {"interface",     CMD_NULL,                0, interface_args},
   {"no",            CMD_NULL,                0x80000000, no_args},
   {"end",           CMD_END,                 0, NULL},
   T_CMDTREE_ELM_NULL
};

/* no ,,, */
const struct t_cmdtree_elm no_args[]={
   {"mac-address-table", CMD_SET_MAC_ADDRESS_TABLE_AGING_TIME, 0, no_mac_address_table_args},
   {"ip",            CMD_SET_IP_IGMP_SNOOPING,0, ip_args},
   {"rrcp",          CMD_SET_RRCP,            0, no_rrcp_args},
   {"vlan",          CMD_SET_GLOBAL_VLAN,     0, no_vlan_args},
   {"mls",           CMD_SET_IP_MLS_QOS,      0, mls_args},
   {"wrr-queue",     CMD_SET_WRR_QUEUE_RATIO, 0, no_wrr_queue_args},
   {"flowcontrol",   CMD_SET_FLOW_CONTROL,    0, flowcontrol_args},
   {"storm-control",  CMD_NULL,                0, no_stormcontrol_args},
   {"spanning-tree", CMD_NULL,                0, spanningtree_args},
   T_CMDTREE_ELM_NULL
};

/* mac-address-table ... */
const struct t_cmdtree_elm mac_address_table_args[]={
   {"aging-time",CMD_SET_MAC_ADDRESS_TABLE_AGING_TIME,0,mac_address_table_aging_time_args},
   T_CMDTREE_ELM_NULL
};

/* mac-address-table aging-time ... */
const struct t_cmdtree_elm mac_address_table_aging_time_args[]={
    {"0",CMD_NULL,0x01,NULL},
    {"12",CMD_NULL,0x02,NULL},
    {"300",CMD_NULL,0x04,NULL},
    T_CMDTREE_ELM_NULL
};

/* no mac-address-table ... */
const struct t_cmdtree_elm no_mac_address_table_args[]={
   {"aging-time",CMD_SET_MAC_ADDRESS_TABLE_AGING_TIME,0,NULL},
    T_CMDTREE_ELM_NULL
};

/* ip ... */
const struct t_cmdtree_elm ip_args[]={
   {"igmp",CMD_NULL,0,ip_igmp_args},
   T_CMDTREE_ELM_NULL
};

/* ip igmp ... */
const struct t_cmdtree_elm ip_igmp_args[]={
   {"snooping",CMD_NULL,0,NULL},
   T_CMDTREE_ELM_NULL
};

/* rrcp ... */
const struct t_cmdtree_elm rrcp_args[]={
   {"enable",CMD_NULL,0x01,NULL},
   {"echo",CMD_NULL,0x02,enable_args},
   {"loop-detect",CMD_NULL, 0x04,enable_args},
   {"authkey",CMD_SET_AUTHKEY,0,NULL},
   T_CMDTREE_ELM_NULL
};

/* no rrcp ... */
const struct t_cmdtree_elm no_rrcp_args[]={
   {"enable",CMD_NULL,0x01,NULL},
   {"echo",CMD_NULL,0x02,enable_args},
   {"loop-detect",CMD_NULL,0x04,enable_args},
   T_CMDTREE_ELM_NULL
};

/* vlan ... */
const struct t_cmdtree_elm vlan_args[]={
   {"portbased",CMD_NULL,0x01,NULL},
   {"dot1q",CMD_NULL,0x02,NULL},
   {"leaky",CMD_SET_LEAKY_VLAN,0,leaky_vlan_args},
   {"drop",CMD_NULL,0,vlan_drop_args},
   T_CMDTREE_ELM_NULL
};

/* no vlan ... */
const struct t_cmdtree_elm no_vlan_args[]={
   T_CMDTREE_ELM_EOS,
   {"leaky",           CMD_SET_LEAKY_VLAN, 0,    leaky_vlan_args},
   {"drop",CMD_NULL,0,vlan_drop_args},
   T_CMDTREE_ELM_NULL
};

/* vlan drop ... */
const struct t_cmdtree_elm vlan_drop_args[]=
{
   {"untagged_frames",CMD_NULL,0x04,NULL},
   {"invalid_vid",CMD_NULL,0x08,NULL},
   T_CMDTREE_ELM_NULL
};

/* vlan leaky ...*/
const struct t_cmdtree_elm leaky_vlan_args[]={
   {"arp",CMD_NULL,0x10,NULL},
   {"unicast",CMD_NULL,0x20,NULL},
   {"multicast",CMD_NULL,0x40,NULL},
   T_CMDTREE_ELM_NULL
};

/* mls .. */
const struct t_cmdtree_elm mls_args[]={
   {"qos", CMD_NULL, 0, mls_trust_args},
   T_CMDTREE_ELM_NULL
};

const struct t_cmdtree_elm mls_trust_args[]={
   {"trust", CMD_NULL, 0, cosdscp_args},
   T_CMDTREE_ELM_NULL
};

/* mls qos ... */
const struct t_cmdtree_elm cosdscp_args[]={
   {"cos",  CMD_NULL, 0x01, NULL},
   {"dscp", CMD_NULL, 0x02, NULL},
   T_CMDTREE_ELM_NULL
};

/* wrr-queue ... */
const struct t_cmdtree_elm wrr_queue_args[]={
   {"ratio", CMD_NULL, 0, wrr_queue_ratio_args},
   T_CMDTREE_ELM_NULL
};

/* no wrr-queue ... */
const struct t_cmdtree_elm no_wrr_queue_args[]={
   {"ratio", CMD_NULL, 0, NULL},
   T_CMDTREE_ELM_NULL
};

/* wrr-queue ratio ... */
const struct t_cmdtree_elm wrr_queue_ratio_args[]={
   {"4:1", CMD_NULL, 0x04, NULL},
   {"8:1", CMD_NULL, 0x08, NULL},
   {"16:1",CMD_NULL, 0x10, NULL},
   {"1:0", CMD_NULL, 0x01, NULL},
   T_CMDTREE_ELM_NULL
};

/* flowcontrol ... */
const struct t_cmdtree_elm flowcontrol_args[]={
   {"dot3x",            CMD_NULL, 0x01, NULL},
   {"backpressure",     CMD_NULL, 0x02, NULL},
   {"ondemand-disable", CMD_NULL, 0x04, NULL},
   T_CMDTREE_ELM_NULL
};

/* storm-control ... */
const struct t_cmdtree_elm stormcontrol_args[]={
   {"broadcast", CMD_SET_STOM_CONTROL_BROADCAST,  0, stormcontrol_broadcast_args},
   {"multicast", CMD_SET_STORM_CONTROL_MULTICAST, 0, NULL},
   T_CMDTREE_ELM_NULL
};

/* no storm-control ... */
const struct t_cmdtree_elm no_stormcontrol_args[]={
   {"broadcast", CMD_SET_STOM_CONTROL_BROADCAST,  0, NULL},
   {"multicast", CMD_SET_STORM_CONTROL_MULTICAST, 0, NULL},
   T_CMDTREE_ELM_NULL
};


/* storm-control broadcast ... */
const struct t_cmdtree_elm stormcontrol_broadcast_args[]={
   {"enable",  CMD_NULL, 0x04, NULL},
   {"relaxed", CMD_NULL, 0x08, NULL},
   {"strict",  CMD_NULL, 0x10, NULL},
   T_CMDTREE_ELM_NULL
};

/* storm-control multicast ... */
const struct t_cmdtree_elm stormcontrol_multicast_args[]={
   {"enable",CMD_NULL, 0x20, NULL},
   {"strict",CMD_NULL, 0x40, NULL},
   T_CMDTREE_ELM_NULL
};

/* spanning-tree ... */
const struct t_cmdtree_elm spanningtree_args[]={
   {"bpdufilter", CMD_SET_SPANNING_TREE_BPDU_FILTER, 0,enable_args},
   T_CMDTREE_ELM_NULL
};

/* interface ... */
const struct t_cmdtree_elm interface_args[] =
{
   /* interface number */
   T_CMDTREE_ELM_VARARG(interface_n_args),
   T_CMDTREE_ELM_NULL
};

/* interface <n> ... */
const struct t_cmdtree_elm interface_n_args[] =
{
   {"shutdown",   CMD_SET_INTERFACE_SHUTDOWN,    0, NULL},
   {"rate-limit", CMD_SET_INTERFACE_RATE_LIMIT,  0, interface_n_rate_limit_args},
   {"rrcp",	  CMD_SET_INTERFACE_RRCP,	 0, enable_args},
   {"mac-learn",  CMD_SET_INTERFACE_MAC_LEARNING,0, disable_args},
   {"mls",	  CMD_SET_INTERFACE_PRIORITY,    0, interface_n_mls_args},
   {"speed",	  CMD_SET_INTERFACE_SPEED,       0, interface_n_speed_args},
   {"duplex",	  CMD_SET_INTERFACE_DUPLEX,      0, interface_n_duplex_args},
   {"switchport", CMD_NULL,                      0, interface_n_switchport_args},
   {"no",         CMD_NULL,             0x80000000, interface_n_no_args},
   {"end",        CMD_END,                      0, NULL},
   T_CMDTREE_ELM_NULL
};

/* interface <n> rate-limit ... */
const struct t_cmdtree_elm interface_n_rate_limit_args[] =
{
   {"input",   CMD_NULL,   0x1000, interface_n_rate_limit_inout_args},
   {"output",  CMD_NULL,   0x2000, interface_n_rate_limit_inout_args},
   T_CMDTREE_ELM_NULL
};

/* interface <n> rate-limit input|output ... */
const struct t_cmdtree_elm interface_n_rate_limit_inout_args[] =
{
   {"auto",   CMD_NULL,   0x01, NULL},
   {"100M",   CMD_NULL,   0x02, NULL},
   {"128K",   CMD_NULL,   0x04, NULL},
   {"256K",   CMD_NULL,   0x08, NULL},
   {"512K",   CMD_NULL,   0x10, NULL},
   {"1M",     CMD_NULL,   0x20, NULL},
   {"2M",     CMD_NULL,   0x40, NULL},
   {"4M",     CMD_NULL,   0x80, NULL},
   {"8M",     CMD_NULL,   0x100, NULL},
   {"1000M",  CMD_NULL,   0x0200, NULL},
   T_CMDTREE_ELM_NULL
};

/* interface <n> mls ... */
const struct t_cmdtree_elm interface_n_mls_args[] =
{
   {"qos", CMD_NULL, 0, interface_n_mls_qos_args},
   T_CMDTREE_ELM_NULL
};

/* interface <n> mls qos ... */
const struct t_cmdtree_elm interface_n_mls_qos_args[] =
{
   {"cos", CMD_NULL, 0, interface_n_mls_qos_cos_args},
   T_CMDTREE_ELM_NULL
};

/* interface <n> mls qos cos ... */
const struct t_cmdtree_elm interface_n_mls_qos_cos_args[] =
{
   {"0", CMD_NULL, 1, NULL},
   {"7", CMD_NULL, 7, NULL},
   T_CMDTREE_ELM_NULL
};

/* interface <n> speed ... */
const struct t_cmdtree_elm interface_n_speed_args[] =
{
   {"10",   CMD_NULL, 0x01, NULL},
   {"100",  CMD_NULL, 0x02, NULL},
   {"1000", CMD_NULL, 0x04, NULL},
   {"auto", CMD_NULL, 0x08, NULL},
   T_CMDTREE_ELM_NULL
};

/* interface <n> duplex ... */
const struct t_cmdtree_elm interface_n_duplex_args[] =
{
   {"half",   CMD_NULL, 0x01, NULL},
   {"full",   CMD_NULL, 0x02, NULL},
   {"auto",   CMD_NULL, 0x04, NULL},
   T_CMDTREE_ELM_NULL
};

/* interface <n> no ... */
const struct t_cmdtree_elm interface_n_no_args[] =
{
   {"shutdown",   CMD_SET_INTERFACE_SHUTDOWN,     0, NULL},
   {"rate-limit", CMD_SET_INTERFACE_RATE_LIMIT,   0, interface_n_no_rate_limit_args},
   {"rrcp",	  CMD_SET_INTERFACE_RRCP,	  0, enable_args},
   {"mac-learn",  CMD_SET_INTERFACE_MAC_LEARNING, 0, disable_args},
   {"mls",	  CMD_SET_INTERFACE_PRIORITY,     0, interface_n_mls_args},
   T_CMDTREE_ELM_NULL
};

/* interface <n> no rate limit ... */
const struct t_cmdtree_elm interface_n_no_rate_limit_args[] =
{
   {"input",   CMD_NULL,   0x1000, NULL},
   {"output",  CMD_NULL,   0x2000, NULL},
   T_CMDTREE_ELM_NULL
};

/* switchport ... */
const struct t_cmdtree_elm interface_n_switchport_args[]=
{
   {"mode",   CMD_SET_INTERFACE_SWITCHPORT_MODE, 0, interface_n_swicthport_mode_args},
   {"access", CMD_SET_INTERFACE_ACCESS_VLAN,     0, interface_n_switchport_access_args},
   {"trunk",  CMD_NULL, 0, interface_n_switchport_trunk_args},
   T_CMDTREE_ELM_NULL
};

/* switchport mode ... */
const struct t_cmdtree_elm interface_n_swicthport_mode_args[]=
{
   {"access",  CMD_NULL, 0x01, NULL},
   {"trunk",   CMD_NULL, 0x02, NULL},
   T_CMDTREE_ELM_NULL
};

/* switchport access ... */
const struct t_cmdtree_elm interface_n_switchport_access_args[]=
{
   {"vlan",   CMD_NULL, 0, NULL},
   T_CMDTREE_ELM_NULL
};

/* switchport trunk ... */
const struct t_cmdtree_elm interface_n_switchport_trunk_args[]=
{
   {"native",   CMD_SET_INTERFACE_TRUNK_NATIVE_VLAN, 0, interface_n_switchport_access_args},
   {"allowed",  CMD_SET_INTERFACE_TRUNK_ALLOWED_VLAN, 0, interface_n_switchport_access_args},
   T_CMDTREE_ELM_NULL
};


/* ... enable */
const struct t_cmdtree_elm enable_args[]={
   {"enable",CMD_NULL,0,NULL},
   T_CMDTREE_ELM_NULL
};

/* ... disable */
const struct t_cmdtree_elm disable_args[]={
   {"disable",CMD_NULL,0,NULL},
   T_CMDTREE_ELM_NULL
};


int push_arg(const char **s, size_t size, const char *arg, unsigned *s_p) {
   if (s_p[0]+1>= size)
      return -1;

   s[*s_p] = strdup(arg);
   if (s[*s_p]==NULL)
      return -1;
   s_p[0]++;
   s[*s_p]=NULL;

   return 0;
}

void free_arglist(const char **s) {
   const char **p;

   for(p=s;*p!=NULL;p++)
      free(*(void **)p);
   s[0]=NULL;
}

static int parse_iface(const char *name, int switchtype)
{
   unsigned ifacenum;
   if (sscanf(name, "FastEthernet0/%u", &ifacenum)==1) {
      if ((ifacenum >= 1)
	    && (ifacenum <= 24)
	    && (ifacenum <= switchtypes[switchtype].num_ports))
	 return switchtypes[switchtype].port_order[ifacenum-1]-1;
   }else if (sscanf(name, "GigabitEthernet0/%u", &ifacenum)==1) {
      if ((ifacenum > 23)
	    && (ifacenum <= switchtypes[switchtype].num_ports))
	 return switchtypes[switchtype].port_order[ifacenum-1]-1;
   }

   return -1;
}

static int check_interface_name(struct t_parsed_cmd *cmd, int switchtype)
{
   int iface;
   if ((cmd->argv[0] == NULL)) {
      snprintf(cmd->errstr, sizeof(cmd->errstr),
	    "Interface not defined");
      return -1;
   }
   iface = parse_iface(cmd->argv[0], switchtype);
   if (iface < 0) {
      snprintf(cmd->errstr, sizeof(cmd->errstr),
	    "Wrong interface name: %s", cmd->argv[1]);
      return -1;
   }
   return 0;
}

static int check_vlan_list(const char *str,
      int max_cnt,
      char *errstr, size_t errstr_size)
{
   struct t_str_number_list l;
   int cnt;
   int val;
   int err;

   cnt = str_number_list_init(str, &l);
   if (cnt <=0 || cnt > max_cnt) {
      snprintf(errstr, errstr_size, "Wrong vlan: %s", str);
      return -1;
   }

   while ((err=str_number_list_get_next(&l, &val))==0) {
      if (val <= 0 || val >= 0xfff) {
	 snprintf(errstr, errstr_size, "Wron vlan number: %i", val);
	 return -2;
      }
   }

   return 0;
}

int parsed_cmd_check_args(struct t_parsed_cmd *cmd, int switchtype)
{
   if (cmd->cmd == CMD_NULL) {
      snprintf(cmd->errstr, sizeof(cmd->errstr), "Unknown command");
      return -1;
   }

   switch (cmd->cmd) {
      case CMD_SET_MAC_ADDRESS_TABLE_AGING_TIME:
      case CMD_SET_IP_IGMP_SNOOPING:
      case CMD_SET_RRCP:
      case CMD_SET_LEAKY_VLAN:
      case CMD_SET_GLOBAL_VLAN:
      case CMD_SET_IP_MLS_QOS:
      case CMD_SET_WRR_QUEUE_RATIO:
      case CMD_SET_FLOW_CONTROL:
      case CMD_SET_STOM_CONTROL_BROADCAST:
      case CMD_SET_STORM_CONTROL_MULTICAST:
      case CMD_SET_SPANNING_TREE_BPDU_FILTER:
	 /* no additional arguments required */
	 if (cmd->argv[0] != NULL) {
	    snprintf(cmd->errstr, sizeof(cmd->errstr),
		  "Wrong argument: %s", cmd->argv[0]);
	    return -2;
	 }
	 break;
      case CMD_SET_VERSION:
	 if ((cmd->argv[0] == NULL)
	       || cmd->argv[1] != NULL) {
	    snprintf(cmd->errstr, sizeof(cmd->errstr),"Wrong arguments count");
	    return -1;
	 }
	 break;
      case CMD_SET_AUTHKEY:
      case CMD_SET_VENDER_ID:
      case CMD_SET_CHIP_ID:
	 {
	    char *endptr;

	    if ((cmd->argv[0] == NULL)
		  || cmd->argv[1] != NULL) {
	       snprintf(cmd->errstr, sizeof(cmd->errstr),"Wrong arguments count");
	       return -1;
	    }

	    strtoul(cmd->argv[0],&endptr, 16);
	    if ((cmd->argv[0][0]=='\0')
		  || (endptr[0]!='\0')) {
	       snprintf(cmd->errstr, sizeof(cmd->errstr),
		     "Mailformed %s: %s",
		     cmd->cmd == CMD_SET_AUTHKEY ? "authkey"
		       : (cmd->cmd == CMD_SET_VENDER_ID ? "vender_id" : "chip_id"),
		     cmd->argv[0]);
	       return -1;
	    }
	 }
	 break;
      case CMD_SET_MAC_ADDRESS:
	 {
	    char mac[6];
	    if ((cmd->argv[0] == NULL)
		  || cmd->argv[1] != NULL) {
	       snprintf(cmd->errstr, sizeof(cmd->errstr),"Wrong arguments count");
	       return -1;
	    }

	    if (sscanf(cmd->argv[0], "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
		     &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
	       snprintf(cmd->errstr, sizeof(cmd->errstr),
		     "Mailformed mac-address: %s\n", cmd->argv[0]);
	       return -1;
	    }
	 }
	 break;
      case CMD_SET_INTERFACE_SHUTDOWN:
      case CMD_SET_INTERFACE_RATE_LIMIT:
      case CMD_SET_INTERFACE_MAC_LEARNING:
      case CMD_SET_INTERFACE_RRCP:
      case CMD_SET_INTERFACE_PRIORITY:
      case CMD_SET_INTERFACE_SPEED:
      case CMD_SET_INTERFACE_DUPLEX:
      case CMD_SET_INTERFACE_SWITCHPORT_MODE:
	 if (check_interface_name(cmd, switchtype))
	    return -1;
	 if (cmd->argv[1] != NULL) {
	    snprintf(cmd->errstr, sizeof(cmd->errstr),
		  "Wrong argument: %s", cmd->argv[1]);
	    return -1;
	 }
	 break;
     case CMD_SET_INTERFACE_ACCESS_VLAN:
     case CMD_SET_INTERFACE_TRUNK_NATIVE_VLAN:
     case CMD_SET_INTERFACE_TRUNK_ALLOWED_VLAN:
	 if (check_interface_name(cmd, switchtype))
	    return -1;
	 if (cmd->argv[1] == NULL) {
	    snprintf(cmd->errstr, sizeof(cmd->errstr),
		  "vlan number not defined");
	    return -1;
	 }
	 if (cmd->argv[2] != NULL) {
	    snprintf(cmd->errstr, sizeof(cmd->errstr),
		  "Wrong argument: %s", cmd->argv[2]);
	    return -1;
	 }
	 if (check_vlan_list(cmd->argv[1],
		  cmd->cmd==CMD_SET_INTERFACE_TRUNK_ALLOWED_VLAN ? 32 : 1,
		  cmd->errstr, sizeof(cmd->errstr)))
	    return -1;
	 break;
     case CMD_END:
	 break;
      default:
	 return -2;
	 break;
   }

   return 0;
}

static uint8_t update_interface_speed_reg(uint8_t orig, unsigned new_args)
{
   uint8_t res;

   res = orig;
   switch (new_args) {
      case 0x01: /* 10M */
	 /* disable autoneg, 1000F, 100F, 100H */
	 res &= 0x73;
	 if ( ((res & 0x03) == 0)
	       || ((res & 0x03) == 3))
	    res = (res & 0xfc) | 0x02; /* 10F */
	 break;
      case 0x02: /* 100M */
	 /* disable autoneg, 1000F, 10F, 10H */
	 res &= 0x7c;
	 if ( ((res & 0x0c) == 0)
	       || ((res & 0x0c) == 3))
	    res = (res & 0xf3) | 0x08; /* 100F */
	 break;
      case 0x04: /* 1000M */
	 res = (res & 0x70) | 0x10;
	 break;
      case 0x08: /* auto */
	 res  |= 0x8f;
	 break;
   }

   return res;
}

static uint8_t update_interface_duplex_reg(uint8_t orig, unsigned set_full) {
   uint8_t res;

   /* Duplex cannot be changed for 1000F interfaces */
   if (orig & 0x10)
      return orig;

   /* Disable auto negotiation */
   res = orig & 0x7f;

   /* set duplex for 100M and 100M|10M(autoneg) interface */
   if (res & 0x0c)
      res = (res & 0xf0) | (set_full ? 0x08 : 0x04);
   /* set duplex for 10M interface */
   else if (res & 0x03)
      res = (res & 0xf0) | (set_full ? 0x02 : 0x01);
   /* set 100M if interface speed not defined */
   else
      res = (res & 0xf0) | (set_full ? 0x08 : 0x04);

   return res;

}

static uint8_t get_rate_limit_speed(unsigned parsed_args) {
   switch (parsed_args & 0x0fff) {
      case 0x0004: /* 128K */
	 return 0x01;
      case 0x0008: /* 256K */
	 return 0x02;
      case 0x0010: /* 512K */
	 return 0x03;
      case 0x0020: /* 1M */
	 return 0x04;
      case 0x0040: /* 2M */
	 return 0x05;
      case 0x0080: /* 4M */
	 return 0x06;
      case 0x0100: /* 8M */
	 return 0x07;
      case 0x0001: /* auto */
      case 0x0002: /* 100M */
      case 0x0200: /* 1000M */
      default:
	 break;
   }
   return 0;
}

static int eeprom_init_new_vlan_idx(
      uint8_t *eeprom,
      unsigned vlan_idx,
      unsigned vlan_num)
{
   if (vlan_idx == 32)
      return -1;
   /* Port VLAN configuration free_vlan_idx_0, _1
    * VLAN port member mask */
   eeprom[0xba + 6*vlan_idx]
      = eeprom[0xbb + 6*vlan_idx]
      = eeprom[0xbc + 6*vlan_idx]
      = eeprom[0xbd + 6*vlan_idx]
      = 0;
   /* Port VLAN configuration free_vlan_idx_2  - VID*/
   eeprom[0xbe + 6 * vlan_idx] = vlan_num & 0xff;
   eeprom[0xbf + 6 * vlan_idx] = (vlan_num >> 8) & 0xff;

   return 0;
}

int eeprom_set_vlan(uint8_t *eeprom,
      size_t eeprom_size,
      unsigned iface,
      const char *num_s,
      unsigned is_trunk,
      unsigned set_native)
{
   struct t_str_number_list l;
   int set_vlan, cur_vlan;
   unsigned i;
   unsigned vlan_idx;
   unsigned add_entry;

   if (!eeprom
	 || eeprom_size <= 0x17b)
      return -1;

   str_number_list_init(num_s, &l);
   vlan_idx=32;

  if (is_trunk) {
      /* TRUNK */
      if (!set_native)
	 for (i=0; i< 32; i++)
	    eeprom[0xba + 6*i + iface/8] &= ~(1 << (iface % 8));

      while (str_number_list_get_next(&l, &set_vlan) == 0) {
	 vlan_idx = 32;
	 add_entry = 1;
	 for (i=0; i < 32; i++) {
	    cur_vlan=(eeprom[0xbe + 6*i] | (eeprom[0xbf + 6*i] << 8))&0xfff;
	    if (cur_vlan == set_vlan) {
	       add_entry=0;
	       vlan_idx=i;
	       break;
	    }else {
	       if ((add_entry == 1)
		     && (vlan_idx == 32)
		     && ( (cur_vlan == 0) || (cur_vlan == 0xfff)))
		  vlan_idx = i;
	    }
	 }
	 if (add_entry == 1)
	    if (eeprom_init_new_vlan_idx(eeprom, vlan_idx, set_vlan))
	       continue;
	 eeprom[0xba + 6*vlan_idx + iface/8] |= (1 << (iface % 8));
      }
      if (set_native)
	 eeprom[0x98 + iface] = vlan_idx;
      /* insert tag to all packets */
      eeprom[0xb2 + iface/4] = (eeprom[0xb2 + iface/4]
	 & (~(3<< 2*(iface%4)))) | (2 << 2*(iface%4));
   }else {
      /* ACCESS */
      add_entry = 1;
      str_number_list_get_next(&l, &set_vlan);
      for (i=0; i < 32; i++) {
	 cur_vlan = (eeprom[0xbe + 6*i] | (eeprom[0xbf + 6*i] << 8))&0xfff;
	 if (cur_vlan == set_vlan) {
	    eeprom[0xba + 6*i + iface/8] |= (1 << (iface % 8));
	    add_entry=0;
	    vlan_idx=i;
	 }else {
	    if ( (add_entry==1)
		  && (vlan_idx == 32)
		  && ( (cur_vlan == 0) || (cur_vlan == 0xfff)))
	       vlan_idx = i;
	    else
	       eeprom[0xba + 6*i + iface/8] &= ~(1 << (iface % 8));
	 }
      }
      if (add_entry == 1) {
	 if (eeprom_init_new_vlan_idx(eeprom, vlan_idx, set_vlan))
	    return -1;
	 eeprom[0xba + 6*vlan_idx + iface/8] |= (1 << (iface % 8));
      }
      /* Port VLAN ID assigment index */
      eeprom[0x98 + iface] = vlan_idx;
      /* remove tag from all packets */
      eeprom[0xb2 + iface/4] = eeprom[0xb2 + iface/4]
	 & (~(3<< 2*(iface%4)));

   }

   return 0;
}

int parsed_cmd_modify(struct t_parsed_cmd *cmd,
	struct t_swconfig *conf,
	uint8_t *eeprom,
	size_t eeprom_size,
	int switchtype)
{
   int res;
   int iface;

   res = parsed_cmd_check_args(cmd, switchtype);

   if (res)
      return res;

   if ((conf == NULL) && (eeprom==NULL));

   switch (cmd->cmd) {
      case CMD_SET_MAC_ADDRESS_TABLE_AGING_TIME:
	 switch (cmd->parsed_args) {
	    case 0x01: /* 0 */
	    case 0x80000000: /* no */
	       if (conf) {
		  conf->alt_config.s.config.mac_aging_disable=1;
		  conf->alt_config.s.config.mac_aging_fast=0;
	       }
	       if (eeprom && eeprom_size >= 0x22)
		  eeprom[0x22] = (eeprom[0x22] & 0xfc) | 0x01;
	       break;
	    case 0x02: /* 12 */
	    case 0x04: /* 300 */
	       if (conf) {
		  conf->alt_config.s.config.mac_aging_disable=0;
		  conf->alt_config.s.config.mac_aging_fast
		     = cmd->parsed_args==0x02 ?  1 : 0;
	       }
	       if (eeprom && eeprom_size > 0x22)
		  eeprom[0x22] = (eeprom[0x22] & 0xfc)
		     | (cmd->parsed_args==0x02 ? 0x02 : 0);
	       break;
	    default:
	       break;
	 }
	 break;
      case CMD_SET_IP_IGMP_SNOOPING:
	 if (conf)
	    conf->alt_igmp_snooping.config.en_igmp_snooping
	       = cmd->parsed_args==0x80000000 ? 0 : 1;
	 if (eeprom && eeprom_size > 0x26)
	    eeprom[0x26] = (eeprom[0x26] & 0xfe)
	       | (cmd->parsed_args==0x80000000 ? 0 : 0x1);
	 break;
      case CMD_SET_RRCP:
	 switch (cmd->parsed_args) {
	    case 0x01: /* enable */
	    case 0x80000001: /* no enable */
	       if (conf)
		  conf->rrcp_config.config.rrcp_disable=
		     cmd->parsed_args == 0x80000001 ? 1 : 0;
	       if (eeprom && eeprom_size > 0x0c)
		  eeprom[0x0c] = (eeprom[0x0c] & 0xfe)
		     | (cmd->parsed_args == 0x80000001 ? 1 : 0);
	       break;
	    case 0x02: /* echo enable */
	    case 0x80000002: /* no echo enable */
	       if (conf)
		  conf->rrcp_config.config.echo_disable=
		     cmd->parsed_args == 0x80000002 ? 1 : 0;
	       if (eeprom && eeprom_size > 0x0c)
		  eeprom[0x0c] = (eeprom[0x0c] & 0xfd)
		     | (cmd->parsed_args == 0x80000002 ? 0x02 : 0);
	       break;
	    case 0x04: /* loop detect enable */
	    case 0x80000004: /* no loop detect enable */
	       if (conf)
		  conf->rrcp_config.config.loop_enable=
		     (cmd->parsed_args == 0x80000004 ? 0 : 1);
	       break;
	    default:
	       break;
	 }
      case CMD_SET_LEAKY_VLAN:
	 switch (cmd->parsed_args) {
	    case 0x10: /* arp */
	    case 0x80000010: /* no arp */
	       if (conf)
		  conf->vlan.s.config.arp_leaky=
		     cmd->parsed_args == 0x80000010 ? 0 : 1;
	       if (eeprom && eeprom_size > 0x28)
		  eeprom[0x28] = (eeprom[0x28] & 0xfb)
		     | (cmd->parsed_args == 0x80000010 ? 0 : 0x04);
	       break;
	    case 0x20: /* unicast */
	    case 0x80000020: /* no unicast */
	       if (conf)
		  conf->vlan.s.config.unicast_leaky=
		     (cmd->parsed_args == 0x80000020 ? 0 : 1);
	       if (eeprom && eeprom_size > 0x28)
		  eeprom[0x28] = (eeprom[0x28] & 0xfd)
		     | (cmd->parsed_args == 0x80000020 ? 0 : 0x02);
	       break;
	    case 0x40: /* multicast */
	    case 0x80000040: /* no multicast */
	       if (conf)
		  conf->vlan.s.config.multicast_leaky=
		     (cmd->parsed_args == 0x80000040 ? 0 : 1);
	       if (eeprom && eeprom_size > 0x28)
		  eeprom[0x28] = (eeprom[0x28] & 0xf7)
		     | (cmd->parsed_args == 0x80000040 ? 0 : 0x08);
	       break;
	    default:
	       break;
	 }
	 break;
      case CMD_SET_GLOBAL_VLAN:
	 switch (cmd->parsed_args) {
	    case 0x01: /* portbased */
	       if (conf) {
		  conf->vlan.s.config.enable=1;
		  conf->vlan.s.config.dot1q=0;
	       }
	       if (eeprom && eeprom_size > 0x28)
		  eeprom[0x28] = (eeprom[0x28] & 0xee) | 0x01;
	       break;
	    case 0x02: /* dot1q */
	       if (conf) {
		  conf->vlan.s.config.enable=1;
		  conf->vlan.s.config.dot1q=1;
	       }
	       if (eeprom && eeprom_size > 0x28)
		  eeprom[0x28] = (eeprom[0x28] & 0xee) | 0x11;
	       break;
	    case 0x04: /* drop untagged frames */
	       if (conf)
		  conf->vlan.s.config.drop_untagged_frames=1;
	       if (eeprom && eeprom_size > 0x28)
		  eeprom[0x28] |= 0x20;
	       break;
	    case 0x08: /* drop invalid vid */
	       if (conf)
		  conf->vlan.s.config.ingress_filtering=1;
	       if (eeprom && eeprom_size > 0x28)
		  eeprom[0x28] |= 0x40;
	       break;
	    case 0x80000000: /* no vlan */
	       if (conf) {
		  conf->vlan.s.config.enable=0;
		  conf->vlan.s.config.dot1q=0;
	       }
	       if (eeprom && eeprom_size > 0x28)
		  eeprom[0x28] = eeprom[0x28] & 0xee;
	       break;
	    case 0x80000004: /* no drop untagged frames */
	       if (conf)
		  conf->vlan.s.config.drop_untagged_frames=0;
	       if (eeprom && eeprom_size > 0x28)
		  eeprom[0x28] = eeprom[0x28] & 0xdf;
	    case 0x80000008: /* no invalid vid */
	       if (conf)
	    	  conf->vlan.s.config.ingress_filtering=0;
	       if (eeprom && eeprom_size > 0x28)
		  eeprom[0x28] = eeprom[0x28] & 0xbf;
	       break;
	    default:
	       break;
	 }
	 break;
      case CMD_SET_IP_MLS_QOS:
	 switch (cmd->parsed_args) {
	    case 0x01: /* cos */
	       if (conf)
		  conf->qos_config.config.cos_enable=1;
	       if (eeprom && eeprom_size > 0x2e)
		  eeprom[0x2e] |= 0x02;
	       break;
	    case 0x02: /* dscp */
	       if (conf)
		  conf->qos_config.config.dscp_enable=1;
	       if (eeprom && eeprom_size > 0x2e)
		  eeprom[0x2e] |= 0x01;
	       break;
	    case 0x80000001: /* no cos */
	       if (conf)
		  conf->qos_config.config.cos_enable=0;
	       if (eeprom && eeprom_size > 0x2e)
		  eeprom[0x2e] &= 0xfd;
	       break;
	    case 0x80000002: /* no dscp */
	       if (conf)
		  conf->qos_config.config.dscp_enable=0;
	       if (eeprom && eeprom_size > 0x2e)
		  eeprom[0x2e] &= 0xfe;
	       break;
	 }
	 break;
      case CMD_SET_WRR_QUEUE_RATIO:
	 switch (cmd->parsed_args) {
	    case 0x04: /* 4:1 */
	       if (conf)
		  conf->qos_config.config.wrr_ratio=0;
	       if (eeprom && eeprom_size > 0x2e)
		  eeprom[0x2e] &= 0xe7;
	       break;
	    case 0x08: /* 8:1 */
	       if (conf)
		  conf->qos_config.config.wrr_ratio=1;
	       if (eeprom && eeprom_size > 0x2e)
		  eeprom[0x2e] = (eeprom[0x2e] & 0xe7) | 0x08;
	       break;
	    case 0x10: /* 16:1 */
	       if (conf)
		  conf->qos_config.config.wrr_ratio=2;
	       if (eeprom && eeprom_size > 0x2e)
		  eeprom[0x2e] = (eeprom[0x2e] & 0xe7) | 0x10;
	       break;
	    case 0x01: /* 1:0 */
	    case 0x80000000: /* no */
	       if (conf)
		  conf->qos_config.config.wrr_ratio=3;
	       if (eeprom && eeprom_size > 0x2e)
		  eeprom[0x2e] |= 0x18;
	       break;
	 }
	 break;
      case CMD_SET_FLOW_CONTROL:
	 switch (cmd->parsed_args) {
	    case 0x01: /* dot3x */
	       if (conf)
		  conf->port_config_global.config.flow_dot3x_disable=0;
	       if (eeprom && eeprom_size > 0x38)
		  eeprom[0x38] &= 0xfe;
	       break;
	    case 0x02: /* backpressure */
	       if (conf)
		  conf->port_config_global.config.flow_backpressure_disable=0;
	       if (eeprom && eeprom_size > 0x38)
		  eeprom[0x38] &= 0xfd;
	       break;
	    case 0x04: /* ondemand-disable */
	       if (conf)
		  conf->qos_config.config.flow_ondemand_disable=1;
	       if (eeprom && eeprom_size > 0x2e)
		  eeprom[0x2e] |= 0x04;
	       break;
	    case 0x80000001: /* no dot3x */
	       if (conf)
		  conf->port_config_global.config.flow_dot3x_disable=1;
	       if (eeprom && eeprom_size > 0x38)
		  eeprom[0x38] |= 0x01;
	       break;
	    case 0x80000002: /* no backpressure */
	       if (conf)
		  conf->port_config_global.config.flow_backpressure_disable=1;
	       if (eeprom && eeprom_size > 0x38)
		  eeprom[0x38] |= 0x02;
	       break;
	    case 0x80000004: /* no ondemand-disable */
	       if (conf)
		  conf->qos_config.config.flow_ondemand_disable=0;
	       if (eeprom && eeprom_size > 0x2e)
		  eeprom[0x2e] &= 0xfb;
	       break;
	 }
	 break;
      case CMD_SET_STOM_CONTROL_BROADCAST:
	 switch (cmd->parsed_args) {
	    case 0x04: /* enable */
	    case 0x10: /* strict */
	       if (conf) {
		  conf->port_config_global.config.storm_control_broadcast_disable=0;
		  conf->port_config_global.config.storm_control_broadcast_strict=1;
	       }
	       if (eeprom && eeprom_size >= 0x38)
		  eeprom[0x38] = (eeprom[0x38] & 0xeb) | 0x04;
	       break;
	    case 0x08: /* relaxed */
	       if (conf) {
		  conf->port_config_global.config.storm_control_broadcast_disable=0;
		  conf->port_config_global.config.storm_control_broadcast_strict=0;
	       }
	       if (eeprom && eeprom_size >= 0x38)
		  eeprom[0x38] = eeprom[0x38] & 0xeb;
	       break;
	    case 0x80000004: /* no enable */
	       if (conf) {
		  conf->port_config_global.config.storm_control_broadcast_disable=1;
		  conf->port_config_global.config.storm_control_broadcast_strict=0;
	       }
	       if (eeprom && eeprom_size >= 0x38)
		  eeprom[0x38] = (eeprom[0x38] & 0xeb) | 0x10;
	       break;
	 }
      case CMD_SET_STORM_CONTROL_MULTICAST:
	 switch (cmd->parsed_args) {
	    case 0:    /* enable */
	    case 0x20: /* enable */
	    case 0x40: /* strict */
	       if (conf)
		  conf->port_config_global.config.storm_control_multicast_strict=1;
	       if (eeprom && eeprom_size >= 0x38)
		  eeprom[0x38] |= 0x08;
	       break;
	    case 0x80000000: /* disable */
	    case 0x80000020: /* no enable */
	    case 0x80000040: /* no strict */
	       if (conf)
		  conf->port_config_global.config.storm_control_multicast_strict=0;
	       if (eeprom && eeprom_size >= 0x38)
		  eeprom[0x38] &= 0xf7;
	       break;
	 }
	 break;
      case CMD_SET_SPANNING_TREE_BPDU_FILTER:
	 if (conf)
	    conf->alt_config.s.config.stp_filter =
	       cmd->parsed_args & 0x80000000 ? 0 : 1;
	 if (eeprom && eeprom_size > 0x22)
	    eeprom[0x22] = (eeprom[0x22] & 0xfb)
	       | (cmd->parsed_args & 0x80000000 ? 0 : 0x04);
	 break;
      case CMD_SET_VERSION:
	 break;
      case CMD_SET_AUTHKEY:
	 {
	    unsigned authk;
	    authk = strtoul(cmd->argv[0],NULL, 16);
	    if (conf)
	       conf->authkey = authk & 0xffff;
	    if (eeprom && eeprom_size > 0x70) {
	       eeprom[0x70]=authk&0xff;
	       eeprom[0x71]=(authk>>8)&0xff;
	    }
	 }
	 break;
      case CMD_SET_VENDER_ID:
	 {
	    unsigned venderid;
	    venderid = strtoul(cmd->argv[0],NULL, 16);
	    if (eeprom && eeprom_size > 0x1d) {
	       eeprom[0x1a]=venderid&0xff;
	       eeprom[0x1b]=(venderid>>8)&0xff;
	       eeprom[0x1c]=(venderid>>16)&0xff;
	       eeprom[0x1d]=(venderid>>24)&0xff;
	    }
	 }
	 break;
      case CMD_SET_CHIP_ID:
	 {
	    unsigned chipid;
	    chipid = strtoul(cmd->argv[0],NULL, 16);
	    if (eeprom && eeprom_size > 0x19) {
	       eeprom[0x18]=chipid&0xff;
	       eeprom[0x19]=(chipid>>8)&0xff;
	    }
	 }
	 break;
      case CMD_SET_MAC_ADDRESS:
	 {
	    uint8_t mac[6];
	    sscanf(cmd->argv[0], "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
		     &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
	    if (eeprom && eeprom_size > 0x17) {
	       eeprom[0x12]=mac[0];
	       eeprom[0x13]=mac[1];
	       eeprom[0x14]=mac[2];
	       eeprom[0x15]=mac[3];
	       eeprom[0x16]=mac[4];
	       eeprom[0x17]=mac[5];
	    }
	 }
	 break;
      case CMD_SET_INTERFACE_SHUTDOWN:
	 iface = parse_iface(cmd->argv[0], switchtype);
	 if (cmd->parsed_args & 0x80000000) {
	    if (conf)
	       conf->port_disable.bitmap &= ~((uint32_t)1 << iface);
	    if (eeprom && eeprom_size > 0x6b)
	       eeprom[0x68+iface/8] &= ~((uint8_t)1 << (iface % 8));
	 }else {
	    if (conf)
	       conf->port_disable.bitmap |= (uint32_t)1 << iface;
	    if (eeprom && eeprom_size > 0x6b)
	       eeprom[0x68+iface/8] |= (uint8_t)1 << (iface % 8);
	 }
	 break;
      case CMD_SET_INTERFACE_RATE_LIMIT:
	 iface = parse_iface(cmd->argv[0], switchtype);
	 if (conf) {
	    uint8_t spd;
	    spd = get_rate_limit_speed(cmd->parsed_args);
	    if (cmd->parsed_args & 0x2000) /* out */
	       conf->bandwidth.rxtx[iface].tx=spd;
	    else /* in */
	       conf->bandwidth.rxtx[iface].rx=spd;
	 }
	 if (eeprom && eeprom_size > 0x80+iface) {
	    uint8_t spd;
	    spd = get_rate_limit_speed(cmd->parsed_args);
	    if (cmd->parsed_args & 0x2000) /* out */
	       eeprom[0x80+iface] = (eeprom[0x80+iface] & 0x0f) | (spd << 4);
	    else /* in */
	       eeprom[0x80+iface] = (eeprom[0x80+iface] & 0xf0) | (spd & 0x0f);
	 }
	 break;
      case CMD_SET_INTERFACE_MAC_LEARNING:
	 iface = parse_iface(cmd->argv[0], switchtype);
	 if (cmd->parsed_args & 0x80000000) {
	    if (conf)
	       conf->alt_mask.mask &= ~((uint32_t)1 << iface);
	 }else {
	    if (conf)
	       conf->alt_mask.mask |= (uint32_t)1 << iface;
	 }
	 break;
      case CMD_SET_INTERFACE_RRCP:
	 iface = parse_iface(cmd->argv[0], switchtype);
	 if (cmd->parsed_args & 0x80000000) {
	    if (conf)
	       conf->rrcp_byport_disable.bitmap |= ((uint32_t)1 << iface);
	    if (eeprom && eeprom_size > 0x11)
	       eeprom[0x0e + iface/8] |= ((uint8_t)1 << (iface % 8));
	 }else {
	    if (conf)
	       conf->rrcp_byport_disable.bitmap &= ~((uint32_t)1 << iface);
	    if (eeprom && eeprom_size > 0x11)
	       eeprom[0x0e + iface/8] &= ~((uint8_t)1 << (iface % 8));
	 }
	 break;
      case CMD_SET_INTERFACE_PRIORITY:
	 iface = parse_iface(cmd->argv[0], switchtype);
	 switch (cmd->parsed_args) {
	    case 0x01: /* 1 */
	    case 0x80000000: /* no */
	       if (conf)
		  conf->qos_port_priority.bitmap &= ~((uint32_t)1 << iface);
	       if (eeprom && eeprom_size > 0x33)
		  eeprom[0x30+iface/8] &= ~((uint8_t)1 << (iface % 8));
	       break;
	    case 7: /* 7 */
	    default:
	       if (conf)
		  conf->qos_port_priority.bitmap |= ((uint32_t)1 << iface);
	       if (eeprom && eeprom_size > 0x33)
		  eeprom[0x30+iface/8] |= ((uint8_t)1 << (iface % 8));
	       break;
	 }
	 break;
      case CMD_SET_INTERFACE_SPEED:
	    iface = parse_iface(cmd->argv[0], switchtype);
	    if (conf) {
	       uint8_t port_conf;
	       port_conf = ((uint8_t *)conf->port_config.config)[iface];
	       port_conf = update_interface_speed_reg(port_conf,
		     cmd->parsed_args);
	       ((uint8_t *)conf->port_config.raw)[iface] = port_conf;
	    }

	    if (eeprom && eeprom_size > 0x3a + iface) {
	       uint8_t port_conf;
	       port_conf = eeprom[0x3a+iface];
	       port_conf = update_interface_speed_reg(port_conf,
		     cmd->parsed_args);
	       eeprom[0x3a+iface] = port_conf;
	    }
	 break;
      case CMD_SET_INTERFACE_DUPLEX:
	    iface = parse_iface(cmd->argv[0], switchtype);
	    if (conf) {
	       uint8_t port_conf;
	       port_conf = ((uint8_t *)conf->port_config.config)[iface];
	       port_conf = update_interface_duplex_reg(port_conf,
		     cmd->parsed_args == 0x01 ? 0 : 1);
	       ((uint8_t *)conf->port_config.raw)[iface] = port_conf;
	    }
	    if (eeprom && eeprom_size > 0x3a + iface) {
	       uint8_t port_conf;
	       port_conf = eeprom[0x3a+iface];
	       port_conf = update_interface_duplex_reg(port_conf,
		     cmd->parsed_args == 0x01 ? 0 : 1);
	       eeprom[0x3a+iface] = port_conf;
	    }
	 break;
      case CMD_SET_INTERFACE_SWITCHPORT_MODE:
	    iface = parse_iface(cmd->argv[0], switchtype);
	    /* 0x01 - acces, 0x02 - trunk */
	    if (conf) {
	       conf->vlan_port_insert_vid.bitmap =
		  (conf->vlan_port_insert_vid.bitmap & ~((uint32_t)1 << iface))
		  | (cmd->parsed_args == 0x02 ? 1 << iface : 0);
	    }
	    if (eeprom && eeprom_size > 0x17d) {
	       eeprom[0x17a+iface/8] =
		  (eeprom[0x17a+iface/8] & ~((uint8_t)1 << (iface % 8)))
		  | (cmd->parsed_args == 0x02 ? 1 << (iface % 8) : 0);
	    }
	 break;
     case CMD_SET_INTERFACE_ACCESS_VLAN:
     case CMD_SET_INTERFACE_TRUNK_NATIVE_VLAN:
	 iface = parse_iface(cmd->argv[0], switchtype);
	 /* TODO: conf */
	 eeprom_set_vlan(eeprom,
		  eeprom_size,
		  iface,
		  cmd->argv[1],
		  cmd->cmd == CMD_SET_INTERFACE_ACCESS_VLAN ? 0 : 1,
		  1);
	 break;
     case CMD_SET_INTERFACE_TRUNK_ALLOWED_VLAN:
	 iface = parse_iface(cmd->argv[0], switchtype);
	 /* TODO: conf */
	 eeprom_set_vlan(eeprom,
		  eeprom_size,
		  iface,
		  cmd->argv[1], 1, 0);
	 break;
     case CMD_END:
	 break;
      default:
	 break;
   }

   return 0;

}

int next_tok(char *dst, size_t size, const char **str) {
   const char *p;
   unsigned idx;

   p = *str;
   while(*p && isspace(*p)) p++;

   if (*p == '\0')
      return -1;

   if (size==0)
      return -2;

   idx=0;
   while((idx<size-1)
	 && *p
	 && !isspace(*p)) {
      dst[idx]=*p++;
      idx++;
   }

   dst[idx]='\0';
   *str = p;

   return 0;
}

unsigned get_best_match_token_idx(
      const struct t_cmdtree_elm *list,
      const char *tok)
{
   unsigned i;
   i = 0;
   while (!IS_CMDTREE_ELM_NULL(&list[i])){
      if (IS_CMDTREE_ELM_VARARG(&list[i]))
	 break;
      if (IS_CMDTREE_ELM_EOS(&list[i])) {
	 i++;
	 continue;
      }
      if (strcasecmp(tok, list[i].token)==0)
	 break;
      i++;
   }

   return i;
}


int parse_cmd (const char *cmd, struct t_parsed_cmd *res, int switchtype)
{
   const char *p;
   unsigned tok_num;
   unsigned argv_p;
   unsigned best_match_idx;
   const struct t_cmdtree_elm *cur;
   char tok[120];

   res->cmd=CMD_NULL;
   res->parsed_args=0;
   res->argv[0]=NULL;
   res->errstr[0]='\0';

   cur = cmdtree_config;
   tok_num=0;
   argv_p=0;
   for (p=cmd; next_tok(tok, sizeof(tok), &p)==0;) {
      tok_num++;
      if (cur->childs == NULL) {
	 if (push_arg(res->argv, sizeof(res->argv)/sizeof(res->argv[0]), tok, &argv_p)) {
	    free_arglist(res->argv);
	    snprintf(res->errstr, sizeof(res->errstr),
		  "Too many arguments on: %s", tok);
	    return -1*tok_num;
	 }
      }else {
	 cur = cur->childs;

	 best_match_idx = get_best_match_token_idx(cur, tok);
	 cur = &cur[best_match_idx];
	 if (IS_CMDTREE_ELM_VARARG(cur)) {
	    if (push_arg(res->argv, sizeof(res->argv)/sizeof(res->argv[0]), tok, &argv_p)) {
	       free_arglist(res->argv);
	       snprintf(res->errstr, sizeof(res->errstr),
		     "Too many arguments on: %s", tok);
	       return -1*tok_num;
	    }
	 } else if (IS_CMDTREE_ELM_NULL(cur)) {
	    /* Token not found */
	    free_arglist(res->argv);
	    snprintf(res->errstr, sizeof(res->errstr),
		  "Wrong command or argument: %s", tok);
	    return -1*tok_num;
	 }

	 res->parsed_args |= cur->arg;
	 if (cur->cmd != CMD_NULL)
	    res->cmd=cur->cmd;
      }
   }

   if ((cur->childs != NULL)
	 && (!IS_CMDTREE_ELM_EOS(&(cur->childs[0]))))
   {
	free_arglist(res->argv);
	snprintf(res->errstr, sizeof(res->errstr), "Incomplete command");
	return -1;
   }

   if (parsed_cmd_check_args(res, switchtype) < 0) {
      free_arglist(res->argv);
      return -1;
   }

   return 0;
}

int cmd_check_syntax(const char *cmd,
      int switchtype,
      char *err, size_t err_size)
{
   struct t_parsed_cmd cmdp;
   int res;
   res = parse_cmd(cmd, &cmdp, switchtype);
   if (err) {
      if (res) {
	 strncpy(err, cmdp.errstr, err_size);
	 err[err_size-1]='\0';
      }else
	 err[0]='\0';
   }

   return res;
}


int cmd_change_config(const char *cmd,
      struct t_swconfig *conf,
      int switchtype,
      char *err, size_t err_size)
{
   struct t_parsed_cmd cmdp;
   int res;
   res = parse_cmd(cmd, &cmdp, switchtype);

   if (res) {
      if (err) {
	 strncpy(err, cmdp.errstr, err_size);
	 err[err_size-1]='\0';
      }
      return res;
   }

   res = parsed_cmd_modify(&cmdp, conf, NULL, 0, switchtype);
   free_arglist(cmdp.argv);

   if (err) {
      if (res) {
	 strncpy(err, cmdp.errstr, err_size);
	 err[err_size-1]='\0';
      } else
	 err[0]='\0';
   }

   return res;
}

int cmd_change_eeprom(const char *cmd,
      uint8_t *eeprom,
      size_t eeprom_size,
      int switchtype,
      char *err, size_t err_size)
{
   struct t_parsed_cmd cmdp;
   int res;
   res = parse_cmd(cmd, &cmdp, switchtype);

   if (res) {
      if (err) {
	 strncpy(err, cmdp.errstr, err_size);
	 err[err_size-1]='\0';
      }
      return res;
   }

   res = parsed_cmd_modify(&cmdp, NULL, eeprom, eeprom_size, switchtype);
   free_arglist(cmdp.argv);

   if (err) {
      if (res) {
	 strncpy(err, cmdp.errstr, err_size);
	 err[err_size-1]='\0';
      } else
	 err[0]='\0';
   }

   return res;
}

