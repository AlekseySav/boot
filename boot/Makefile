CC		= gcc
CFLAG	= -I ../include
AS		= $(CC) $(CFLAG) -c
LD		= ld -T link.ld

%.o: %.S
	$(AS) $< -o $@

boot: boot.o
	$(LD) $^ -o $@

boot.o: boot.S
