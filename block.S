BOOTSEG         = 0x07c0
INITSEG         = 0x0060

SIGNPOS         = 510

BOOTSIZE        = SIGNPOS

.code16
.section ".text"

start:
    movw $BOOTSEG, %ax
    movw %ax, %ds                       # ds = BOOTSEG
    movw $INITSEG, %ax
    movw %ax, %es                       # es = INITSEG
    movw $BOOTSIZE/2, %cx               # cx = size of boot in words
    xorw %di, %di
    xorw %si, %si
    cld
    rep movsw                           # copy boot from BOOTSEG to INITSEG
    jmp $INITSEG, $go
go:                                     # it is already in INITSEG here
    movw %cs, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    movw %ax, %ss
    movw $0x200, %sp

    call print
    .ascii "\n\rLoading system ...\n\r\0"

    jmp die
die:
    call print
    .ascii "Booting failed. Hit any key to reboot.\n\r\0"
    xorb %ah, %ah                       # wait for key
    int $0x16
    int $0x19
    .word 0xfeeb                        # if cannot reboot, loop forever
    
    
print:
    popw %si                            # value after 'call print'
    movb $0x0e, %ah
    movw $0x0001, %bx
pr_loop:
    lodsb
    orb %al, %al
    jz pr_done
    int $0x10
    jmp pr_loop
pr_done:
    jmp * %si                           # value after message
