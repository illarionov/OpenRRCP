CFLAGS=`/bin/sh Makefile-helper.sh`

all: rtl8326 rtl8316b rtl83xx_dlink_des1016d rtl83xx_dlink_des1024d_b1 rtl83xx_dlink_des1024d_c1 rtl83xx_compex_ps2216 rtl83xx_ovislink_fsh2402gt rtl83xx_zyxel_es116p rtl83xx_compex_sds1224 rtl83xx_signamax_065-7531a rtl83xx_compex_ps2216_6d rtl83xx_compex_ps2216_6dp rrcpcli

clean:
	rm -f ../bin/*

rrcpcli: rrcp_cli.c rrcp_cli_cmd_show.c rrcp_cli_cmd_config.c rrcp_cli_cmd_config_int.c rrcp_cli_cmd_other.c rrcp_config.c rrcp_io.c rrcp_lib.c rrcp_switches.c
	gcc $(CFLAGS) rrcp_cli.c rrcp_cli_cmd_show.c rrcp_cli_cmd_config.c rrcp_cli_cmd_config_int.c rrcp_cli_cmd_other.c rrcp_config.c rrcp_io.c rrcp_lib.c rrcp_switches.c ../lib/libcli.c -lcrypt -o ../bin/rrcpcli

rrcpcli-debug: rrcp_cli.c rrcp_cli_cmd_show.c rrcp_cli_cmd_config.c rrcp_cli_cmd_config_int.c rrcp_cli_cmd_other.c rrcp_config.c rrcp_io.c rrcp_lib.c rrcp_switches.c
	gcc -Wall -ggdb rrcp_cli.c rrcp_cli_cmd_show.c rrcp_cli_cmd_config.c rrcp_cli_cmd_config_int.c rrcp_cli_cmd_other.c rrcp_config.c rrcp_io.c rrcp_lib.c rrcp_switches.c ../lib/libcli.c -lcrypt -o ../bin/rrcpcli-debug

rtl83xx: rtl83xx.c
	gcc $(CFLAGS) rtl83xx.c rrcp_io.c rrcp_switches.c rrcp_config.c -o ../bin/rtl83xx

rtl8326: rtl83xx
	ln -sf rtl83xx ../bin/rtl8326

rtl8316b: rtl83xx
	ln -sf rtl83xx ../bin/rtl8316b

rtl83xx_dlink_des1024d_b1: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_dlink_des1024d_b1

rtl83xx_dlink_des1024d_c1: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_dlink_des1024d_c1

rtl83xx_dlink_des1016d: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_dlink_des1016d

rtl83xx_compex_ps2216: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_compex_ps2216

rtl83xx_compex_ps2216_6d: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_compex_ps2216_6d

rtl83xx_compex_ps2216_6dp: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_compex_ps2216_6dp

rtl83xx_ovislink_fsh2402gt: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_ovislink_fsh2402gt

rtl83xx_zyxel_es116p: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_zyxel_es116p

rtl83xx_compex_sds1224: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_compex_sds1224

mips_rtl83xx_dlink_des1024d: rtl83xx
	/opt/hardhat/previewkit/mips/mipsel-linux-uclibc/bin/mipsel-uclibc-gcc -I /opt/hardhat/previewkit/mips/mipsel-linux-uclibc/include -s -Wall -O2 -fomit-frame-pointer rtl83xx.c rrcp_io.c rrcp_lib.c rrcp_switches.c rrcp_config.c -o ../bin/mips_rtl83xx_dlink_des1024d

rtl83xx_signamax_065-7531a: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_signamax_065-7531a

rrcp_decode:
	gcc $(CFLAGS) -o ../bin/rrcp_decode rrcp_switches.c rrcp_config.c rrcp_io.c rrcp_decode.c -lpcap
