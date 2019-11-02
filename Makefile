CC		= gcc
CFLAG	= -I include
AS		= $(CC) $(CFLAG) -c
LD		= ld -T tools/link.ld

usage:
	@echo "Usage:" >&2
	@echo "  make image     		build image file" >&2
	@echo "  make install   		install kernel" >&2
	@echo "  make clean     		delete all object files etc" >&2
	@echo "  make uninstall   		delete kernel file" >&2
	@echo "  make usage     		display this help" >&2
	@echo "  make           		same as make usage" >&2

image: tools/build tools/boot
	tools/build tools/boot > $@

install: tools/install image
	tools/install image

clean:
	(cd tools; make clean)
	(cd boot; rm -f boot boot.o)

uninstall:
	rm -f image

tools/boot: tools/elf boot/boot
	tools/elf .text .data .bss boot/boot > $@

boot/boot: 
	(cd boot; make boot)

tools/elf:
	(cd tools; make elf)

tools/build:
	(cd tools; make build)

tools/install:
	(cd tools; make install)
