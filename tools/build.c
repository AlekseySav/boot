#include <sys/types.h>          // for u_int32_t
#include <stdnoreturn.h>        // for noreturn
#include <stdlib.h>             // for exit
#include <stdio.h>              // for fprintf, stderr
#include <unistd.h>             // for read/write
#include <fcntl.h>              // for open/close

static noreturn void die(const char * msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static inline void usage(void)
{
    die("Usage: build munix");
}

int main(int argc, char * argv[])
{
    char buf[512];
    int fd;
    u_int32_t i;

    if(argc != 2)
        usage();
    
    if((fd = open(argv[1], O_RDONLY)) < 0)
        die("Unable to open boot file");

    /* 28 bytes:
     * 0-3 entry
     * 4-7 text offset      8-11 text size
     * 12-15 data offset    16-19 data size
     * 20-23 bss offset     24-27 bss size
     */
    if(read(fd, buf, 28) != 28)
        die("Unable to read boot header");

    if(((u_int32_t *)buf)[0] != 0)
        die("Invalid entry in boot");
    if(((u_int32_t *)buf)[1] != 0)
        die("Invalid text offset in boot");
    if(((u_int32_t *)buf)[2] == 0)
        die("Invalid text size in boot");
    
    i = ((u_int32_t *)buf)[2] + ((u_int32_t *)buf)[4] + ((u_int32_t *)buf)[6];
    if(i > 510)
        die("Boot size can't exceed 510 bytes");

    if(read(fd, buf, i) != i)
        die("Unable to read boot data");
    
    close(fd);

    while(i < 510)
        buf[i++] = 0;
    buf[510] = 0x55;
    buf[511] = 0xaa;

    if(write(1, buf, 512) != 512)
        die("Unable to write boot data");

    return 0;
}
