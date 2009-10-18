EEPROM_CONFIG_ADDR = 0x1c00
EEPROM_CONFIG_SIZE = 0x400
EEPROM_FILE=2408.eeprom

CC = sdcc
CFLAGS = --model-small --opt-code-size
LFLAGS =  --code-loc 0x0000 --code-size 0x2000

all: fsd1600.bin

OBJECTS = fsd1600.rel io.rel jumpers.rel loopdetect.rel porttest.rel

#http://sf.net/projects/srecord 
fsd1600.bin: fsd1600.ihx $(EEPROM_FILE)
	srec_cat fsd1600.ihx -intel $(EEPROM_FILE) -binary -offset $(EEPROM_CONFIG_ADDR) -o bin/fsd1600.bin -binary

fsd1600.ihx: $(OBJECTS)
	$(CC) $(CLAGS) $(LFLAGS) $(OBJECTS)

$(OBJECTS): fsd1600.c io.c jumpers.c loopdetect.c porttest.c fsd1600.h
	$(CC) -c $(CFLAGS) $(MFLAGS) fsd1600.c
	$(CC) -c $(CFLAGS) $(MFLAGS) io.c
	$(CC) -c $(CFLAGS) $(MFLAGS) jumpers.c
	$(CC) -c $(CFLAGS) $(MFLAGS) loopdetect.c
	$(CC) -c $(CFLAGS) $(MFLAGS) porttest.c


clean:
	rm *.asm *.ihx *.lnk *.lst *.map *.mem *.rel *.rst *.sym bin/fsd1600.bin