all: rtl8326 rtl8316b rtl83xx_dlink_des1016d rtl83xx_dlink_des1024d rtl83xx_compex_ps2216

rtl83xx: rtl83xx.c
	gcc -s -Wall -O2 -fomit-frame-pointer rtl83xx.c rrcp_io.c rrcp_switches.c rrcp_config.c -o ../bin/rtl83xx

rtl8326: rtl83xx
	ln -sf rtl83xx ../bin/rtl8326

rtl8316b: rtl83xx
	ln -sf rtl83xx ../bin/rtl8316b

rtl83xx_dlink_des1024d: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_dlink_des1024d

rtl83xx_dlink_des1016d: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_dlink_des1016d

rtl83xx_compex_ps2216: rtl83xx
	ln -sf rtl83xx ../bin/rtl83xx_compex_ps2216

mips_rtl83xx_dlink_des1024d: rtl83xx
	/opt/hardhat/previewkit/mips/mipsel-linux-uclibc/bin/mipsel-uclibc-gcc -I /opt/hardhat/previewkit/mips/mipsel-linux-uclibc/include -s -Wall -O2 -fomit-frame-pointer rtl83xx.c rrcp_io.c rrcp_switches.c rrcp_config.c -o ../bin/mips_rtl83xx_dlink_des1024d
