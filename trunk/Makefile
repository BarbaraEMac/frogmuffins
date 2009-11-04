#
# Makefile for busy-wait IO tests
#
XCC     = gcc
AS	= as
LD      = ld
CFLAGS  = -c -fPIC -Wall -I. -Iinclude -mcpu=arm920t -msoft-float 
# -g: include hooks for gdb
# -c: only compile
# -mcpu=arm920t: generate code for the 920t architecture
# -fpic: emit position-independent code
# -Wall: report all warnings

ASFLAGS	= -mcpu=arm920t -mapcs-32
# -mapcs: always generate a complete stack frame

LDFLAGS = -init main -Map main/main.map -N  -T main/orex.ld -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2 -Llib

OBJECTS = arch/drivers.o arch/switch.o arch/ep93xx.o main/main.o main/syscalls.o main/td.o server/clockserver.o server/nameserver.o server/serialio.o task/gameplayer.o task/gameserver.o task/requests.o task/shell.o task/task.o task/traincontroller.o

HEADERS = include/*.h

all: main/main.elf 

.PRECIOUS: %.s
# the following line assumes that each .c file depends on all the header files
%.s: %.c $(HEADERS)
	$(XCC) -S $(CFLAGS) -o $@ $<

%.o: %.c

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

libraries: 
	cd src && make && cd ..

main/main.elf: $(OBJECTS) libraries
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) -lbwio -ldebug -lstring -lmath -lgcc 

arch/switch.o: arch/switch.S
	$(AS) $(ASFLAGS) -o arch/switch.o arch/switch.S
	
clean:
	rm -f main/main.elf main/main.map
	find . -name '*.[so]' -print | xargs rm -f
	find . -name '*~' -print | xargs rm -f


upload: 
	cp main/main.elf /u/cs452/tftpboot/ARM/dgoc/main.elf
	chmod a+rx -R /u/cs452/tftpboot/ARM/dgoc

copy:
	cp main/main.elf /u/cs452/tftpboot/ARM/becmacdo.elf
