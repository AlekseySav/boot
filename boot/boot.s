!
! boot.s
!
! i copied it from .../munix to .../boot
!
!
! this is the second version of munix bootloader
! i hope to forget my previous ugly boot...
!
! boot.s is loaded by bios at 0x7c00 addr (0x07c0 segment)
! then it copies itself to 0x90000 addres (0x9000 segment)
!
! next it loads system to 0x1000 segment, disables
! bios interrupts and moves kernel to 0x000 addr (start of memory)
! before jumping to kernel, bootloader goes to protected mode
! (it loads a20 line, gdt and unitialize interrupts)
! then first command of kernel is loaded
! system must reload gdt and idt by itself later
!
! NOTE: max size of kernel is ~0x80000 bytes = 512KiB
! there wouldn't be any problems with it now (for sure)
! and they could only appear in the long future after
! by now, i want keep it as simple as possible
! (because bootblock must be 510 bytes at most
! and i'm to lazy to do more stages)
!
! NOTE 2: bootblock works only for floppies
! it means, you could lauhch kernel only from
! /dev/fd0. not from /dev/hd.. or /dev/fd1, etc.
!
! NOTE 3: i don't know, how to make reload procces,
! so, reboot by hand...
!

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

BOOTSEG = 0x07c0                                            ! bios puts boot in this segment
INITSEG = 0x9000                                            ! boot move itself here

SYSSEG  = 0x1000                                            ! save kernel in this segment
SYSSIZE = 0x8000                                            ! kernel size in bytes * 16

MAGIC   = 510                                               ! location of 0xaa55 magic number

entry start
start:
    mov ax, #BOOTSEG
    mov ds, ax                                              ! ds = BOOTSEG
    mov ax, #INITSEG
    mov es, ax                                              ! es = INITSEG
    mov cx, #256                                            ! size of bootloader in words
    xor di, di
    xor si, si
    cld
    rep                                                     ! repeat cx(256) times
    movsw
    jmpf go, #INITSEG                                       ! far jump to INITSEG
go:
! we are in INITSEG now
! so, we should to reload all segments

    mov ax, cs                                              ! cs reloaded automaticly
    mov ds, ax
    mov es, ax

! may not to load fs ang gs segments,
! but i do it for safety :)

    mov fs, ax
    mov gs, ax
    cli
    mov ss, ax                                              ! set stack
    mov sp, #0x400                                          ! value >> 512
    sti
    
! now we can print message

    call print
    .ascii "\n\r\n\rMunix Bootloader 2.0\n\r\0"

! ok, we've written the message, now
! we want to load the system (at 0x10000)
! and close floppy

    mov ax, #SYSSEG
    mov es, ax
    mov ax, #SYSSIZE
    call read_kernel
    call kill_monitor

! before going to protected mode,
! save cursor position, using bios

    mov ah, #0x03                                           ! read cursor
    xor bh, bh
    int 0x10
    mov MAGIC, dx                                           ! save cursor to [0x9000:MAGIC]

    cli                                                     ! ban interrupts!

    call do_move                                            ! copy system from sysseg to 0x000 addr
    call line20                                             ! enable a20 line

! load interrupt and global descriptor tables

    lidt idt_d
    lgdt gdt_d

