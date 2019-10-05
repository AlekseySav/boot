AS86 = as86 -0 -a
LD86 = ld86 -0

CC	= gcc -m32
LD = ld -s -x -m elf_i386 -T link.ld

AS	= as --32

%: %.c
	$(CC) $< -o $@

%.o: %.s
	$(AS) -o $@ $<

%.o: %.c
	$(CC) -c $< -o $@

%.o: %.S
	$(AS) $< -o $@

all: 
	make clean
	make kernel

kernel: tools/build boot/boot tools/system
	tools/build boot/boot tools/system > $@

tools/build: tools/build.c

boot/boot: boot/boot.s
	$(AS86) -o $@.o $<
	$(LD86) -0 -s -o $@ $@.o

tools/system: boot/head.o init/main.o
	$(LD) $^ -o $@

boot/head.o: boot/head.s

init/main.o: init/main.c

clean:
	rm -f init/*.o
	rm -f boot/*.o
	rm -f boot/boot
	rm -f tools/build

hex:
	make
	hexdump -C kernel
	make clean

test:
	make clean
	make
	qemu-system-i386 -full-screen -fda kernel
	make clean
