#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "../boot.h"

struct elf_section
{
    u_int32_t name;
    u_int32_t type;
    u_int32_t flags;
    u_int32_t addr;
    u_int32_t offset;
    u_int32_t size;
    u_int32_t link;
    u_int32_t info;
    u_int32_t addralign;
    u_int32_t entsize;
};

struct elf_symbol
{
    u_int32_t name;
    u_int32_t value;
    u_int32_t size;
    unsigned char info;
    unsigned char other;
    u_int32_t shndx;
};

#define EFL_HEADER 52

#define ELF_STRTAB  3

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

char buf[BUFSIZ];

#define to64(addr, off) ((((int64_t)addr) * sizeof(int32_t)) + (int64_t)off)

void boot(char * filename)
{
    FILE * f;
    int32_t sec_pos;
    int16_t sec_number; // number of sections
    int16_t sec_shstrtab;   // shstrtab index in sections
    struct elf_section * sections;
    char * shstrtab;    // pointer to .shstrtab source

    int64_t text_start;
    u_int32_t text_len;

    f = fopen(filename, "r");

    if(f == NULL)
        die("Can't open boot object file");

    if(fread(buf, EFL_HEADER, 1, f) != 1)
        die("Can't read ELF header of boot");

    // read header
    if(((int32_t *)buf)[0] != 0x464c457f)
        die("Invalid ELF magic number");
    if(((int8_t *)buf)[4] != 0x01)
        die("Invalid ELF class");
    if(((int8_t *)buf)[5] != 0x01)
        die("Invalid endian");
    if(((int8_t *)buf)[6] != 0x01)
        die("Illegal ELF header version");
    if(((int32_t *)buf)[2] || ((int32_t *)buf)[3])
        die("Illegal padding in ELF header");
    if(((int16_t *)buf)[8] != 0x0001)
        die("Invalid ELF type");
    if(((int16_t *)buf)[9] && ((int16_t *)buf)[9] != 0x0003)
        die("Invalid ELF instruction set");
    if(((int32_t *)buf)[5] != 0x00000001)
        die("Invalid ELF version");
    if(((int32_t *)buf)[6])
        die("Invalid program entry point");
    if(((int32_t *)buf)[7])
        die("Invalid program header position");
    sec_pos = ((int32_t *)buf)[8];
    if(((int32_t *)buf)[9])
        die("Invalid ELF flags");
    if(((int16_t *)buf)[20] != EFL_HEADER)
        die("Invalid ELF header size");
    if(((int16_t *)buf)[21])
        die("Invalid header table size");
    if(((int16_t *)buf)[22])
        die("Invalid number of headers");
    if(((int16_t *)buf)[23] != 40)
        die("Invalid section header table");
    sec_number = ((int16_t *)buf)[24];
    if(sec_number < 2)
        die("Invalid section header table");
    sec_shstrtab = ((int16_t *)buf)[25];
    if(sec_shstrtab >= sec_number)
        die("Invalid '.shstrtab' section position");
    
    // read all before sections
    if(fread(buf + EFL_HEADER, sec_pos - EFL_HEADER, 1, f) != 1)
        die("Can't read boot file source code");

    // read section table
    sections = malloc(sec_number * sizeof(struct elf_section));

    for(int16_t i = 0; i < sec_number; i++)
        if(fread(sections + i, sizeof(struct elf_section), 1, f) != 1)
            die("Can't read ELF header of boot");

    // get '.shstrtab' section
    shstrtab = buf + to64(sections[sec_shstrtab].addr, sections[sec_shstrtab].offset);

    // get all other sections
    for(int i = 0; i < sec_number; i++) {
        if(sections[i].type == 0)   // null section
            continue;
        if(strcmp(shstrtab + sections[i].name, ".text") == 0) {
            text_start = to64(sections[i].addr, sections[i].offset);
            text_len = sections[i].size;
        }
        else if(strcmp(shstrtab + sections[i].name, ".symtab") == 0);
        else if(strcmp(shstrtab + sections[i].name, ".strtab") == 0);
        else if(strcmp(shstrtab + sections[i].name, ".shstrtab") == 0);
        else if(strcmp(shstrtab + sections[i].name, ".rel.text") == 0);
        else if(sections[i].size != 0)
            die("Illegal %s section in boot", shstrtab + sections[i].name + 1);
    }

    // now text_start contains the start pof boot
    // and text_len its length

    if(text_len > BOOTSIZE)
        die("Boot size cannot exceed %d bytes", BOOTSIZE);
    
    for(int64_t i = text_start + text_len; i < text_start + BOOTSIZE; i++)
        buf[i] = 0;
    buf[text_start + SIGNPOS] = 0x55;
    buf[text_start + SIGNPOS + 1] = 0xaa;

    if(fwrite(buf + text_start, 512, 1, stdout) != 1)
        die("Cannot write boot data");

    fclose(f);
}

int main(int argc, char ** argv)
{
    if(argc != 2)
        usage();

    boot(argv[1]);
    //boot(argv[2]);
    return 0;
}
