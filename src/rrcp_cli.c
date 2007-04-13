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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <termios.h>

#ifndef __linux__
#include <netinet/in.h>
#include <net/if.h>
#endif

#include <arpa/inet.h>
#include "rrcp_packet.h"
#include "rrcp_io.h"
#include "rrcp_switches.h"
#include "rrcp_config.h"
#include "../lib/libcli.h"
#include "rrcp_cli_cmd_show.h"
#include "rrcp_cli_cmd_config.h"
#include "rrcp_cli_cmd_config_int.h"
#include "rrcp_cli.h"

int myPid = 0;

void sigHandler(int sig)
{
    if (myPid == 0) return;
    printf("Timeout! (no switch with rtl83xx chip found?)\n");
    sleep(1);
    kill(myPid, SIGKILL);
    exit(0);
}

void engage_timeout(int seconds)
{
    myPid = getpid();
    signal( SIGALRM, sigHandler );
    alarm( seconds );
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
    struct cli_def *cli;
    int s, x;
    struct sockaddr_in servaddr;
    int on = 1;
    int switchtype_force,tcp_port;

    cli = cli_init();
    {
	char *p;
	p=malloc(512);
	sprintf(p,"OpenRRCP CLI Version %s",RRCP_CLI_VERSION);
	cli_set_banner(cli, p);
	free(p);
    }
    cli_set_hostname(cli, "rrcpswitch");

    cmd_show_register_commands(cli);
    cmd_config_register_commands(cli);
    cmd_config_int_register_commands(cli);

//    cli_set_auth_callback(cli, check_auth);
//    cli_set_enable_callback(cli, check_enable);

/*
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
*/

    switchtype=-1;
    switchtype_force=0;
    tcp_port=0;
    while(1) {
	char c;
	c = getopt_long (argc, argv, "t:fp:",
                        NULL, NULL);
	if (c == -1)
    	    break;

        switch (c) {
    	    case 't':
		switchtype=atoi(optarg);
    		break;
    	    case 'f':
		switchtype_force=1;
    		break;
    	    case 'p':
		tcp_port=atoi(optarg);
    		break;
	}
    }

    if (optind < argc) {
        unsigned int x[6];
	int i;
	if (sscanf(argv[optind], "%x:%x:%x:%x:%x:%x@%s",x,x+1,x+2,x+3,x+4,x+5,ifname)==7){
	    for (i=0;i<6;i++){
		dest_mac[i]=(unsigned char)x[i];
	    }
	}else{
	    strcpy(ifname,argv[optind]);
	}
	optind++;
    }else{
	printf("\nUsage: rrcp_cli [options] <xx:xx:xx:xx:xx:xx@if-name>\n");
	printf(" -p <port>    go to background and listen on specified TCP port\n");
	printf(" -t <number>  specify switch type\n");
	printf(" -f           force switch_type (be careful!)\n");
	printf("Supported switch types are:\n");
	{
	    int i;
	    for (i=0;i<switchtype_n;i++){
    		printf(" %d - %s %s\n",i,switchtypes[i].vendor,switchtypes[i].model);
    	    }
    	}
	exit(0);
    }

    if (if_nametoindex(ifname)<=0){
	printf("%s: no such interface: %s\n",argv[0],ifname);
	exit(0);
    }


    engage_timeout(10);
    rtl83xx_prepare();

    if (switchtype==-1){
	switchtype=rrcp_switch_autodetect();
	printf("detected %s %s\n",
	    switchtypes[switchtype].vendor,
	    switchtypes[switchtype].model);
    }else{
	if(switchtypes[switchtype].chip_id!=rrcp_switch_autodetect_chip()){
	    printf("%s: ERROR - Chip mismatch\n"
		    "Specified switch: %s %s\n"
		    "Specified chip: %s\n"
		    "Detected chip: '%s'\n",
		    argv[0],
		    switchtypes[switchtype].vendor,
		    switchtypes[switchtype].model,
		    switchtypes[switchtype].chip_name,
		    "");
	    exit(0);
	}
    }
    {
	char s[64];
	sprintf(s,"%s_%s",
	    switchtypes[switchtype].vendor,
	    switchtypes[switchtype].model);
	cli_set_hostname(cli,s);
    }

    printf("Fetching current config from switch\n");
    rrcp_config_read_from_switch();
    myPid = 0; //clear timeout handler


    if (tcp_port==0){
	struct termios ttyarg, intrm ;           /* arguments for termios */
	int n;

	x=0;
	tcgetattr( x , &intrm ) ;                /* get stdin arguments (SAVE) */
	ttyarg = intrm ;                         /* saved original configuration */
	ttyarg.c_iflag = 0 ;                     /* clear iflag of all input */
	ttyarg.c_lflag = ISIG ;                  /* clear lflag of all processing, except for signals */
	n = tcsetattr( x , TCSANOW , &ttyarg ) ; /* set changed tty arguments */
	cli_set_privilege(cli, PRIVILEGE_PRIVILEGED);
	cli_loop(cli, x);
	system("reset"); //reset terminal on exit. FIXME: need to find more straight-forward solution to do this.
    }else{
	if (fork()==0){
	    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		return 1;
	    }
	    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	    memset(&servaddr, 0, sizeof(servaddr));
	    servaddr.sin_family = AF_INET;
	    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	    servaddr.sin_port = htons(tcp_port);
	    if (bind(s, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
		perror("bind");
		return 1;
	    }

	    if (listen(s, 50) < 0){
		perror("listen");
		return 1;
	    }

	    printf("Listening on port %d\n", tcp_port);
	    while ((x = accept(s, NULL, 0))){
		cli_loop(cli, x);
		close(x);
	    }
	}
    }
    cli_done(cli);
    return 0;
}

