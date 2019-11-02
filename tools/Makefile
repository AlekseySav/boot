CC			= gcc
TOOLS		= build elf install

%: %.c
	$(CC) $< -o $@

tools: $(TOOLS)

elf: elf.c

build: build.c

install: install.c

clean:
	rm -f $(TOOLS)
	rm -f boot
