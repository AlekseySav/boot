#include <boot.h>

.text
.code16

ENTRY(start)

    movw $BOOTSEG, %ax
    movw %ax, %ds
    movw $INITSEG, %ax
    movw %ax, %es
    movw $512/2, %cx
    xorw %di, %di
    xorw %si, %si
    cld
rep movsw
    ljmp $INITSEG, $go
go:
    movw %cs, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    cli
    movw %ax, %ss
    xorw %sp, %sp
    sti

    call print
.ascii "\n\rLoading system ...\n\r\n\r\0"

    jmp die
die:
    call print
.ascii "Failed booting Munix\n\r\0"
1:  jmp 1b

print:
    popw %si
    movw $0x01, %bx
    movb $0x0e, %ah
pr_loop:
    lodsb
    orb %al, %al
    jz pr_end
    int $0x10
    jmp pr_loop
pr_end:
    jmp * %si