! ok, it was fun,
! but we have to switch to protected mode :-(

    mov ax, #1
    lmsw ax                                                 ! set protected mode bit active

! ...and set all segment registers to 0x10
! (location of data descriptor in gdt)

    mov ax, #0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

! this is last thing, which bootloader do:
! far jump to our kernel (it located at 0x0000)

    jmpf 0, 8                                               ! 0x8 is code seg (defined in gdt)

! that's all
! next code is functions and utilities,
! used in bootloader

die:
    call print
    .ascii "\n\rE: Failed booting Munix.\n\r\0"
    .word 0xfeeb                                            ! loop forever

! this routine loads kernel to SYSSEG
! in:   es must be equal to SYSSEG for correct work
!       ax is size of kernel
! NOTE: ax must be devided by 32 (or last sector won't be loaded)
! out:  es - end of kernel if all was correct
read_kernel:
    pusha
    shr ax, #5                                              ! from bytes to sectors (equal ax = ax / 32)
    mov size, ax
    xor ah, ah                                              ! tell bios to reload disk
    xor dl, dl                                              ! dl = 0 - use curr disk
    int 0x13                                                ! bios interrupt working with disks
    jc die                                                  ! error? jump to die

    mov ah, #0x08                                           ! learn disk geometry
    xor dl, dl                                              ! our drive
    push es
    xor di, di                                              ! need to correct work (some bioses crash if not)
    mov es, di
    int 0x13
    pop es
    jc die

    inc dh                                                  ! dh + 1 is number of heads per track
    and cl, #0x3f                                           ! 6 lowest bits of cl is secctors per track (0x3f = 111111b)    
    mov heads, dh                                           ! save heads/track
    mov sectors, cl                                         ! save sectors/track
rp_read:
    call read_setreg                                        ! set correct registers for 0x13
    int 0x13
    jc die

    mov ax, curr
    inc ax
    mov curr, ax
    mov ax, es
    add ax, #32                                             ! 32 = 512 / 16 (go to next segment)
    mov es, ax
    mov ax, curr
    sub ax, #2
    cmp ax, size                                            ! have we read all yet?
    jl rp_read
read_end:
    popa
    ret

read_setreg:
    mov ax, curr
    dec ax
    mov bl, sectors
    div bl                                                  ! ah = cuur % sectors; al = curr / sectors
    mov bl, al
    inc ah                                                  ! ah is current sector now
    mov dl, ah                                              ! save it
    xor bh, bh
    mov ax, bx                                              ! ax = bl = curr / sectors
    mov bl, heads
    div bl                                                  ! ah = curr head, al = curr cylinder

    mov ch, al
    and ch, #0xff                                           ! ch = cylinder & 0xff
    mov cl, al
    shr cl, #2
    and cl, #0xc0
    or cl, dl                                               ! dl is stil current sector
    mov dh, ah                                              ! dh is current head now

    mov ah, #0x02                                           ! tell bios to read sectors
    mov al, #0x01                                           ! 1 sector
    xor bx, bx                                              ! es:bx - output buffer
    xor dl, dl                                              ! our drive
    ret

! this params are used by read_kernel function
heads:      .byte 0
sectors:    .byte 0
size:       .word 0
curr:       .word 2

! close floppy function
kill_monitor:
	push dx
	mov dx, #0x3f2
	mov al, #0
	outb
	pop dx
	ret

! do move utility
do_move:
    push ds
    push es
    xor	ax, ax
	cld			                                            ! 'direction'=0, movs moves forward
dm_loop:
	mov	es, ax		                                        ! destination segment
	add	ax, #SYSSEG
	cmp	ax, #SYSEND
	jz	dm_done
	mov	ds, ax		                                        ! source segment
	sub	di, di
	sub	si, si
	mov cx, #SYSSIZE
	rep
	movsw
	jmp dm_loop
dm_done:
    pop es
    pop ds
    ret

! const uses by do_move util
SYSEND = SYSSEG + SYSSIZE

! line20 utility
! enables A20 line (need for 32-bit system)
line20:
    call a20wait
    mov al, #0xad
    out #0x64, al

    call a20wait
    mov al, #0xd0
    out #0x64, al
 
    call a20waitd
    in al, #0x60
    push eax
 
    call a20wait
    mov al, #0xd1
    out #0x64, al
 
    call a20wait
    pop eax
    or al, #2
    out #0x60, al
 
    call a20wait
    mov al, #0xae
    out #0x64, al
 
    call a20wait
    ret

a20wait:                                                    ! wait until release keyboard
    in al, #0x64
    test al, #2
    jnz a20wait
    ret

a20waitd:                                                   ! wait until get key data
    in al, #0x64
    test al, #1
    jz a20waitd
    ret

! print utility
print:
    pop si                                                  ! the value after 'call print'
    push ax
    push bx
    mov ah, #0x0e                                           ! print char
    mov bx, #0x01                                           ! foreground mode
pr_loop:
    lodsb                                                   ! al = si; si++
    or al, al
    jz pr_done                                              ! exit if al is 0
    int 0x10                                                ! else print al
    jmp pr_loop
pr_done:
    pop bx
    pop ax
    jmp si                                                  ! si points to space after msg

idt_d:                                                      ! idt is unitialized
    .word 0
    .word 0, 0

gdt_d:
    .word 3 * 8 - 1
    .word gdt, 0x9                                          ! 0x90000 + gdt = INITSEG:gdt

gdt:
	.word	0,0,0,0		                                    ! null descriptor

	.word	0x07ff		                                    ! 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		                                    ! base address=0
	.word	0x9a00		                                    ! code read/exec
	.word	0x00c0		                                    ! granularity=4096, 386

	.word	0x07ff		                                    ! 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		                                    ! base address=0
	.word	0x9200		                                    ! data read/write
	.word	0x00c0		                                    ! granularity=4096, 386

.text
endtext:
.data
enddata:
.bss
endbss:
