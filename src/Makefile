CFLAGS=`/bin/sh Makefile-helper.sh`

all: rtl8316b rtl8316bp rtl8324 rtl8324p rtl8326 \
rtl83xx_dlink_des1016d rtl83xx_dlink_des1024d_b1 \
rtl83xx_dlink_des1024d_c1 rtl83xx_compex_ps2216 rtl83xx_ovislink_fsh2402gt \
rtl83xx_zyxel_es116p rtl83xx_compex_sds1224 rtl83xx_signamax_065-7531a \
rtl83xx_compex_ps2216_6d rtl83xx_compex_ps2216_6e rtl83xx_asus_gigax_1024p \
rtl83xx_repotec_g3224x rtl83xx_edimax_es-3116p rtl83xx_neomontana_cs8p \
rtlping rrcpcli

clean:
	rm -f ../bin/*

rrcpcli: rrcp_cli.c rrcp_cli_cmd_show.c rrcp_cli_cmd_config.c rrcp_cli_cmd_config_int.c rrcp_cli_cmd_other.c rrcp_config.c rrcp_packet.c rrcp_io.c rrcp_lib.c rrcp_switches.c
	$(CC) $(CFLAGS) rrcp_cli.c rrcp_cli_cmd_show.c rrcp_cli_cmd_config.c rrcp_cli_cmd_config_int.c rrcp_cli_cmd_other.c rrcp_config.c rrcp_packet.c rrcp_io.c rrcp_lib.c rrcp_switches.c ../lib/libcli.c -o ../bin/rrcpcli

rrcpcli-debug: rrcp_cli.c rrcp_cli_cmd_show.c rrcp_cli_cmd_config.c rrcp_cli_cmd_config_int.c rrcp_cli_cmd_other.c rrcp_config.c rrcp_io.c rrcp_lib.c rrcp_switches.c
	$(CC) -Wall -ggdb rrcp_cli.c rrcp_cli_cmd_show.c rrcp_cli_cmd_config.c rrcp_cli_cmd_config_int.c rrcp_cli_cmd_other.c rrcp_config.c rrcp_io.c rrcp_packet.c rrcp_lib.c rrcp_switches.c ../lib/libcli.c -o ../bin/rrcpcli-debug

rtl83xx: rtl83xx.c
	$(CC) $(CFLAGS) rtl83xx.c rrcp_packet.c rrcp_io.c rrcp_lib.c rrcp_switches.c rrcp_config.c -o ../bin/rtl83xx

rtl8316b: rtl83xx
	ln -sf rtl83xx ../bin/rtl8316b

rtl8316bp: rtl83xx
	ln -sf rtl83xx ../bin/rtl8316bp

rtl8324: rtl83xx
	ln -sf rtl83xx ../bin/rtl8324

rtl8324p: rtl83xx
	ln -sf rtl83xx ../bin/rtl8324p

rtl8326: rtl83xx
	ln -sf rtl83xx ../bin/rtl8326

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

rtl83xx_compex_ps2216_6e: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_compex_ps2216_6e

rtl83xx_ovislink_fsh2402gt: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_ovislink_fsh2402gt

rtl83xx_zyxel_es116p: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_zyxel_es116p

rtl83xx_compex_sds1224: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_compex_sds1224

rtl83xx_asus_gigax_1024p: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_asus_gigax_1024p

mips_rtl83xx_dlink_des1024d: rtl83xx
	/opt/hardhat/previewkit/mips/mipsel-linux-uclibc/bin/mipsel-uclibc-gcc -I /opt/hardhat/previewkit/mips/mipsel-linux-uclibc/include -s -Wall -O2 -fomit-frame-pointer rtl83xx.c rrcp_packet.c rrcp_io.c rrcp_lib.c rrcp_switches.c rrcp_config.c -o ../bin/mips_rtl83xx_dlink_des1024d

rtl83xx_signamax_065-7531a: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_signamax_065-7531a

rtl83xx_repotec_g3224x: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_repotec_g3224x

rtl83xx_edimax_es-3116p: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_edimax_es-3116p

rtl83xx_neomontana_cs8p: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_neomontana_cs8p

rrcp_decode:
	gcc $(CFLAGS) -o ../bin/rrcp_decode rrcp_switches.c rrcp_config.c rrcp_packet.c rrcp_io.c rrcp_decode.c -lpcap

check_rrcp:
	gcc $(CFLAGS) check_rrcp.c rrcp_packet.c rrcp_io.c rrcp_switches.c rrcp_config.c rrcp_lib.c -o ../bin/check_rrcp

rtlping:
	gcc $(CFLAGS) rtlping.c rrcp_packet.c rrcp_io.c rrcp_lib.c rrcp_switches.c rrcp_config.c -o ../bin/rtlping

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin 2>/dev/null
	cp -p ../bin/* $(DESTDIR)$(PREFIX)/bin
#	chmod 4555 $(DESTDIR)$(PREFIX)/rtl83xx
#	chown root $(DESTDIR)$(PREFIX)/rtl83xx
