/*
 * elf.c
 * 
 * dumps elf file
 * and returns entry and section info
 * format of output file:
 * 
 * 0-3 entry
 * 4-... section info in format
 *      0-3 section addres
 *      4-7 section length
 * ...-(eof) program data
 */

#include <assert.h>                 // for assert
#include <stdio.h>                  // for fprintf, stderr
#include <stdlib.h>                 // for c/re/malloc, free, exit
#include <stdnoreturn.h>            // for noreturn
#include <stdbool.h>                // for bool, true, false
#include <stddef.h>                 // for size_t
#include <fcntl.h>                  // for open/close
#include <unistd.h>                 // for read/write
#include <errno.h>                  // for errno
#include <string.h>                 // for strerror, strcmp
#include <sys/types.h>              // for u_in32_t, etc

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

    /* entry, phoff and shoff
     * have diffrent sizes
     * depending on arch (32/64)
     * (so, now only 32bit is sypported)
     */
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

struct program_header 
{
    u_int32_t type;
    u_int32_t offset;
    u_int32_t vaddr;
    u_int32_t paddr;        // unused
    u_int32_t filesz;
    u_int32_t memsz;
    u_int32_t flags;
    u_int32_t align;
};

struct section_header
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

struct section
{
    /*
     * addr and len
     * are writed to the output
     * by write_sections
     * don't touch them
     */
    u_int32_t addr;
    u_int32_t len;
    const char * name;
    char * buf;
};

struct program
{
    char * buf;
    size_t buf_len;

    size_t real_pos;
    size_t real_len;
};

struct table 
{
    size_t pos;
    size_t entry_size;
    size_t count;
};

struct table program_headers;
struct table section_headers;

unsigned string_section;

int fd;
size_t offset;

char * buf;
size_t pos;

struct section * sections;
size_t section_count;

struct program * programs;
size_t program_count;

static noreturn void die(const char * msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void usage(void)
{
    die("Usage: tools/elf [ sections ] <file>");
}

static void extend_buffer(size_t len)
{
    len += pos;
    if(buf && pos)
        buf = realloc(buf, len);
    else buf = malloc(len);

    if(!buf) die("Unable to allocate dynamic memory");
}

static void read_data(void * buf, size_t len)
{
    if(!len) return;
    
    if(read(fd, buf, len) != len)
        die(strerror(errno));
    offset += len;
}

static void write_data(void * buf, size_t len)
{
    if(!len) return;
    
    if(write(1, buf, len) != len)
        die(strerror(errno));
}

static void skip_data(size_t len)
{
    if(!len) return;

    extend_buffer(len);
    read_data(buf + pos, len);
    pos += len;
}

static void read_header(void)
{
    struct elf_header h;

    // read high part of header
    read_data(&h, sizeof(struct elf_header));

    if(h.magic != 0x464c457f)
        die("Invalid ELF magic number");
    if(h.version != 1)
        die("Unrecognized ELF version");
    if(h.type != 2)
        die("Only executable format is supported");
    
    if(h.info[0] != 1)
        die("Illegal ELF class (only 32-bit class is supported)");
    if(h.info[2] != 1)
        die("Unrecognized ELF header version");
    if(h.info[3] != 0)
        die("Unrecognized OS/ABI type");
    if(h.info[4] != 0)
        die("Unrecognized ABI version");

    if(h.ehsize != sizeof(struct elf_header))
        die("Invalid ELF header");

    if(h.phnum == 0)
        die("No program headers");
    if(h.shnum == 0)
        die("No section headers");

    write_data(&h.entry, sizeof(h.entry));
    
    program_headers.pos = h.phoff;
    program_headers.count = h.phnum;
    program_headers.entry_size = h.phentsize;

    section_headers.pos = h.shoff;
    section_headers.count = h.shnum;
    section_headers.entry_size = h.shentsize;

    string_section = h.shstrndx;
}

#define rounded(size, align) \
    (((size + align - 1) / align) * align)

static void read_program_headers(void)
{
    struct program_header * p;
    size_t i;
    register size_t e = program_headers.entry_size;
    
    if(e != sizeof(struct program_header))
        die("Invalid program header size");

    programs = malloc(e * sizeof(struct program));
    e *= program_headers.count;

    p = malloc(e);
    assert(p);
    read_data(p, e);

    for(i = 0; i < program_headers.count; i++) {
        if(p[i].type == 0)
            continue;
        if(p[i].type != 1)
            die("Illegal program header type");
        if(p[i].offset < offset)
            die("Invalid program header offset");
        if(p[i].vaddr != p[i].paddr)
            die("Invalid program header physical addres");
        if(p[i].vaddr < pos)
            die("Invalid program header virtual addres");
        if((p[i].flags & (4)) == 0)
            die("File contains non-readable code");

        if(p[i].offset > offset)
            skip_data(p[i].offset - offset);

        programs[i].buf = buf + pos;
        programs[i].buf_len = p[i].filesz;
        programs[i].real_pos = p[i].vaddr;
        programs[i].real_len = rounded(p[i].memsz, p[i].align);

        skip_data(p[i].filesz);
    }

    free(p);
}

static void read_section_headers(void)
{
    struct section_header * s;
    ptrdiff_t off_to_pos;
    size_t i, j;
    char * names;
    register size_t e = section_headers.entry_size;
    
    if(e != sizeof(struct section_header))
        die("Invalid section header size");
    
    e *= section_headers.count;
    off_to_pos = pos - offset;

    s = malloc(e);
    assert(s);
    read_data(s, e);

    if(s[string_section].type != 3)
        die("Illegal symbol table");
    names = buf + off_to_pos + s[string_section].offset;

    for(i = 0; i < section_headers.count; i++) {
        if(s[i].type == 0)
            continue;
        for(j = 0; j < section_count; j++) {
            if(sections[j].buf)
                continue;
            if(strcmp(sections[j].name, names + s[i].name) == 0) {
                sections[j].addr = s[i].addr;
                sections[j].len = s[i].size;
                sections[j].buf = buf + off_to_pos + s[i].offset;
            }
        }
    }
    free(s);
}

static inline void read_body(void)
{
    skip_data(program_headers.pos - offset);
    read_program_headers();
    skip_data(section_headers.pos - offset);
    read_section_headers();
}

static void write_sections(void)
{
    char zero = 0;
    size_t i, j;
    size_t written;

    // write addr and len parameters
    for(i = 0; i < section_count; i++)
        write(1, &sections[i], 8);

    // write sections
    written = 0;
    for(i = 0; i < section_count; i++) {
        if(written < sections[i].addr)
            for(j = 0; j < sections[i].addr - written; j++)
                write_data(&zero, 1);
        write_data(sections[i].buf, sections[i].len);
    }

    // get program header, which contains last section
    for(i = 0; i < program_count; i++)
        if(programs[i].real_pos >= written &&
            programs[i].real_len + programs[i].real_pos < written)
                break;
    
    for(j = 0; j < programs[i].real_len + programs[i].real_pos - written; j++)
        write_data(&zero, 1);
}

int main(int argc, char * argv[])
{
    size_t i;

    if(argc < 3) usage();
    
    section_count = argc - 2;
    sections = calloc(section_count, sizeof(struct section));
    for(i = 1; i < argc - 1; i++)
        sections[i - 1].name = argv[i];
    
    fd = open(argv[i], O_RDONLY);       // i is equal to argc - 1
    if(fd < 0)
        die(strerror(errno));

    read_header();
    read_body();

    close(fd);
    
    write_sections();
    
    free(buf);
    free(sections);
    free(programs);

    return 0;
}
