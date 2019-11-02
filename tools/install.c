#include <stdnoreturn.h>        // for noreturn
#include <stdlib.h>             // for exit
#include <stdio.h>              // for fprintf, stderr, printf, scanf
#include <unistd.h>             // for read/write
#include <fcntl.h>              // for open/close

static noreturn void die(const char * msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static inline void usage(void)
{
    die("Usage: install munix");
}

int main(int argc, char * argv[])
{
    char buf[512];
    int fd, dev, i;
    unsigned long rd = 0;

    if(argc != 2)
        usage();

    printf("Enter device name: ");
    scanf("%s", buf);

    if((fd = open(argv[1], O_RDONLY)) < 0)
        die("Unable to open kernel file");
    if((dev = open(buf, O_WRONLY)) < 0)
        die("Unable to open device descriptor. Make sure you have root rights");

    while((i = read(fd, buf, 512)) > 0) {
        if(write(dev, buf, i) != i)
            die("Unable to install kernel data");
        rd += i;
        printf("\rWritten %ld bytes\n", rd);
    }

    close(fd);
    close(dev);

    return 0;
}
