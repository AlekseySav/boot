/*
 * elf.c
 * 
 * unpack elf32 file
 * and save it as array of
 * .text, .data and .bss sections
 * (and entry point, of course)
 * 
 * supports only elf32
 * for correct work, compile elf-file with gld (ld);
 * linking must starts from the beginning of memory (. = 0)
 */

#include <assert.h>
#include <sys/types.h>
#include <stdnoreturn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

struct elf_header
{
    u_int32_t magic;

    /* info[0] - class
     * info[1] - endian
     * info[2] - header version
     * info[3] - OS ABI 
     * info[4] - ABI version
     * info[5-11] - unused 
     */
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

struct elf_program {
    u_int32_t type;
    u_int32_t offset;
    u_int32_t vaddr;
    u_int32_t paddr;        // unused
    u_int32_t filesz;
    u_int32_t memsz;
    u_int32_t flags;
    u_int32_t align;
};

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

noreturn void die(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    
    va_end(ap);
    exit(1);
}

void usage(void)
{
    die("Usage: tools/elf [format] <file>\n\n"
        "Supported formats:\n"
        "a.out      unix/minix output format\n"
        "bin        file without header\n");
}

static inline void write_aout(void * buf, u_int32_t text, u_int32_t data, u_int32_t bss, u_int32_t entry)
{
    if(entry)
        die("Invalid entry for bin file");
    
    struct {
        u_int32_t magic;
        u_int32_t text;
        u_int32_t data;
        u_int32_t bss;
        u_int32_t syms;
        u_int32_t entry;
        u_int32_t trsize;
        u_int32_t drsize;
    } exec = { 0 };

    exec.magic = 0x04100301;
    exec.text = text;
    exec.data = data;
    exec.bss = bss;
    exec.entry = entry;

    write(1, &exec, sizeof(exec));
    write(1, buf, text + data + bss);
}

static inline void write_bin(void * buf, u_int32_t text, u_int32_t data, u_int32_t bss, u_int32_t entry)
{
    if(entry)
        die("Invalid entry for bin file");
    write(1, buf, text + data + bss);
}

void write_fmt(char * fmt, void * buf, u_int32_t text, u_int32_t data, u_int32_t bss, u_int32_t entry)
{
    if(strcmp(fmt, "a.out") == 0)
        write_aout(buf, text, data, bss, entry);
    else if(strcmp(fmt, "bin") == 0)
        write_bin(buf, text, data, bss, entry);
    else usage();
}

int main(int argc, char * argv[])
{
    u_int32_t text, data, bss, entry;
    void * buf;                     // buffer for exec code

    // pos - position in buf
    // off - position in file
    size_t i, size, pos, off;
    void * table;                   // memory for program and section headers
    char * symtab;

    struct elf_header h;
    struct elf_program p;
    struct elf_section s;
    const char * names;             // start of strtab

    int fd;
    char * f;                       // filename

    if(argc != 3)
        usage();

    f = argv[2];
    buf = NULL;
    pos = 0;

    if((fd = open(f, O_RDONLY)) < 0)
        die("Unable to open '%s' file", f);
    
    if(read(fd, &h, sizeof(struct elf_header)) != sizeof(struct elf_header))
        die("Unable to read ELF header");

    if(h.magic != 0x464c457f)
        die("Invalid ELF magic number");
    if(h.version != 1)
        die("Unrecognized ELF version");

    if(h.info[0] != 1)              // 32-bit format
        die("Only 64-bit formats are supported");
    if(h.info[3] != 0)
        die("Unrecognized OS/ABI type");
    if(h.info[3] != 0)
        die("Unrecognized ABI version");

    if(h.type != 2)                 // executeble format
        die("Only executable formats are supported");
    if(h.ehsize != sizeof(struct elf_header))
        die("Invalid ELF header size");
    if(h.phentsize != sizeof(struct elf_program))
        die("Invalid elf program header size");

    off = h.ehsize;
    entry = h.entry;

    /* program header table
     * should be at the begining of program
     * and section header table 
     * at the ending 
     */

    if(h.phoff && h.phoff < h.ehsize)
        die("Invalid offset of program header table");
    if(h.shoff && h.shoff < h.ehsize)
        die("Invalid offset of section header table");

    if(h.phoff > h.shoff)
        die("Section table must be after program table");
    if(h.phoff != h.ehsize)
        die("Illegal program header table position");

    // read program headers

    size = h.phentsize * h.phnum;

    table = malloc(size);
    if(table == NULL)
        die("Unable to allocate memory for header tables");
    
    if(read(fd, table, size) != size)
        die("Unable to read program header table");
    off += size;

    for(i = 0; i < h.phnum; i++) {
        memcpy(&p, (struct elf_program *)table + i, h.phentsize);
        if(p.type && p.type != 1)       // not load file
            die("Illegal program header type");
        if(pos > p.vaddr)
            die("Invalid program virtual addres");
        if(off > p.offset)
            die("Invalid program offset");
        
        size = ((p.memsz + p.align - 1) / p.align) * p.align;  // round value
        size += p.vaddr;

        if(!buf) buf = malloc(size);
        else buf = realloc(buf, size);

        if(buf == NULL)
            die("Unable to allocate memory for code buffer");

        while(pos < p.vaddr)
            ((char *)buf)[pos++] = 0;   // clear padding
        
        if(off < p.offset) {            // read padding from file
            size_t x = p.offset - off;
            symtab = malloc(x);
            assert(symtab);
            if(read(fd, symtab, x) != x)
                die("Unable to read padding");
            free(symtab);
            off = p.offset;
        }

        if(read(fd, buf + pos, p.filesz) != p.filesz)
            die("Unable to read code data");

        pos += p.filesz;
        off += p.filesz;
        while(pos < size)
            ((char *)buf)[pos++] = 0;   // set align
    }

    // read section headers
    
    if(off > h.shoff)
        die("Illegal section header table position");

    size = h.shoff - off;
    if(size) {
        symtab = malloc(size);
        if(!symtab)
            die("Unable to allocate memory for symbol table");
        if(read(fd, symtab, size) != size)
            die("Unable to read symbol table");
    }
    else symtab = NULL;

    size = h.shentsize * h.shnum;
    free(table);
    
    table = malloc(size);
    if(table == NULL)
        die("Unable to allocate memory for header tables");

    if(read(fd, table, size) != size)
        die("Unable to read program header table");

    names = symtab + ((struct elf_section *)table)[h.shstrndx].offset - off;
    text = data = bss = 0;

    for(i = 0; i < h.shnum; i++) {
        memcpy(&s, (struct elf_section *)table + i, h.shentsize);
        if(s.type != 1)             // progbits
            continue;
        if(strcmp(names + s.name, ".text") == 0)
            text = s.size;
        else if(strcmp(names + s.name, ".data") == 0)
            data = s.size;
        else if(strcmp(names + s.name, ".bss") == 0)
            bss = s.size;
        else die("Invalid section");
    }

    free(table);
    free(symtab);
    
    write_fmt(argv[1], buf, text, data, bss, entry);

    free(buf);
    close(fd);

    return 0;
}
