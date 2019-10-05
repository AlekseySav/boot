/*
* head.s 
*
* contains 32-bit startup code of kernel
*/

.globl start, idt, gdt

start:
	lss stack_start, %esp									/* setup stack */

    call setup_idt
    call setup_gdt

    movl $0x10, %eax                                        /* load all segments to 0x10 */
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
	lss stack_start, %esp

    xorl %eax,%eax
1:	incl %eax		                                        /* check that A20 really IS enabled */
	movl %eax,0x000000
	cmpl %eax,0x100000
	je 1b

	movl %cr0, %eax		                                    /* check math chip */
	andl $0x80000011, %eax	                                /* Save PG,ET,PE */
	testl $0x10, %eax
	jne 1f			                                        /* ET is set - 387 is present */
	orl $4, %eax		                                    /* else set emulate bit */
1:	movl %eax, %cr0

    jmp afterpg

/*
* setup_idt
*
* sets all interrupt gates of idt as
* pointers to ignore_int functions
*/
setup_idt:
	lea ignore_int, %edx
	movl $0x00080000, %eax
	movw %dx, %ax		                                    /* selector = 0x0008 = cs */
	movw $0x8e00, %dx	                                    /* interrupt gate - dpl=0, present */

	lea idt, %edi
	mov $256, %ecx                                          /* size of idt */
rp_sidt:
	movl %eax, (%edi)
	movl %edx, 4(%edi)
	addl $8, %edi
	dec %ecx
	jne rp_sidt
	lidt idt_d
	ret

/*
* setup_gdt
*
* this routine just only loads
* gdt descriptor (using lgdt)
* and reloads cs (by ret command)
*/
setup_gdt:
    lgdt gdt_d
    ret

/*
* all code, which is located earlier '.org',
* will be cleared later
*/
.org 0x1000

afterpg:
    call main
    cli
    hlt
1:  jmp 1b

.align 2
ignore_int:
	movb $2, %al
	incb 0xb8000		                                	/* put something on the screen */
	movb %al, 0xb8001		                                /* so that we know something */
	iret

.align 2
.word 0
idt_d:
    .word 256 * 8 - 1
    .long idt

.align 2
.word 0
gdt_d:
	.word 256 * 8 - 1
	.long gdt

idt:
    .fill 256, 8, 0

gdt:
    .quad 0x0000000000000000	                            /* NULL descriptor */
	.quad 0x00c09a00000007ff	                            /* 8Mb */
	.quad 0x00c09200000007ff	                            /* 8Mb */
	.quad 0x0000000000000000	                            /* TEMPORARY - don't use */
	.fill 252, 8, 0			                                /* space for LDT's and TSS's etc */
