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

OBJECTS = main/main.o arch/switch.o task/requests.o main/td.o task/task.o syscall/syscalls.o server/nameserver.o task/gameserver.o task/gameplayer.o

all: main/main.elf 

.PRECIOUS: %.s
%.s: %.c $(wildcard ../include/*.h)
	$(XCC) -S $(CFLAGS) -o $@ $<

%.o: %.c

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

main/main.elf: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) -lbwio -ldebug -lstring -lgcc 

arch/switch.o: arch/switch.S
	$(AS) $(ASFLAGS) -o arch/switch.o arch/switch.S
	
clean:
	rm -f main/main.elf main/main.map
	find . -name '*.[so]' -print | xargs rm -f
	find . -name '*~' -print | xargs rm -f


upload: 
	cp main/main.elf /u/cs452/tftpboot/ARM/dgoc/main.elf
	chmod a+rx -R /u/cs452/tftpboot/ARM/dgoc

copy2:
	cp main/main.elf /u/cs452/tftpboot/ARM/becmacdo_a2.elf
