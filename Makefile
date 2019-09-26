CC	= gcc
AS	= as --32

%: %.c
	$(CC) $< -o $@

%.o: %.S
	$(AS) $< -o $@

all: boot

clean:
	rm -f *.o
	rm -f boot
	rm -f tools/build

boot: tools/build block.o
	tools/build block.o > $@

tools/build: tools/build.c

master.o: master.S

# block.o: block.S

hex:
	make
	hexdump -C master.o
	make clean

test:
	make clean
	make
	qemu-system-i386 -full-screen -fda boot
	make clean
