#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "boot.h"

#define AOUT_HEADER 32

struct elf_header
{
    u_int32_t magic;
    u_int8_t info[12];

    u_int16_t type;
    u_int16_t machine;
    u_int32_t version;
    u_int32_t entry;
    u_int32_t phoff;
    u_int32_t shoff;
    u_int32_t flags;
    u_int16_t ehsize;
    u_int16_t phentsize;
    u_int16_t phnum;
    u_int16_t shentsize;
    u_int16_t shnum;
    u_int16_t shstrndx;
};

#define ELF_TYPE_NONE	0
#define ELF_TYPE_REL	1
#define ELF_TYPE_EXEC	2
#define ELF_TYPE_DYN	3
#define ELF_TYPE_CORE	4

struct elf_program_header {
    u_int32_t type;
    u_int32_t offset;
    u_int32_t vaddr;
    u_int32_t paddr;        // unused
    u_int32_t filesz;
    u_int32_t memsz;
    u_int32_t flags;
    u_int32_t align;
};

#define ELF_PROGRAM_NULL        0
#define ELF_PROGRAM_LOAD        1
#define ELF_PROGRAM_DYNAMIC     2
#define ELF_PROGRAM_INTERP      3
#define ELF_PROGRAM_NOTE        4

#define ELF_PROGRAM_FLAG_EXEC   1
#define ELF_PROGRAM_FLAG_WRITE  2
#define ELF_PROGRAM_FLAG_READ   4

void die(const char * msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

void usage(void)
{
    die("Usage: build boot image");
}

#define to64(addr, off) ((((int64_t)addr) * sizeof(int32_t)) + (int64_t)off)

void boot(const char * filename)
{
    char buf[512];

    FILE * f = fopen(filename, "r");
    if(f == NULL) 
        die("Can't open boot file");

    if(fread(buf, AOUT_HEADER, 1, f) != 1)
        die("Can't read boot header");
    if(((u_int32_t *)buf)[0] != 0x04100301)
        die("Non-Minix header in boot file");
    if(((u_int32_t *)buf)[1] != AOUT_HEADER)
        die("Non-Minix header in boot file");
    if(((u_int32_t *)buf)[3] != 0)
        die("Invalid data section in boot file");
    if(((u_int32_t *)buf)[4] != 0)
        die("Invalid bss section in boot file");
    if(((u_int32_t *)buf)[5] != 0)
        die("Non-Minix header in boot file");
    if(((u_int32_t *)buf)[7] != 0)
        die("Invalid symbol table in boot file");
    
    for(int i = 0; i < 510; i++) buf[i] = 0;
    
    if(fread(buf, 1, sizeof(buf), f) > 510)
        die("size of boot can't exceed 510 bytes");
    fclose(f);

    buf[510] = 0x55;
    buf[511] = 0xaa;

    if(fwrite(buf, 1, 512, stdout) != 512)
        die("Failed write boot data");
}

void kernel(const char * filename)
{
    FILE * f;
    char * buf;
    size_t i, off = 0;
    struct elf_header h;
    struct elf_program_header program, p;

    f = fopen(filename, "r");

    if(f == NULL)
        die("Can't open system file");

    if(fread(&h, sizeof(struct elf_header), 1, f) != 1)
        die("Can't read ELF header");
    off += sizeof(struct elf_header);

    if(h.magic != 0x464c457f)
        die("Invalid ELF magic number");
    if(h.info[0] != 0x01)
        die("Invalid ELF class");
    if(h.info[1] != 0x01)
        die("Invalid endian");
    if(h.info[2] != 0x01)
        die("Illegal ELF header version");
        
    for(i = 3; i < 12; i++)
        if(h.info[i])
            die("Illegal padding in ELF header"); 

    if(h.type != ELF_TYPE_EXEC)
        die("Invalid ELF type");
    if(h.machine && h.machine != 0x0003)
        die("Invalid ELF instruction set");
    if(h.version > 1)
        die("Invalid ELF version");

    if(h.flags)
        die("Invalid ELF flags");

    if(h.ehsize != sizeof(struct elf_header))
        die("Invalid ELF header size");
    if(h.phentsize != sizeof(struct elf_program_header))
        die("Invalid ELF program header size");

    if(!h.phnum)
        die("Can't find program header table");
    if(h.phnum != 2)
        die("Illegal program headers number");  // phnums set to 2 by link.ld
    if(h.phoff != sizeof(struct elf_header))
        die("Can't find program header table");

    for(i = 0; i < h.phnum; i++) {
        if(fread(&p, sizeof(struct elf_program_header), 1, f) != 1)
            die("Can't read ELF program header");
        off += sizeof(struct elf_program_header);
        if(p.type == ELF_PROGRAM_LOAD) {
            if(p.vaddr != 0 || p.paddr != 0)
                die("Illegal section addres");
            if(p.flags & ELF_PROGRAM_FLAG_EXEC == 0)
                die("Kernel image does not executable");
            if(p.flags & ELF_PROGRAM_FLAG_READ == 0)
                die("Kernel image does not readable");
            program = p;
        }
    }

    if(off > program.offset)
        die("Invalid program offset");

    buf = calloc(1, program.memsz);

    for(i = off; i < program.offset; i++)
        if(fread(buf, 1, 1, f) != 1)
            die("Invalid program offset");
        
    buf[0] = 0;
    
    if(fread(buf + program.vaddr, program.filesz, 1, f) != 1)
        die("Can't read program data");
    fclose(f);

    if(fwrite(buf, program.memsz, 1, stdout) != 1)
        die("Can't write system data");

    free(buf);
}

int main(int argc, char ** argv)
{
    if(argc != 3)
        usage();

    boot(argv[1]);
    kernel(argv[2]);
    return 0;
}
