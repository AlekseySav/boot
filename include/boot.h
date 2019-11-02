#ifndef _BOOT_H_
#define _BOOT_H_

#define NL ;

#define GLOBAL(x) \
    .globl x NL \
    x:

#define ENTRY(x) \
    GLOBAL(_start) NL \
    x:  

#define BOOTSEG 0x07c0
#define INITSEG 0x0060

#define SECTION(x) \
    .section .x NL

#endif
