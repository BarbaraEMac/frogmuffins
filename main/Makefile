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

all:  main.s main.elf 

main.s: main.c main.h
	$(XCC) -S $(CFLAGS) main.c

main.o: main.s
	$(AS) $(ASFLAGS) -o main.o main.s

main.elf: main.o switch.o requests.o
	$(LD) $(LDFLAGS) -o $@ main.o switch.o requests.o -lbwio -lgcc

switch.REDO: switch.c switch.h
	$(XCC) -S $(CFLAGS) -o switch.REDO switch.c

switch.o: switch.S
	$(AS) $(ASFLAGS) -o switch.o switch.S
	
requests.s: requests.c requests.h
	$(XCC) -S $(CFLAGS) requests.c

requests.o: requests.s
	$(AS) $(ASFLAGS) -o requests.o requests.s

clean:
	-rm -f main.elf *.s *.o main.map

copy: 
	cp main.elf /u/cs452/tftpboot/ARM/dgoc
	chmod a+r /u/cs452/tftpboot/ARM/dgoc/*
