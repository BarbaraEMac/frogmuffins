#
# Makefile for busy-wait IO tests
#
XCC     = gcc
AS	= as
LD      = ld
CFLAGS  = -c -fPIC -Wall -I. -I../include -mcpu=arm920t -msoft-float
# -g: include hooks for gdb
# -c: only compile
# -mcpu=arm920t: generate code for the 920t architecture
# -fpic: emit position-independent code
# -Wall: report all warnings

ASFLAGS	= -mcpu=arm920t -mapcs-32
# -mapcs: always generate a complete stack frame

LDFLAGS = -init main -Map main.map -N  -T orex.ld -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2 -L../lib

SOURCES = main.o switch.o requests.o 

all:  main.s requests.s switch.S main.elf 

%.s: %.c %.h
	$(XCC) -S $(CFLAGS) $<

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

main.elf: $(SOURCES)
	$(LD) $(LDFLAGS) -o $@ $(SOURCES) -lbwio -lgcc

switch.o: switch.S
	$(AS) $(ASFLAGS) -o switch.o switch.S
	
clean:
	-rm -f main.elf *.s *.o main.map

copy: 
	cp main.elf /u/cs452/tftpboot/ARM/dgoc
	chmod a+r /u/cs452/tftpboot/ARM/dgoc/*
