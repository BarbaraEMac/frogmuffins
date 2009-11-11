#
# Makefile for busy-wait IO library
#
XCC     = gcc
AS	= as
AR	= ar
CFLAGS  = -c -fPIC -Wall -I. -I../include -mcpu=arm920t -msoft-float 
# -g: include hooks for gdb
# -c: only compile
# -mcpu=arm920t: generate code for the 920t architecture
# -fpic: emit position-independent code
# -Wall: report all warnings
# -msoft-float: use software for floating point

ASFLAGS	= -mcpu=arm920t -mapcs-32
# -mapcs-32: always create a complete stack frame

ARFLAGS = rcs

all:  bwio.a debug.a string.a math.a buffer.a

bwio.s: bwio.c
	$(XCC) -S $(CFLAGS) -o $@ $<

%.s: %.c 
	$(XCC) -S $(CFLAGS) -O2 -o $@ $<

%.o: %.c

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

%.a: %.o
	$(AR) $(ARFLAGS) $@ $<
	cp $@ ../lib/lib$@


clean:
	-rm -f *.s *.a *.o

