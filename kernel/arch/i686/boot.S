.set PG_PRESENT,	1<<0
.set PG_READ_WRITE,	1<<1
.set PG_PAGE_SIZE,	1<<7

.set FB_WIDTH,	800
.set FB_HEIGHT,	600
.set FB_BPP,	32

#define KERNEL_OFFSET 0xC0000000
#define V2P(vaddr) ((vaddr) - KERNEL_OFFSET)

.code32

# multiboot2 header
.section .multiboot, "aw"
	.align 8
multiboot2_start:
	.long 0xE85250D6
	.long 0
	.long multiboot2_end - multiboot2_start
	.long -(0xE85250D6 + (multiboot2_end - multiboot2_start))

	# framebuffer tag
	.align 8
	.short 5
	.short 0
	.long 20
	.long FB_WIDTH
	.long FB_HEIGHT
	.long FB_BPP

	# legacy start
	.align 8
	.short 3
	.short 0
	.long 12
	.long V2P(_start)

	.align 8
	.short 0
	.short 0
	.long 8
multiboot2_end:

.section .bananboot, "aw"
	.align 8
bananboot_start:
	.long 0xBABAB007
	.long -(0xBABAB007 + FB_WIDTH + FB_HEIGHT + FB_BPP)
	.long FB_WIDTH
	.long FB_HEIGHT
	.long FB_BPP
bananboot_end:

.section .bss, "aw", @nobits
	.align 4096
	boot_stack_bottom:
		.skip 4096 * 4
	boot_stack_top:

	.global g_kernel_cmdline
	g_kernel_cmdline:
		.skip 4096

	bootloader_magic:
		.skip 8
	bootloader_info:
		.skip 8

.section .data

# Map first GiB to 0x00000000 and 0xC0000000
.align 32
boot_pdpt:
	.long V2P(boot_pd) + (PG_PRESENT)
	.long 0
	.quad 0
	.quad 0
	.long V2P(boot_pd) + (PG_PRESENT)
	.long 0
.align 4096
boot_pd:
	.set i, 0
	.rept 512
		.long V2P(boot_pts) + i + (PG_READ_WRITE | PG_PRESENT)
		.long 0
		.set i, i + 0x1000
	.endr
boot_pts:
	.set i, 0
	.rept 512
		.rept 512
			.long i + (PG_READ_WRITE | PG_PRESENT)
			.long 0
			.set i, i + 0x1000
		.endr
	.endr

boot_gdt:
	.quad 0x0000000000000000 # null descriptor
	.quad 0x00CF9A000000FFFF # kernel code
	.quad 0x00CF92000000FFFF # kernel data
boot_gdtr:
	.short . - boot_gdt - 1
	.long V2P(boot_gdt)

.global g_ap_startup_done
g_ap_startup_done:
	.byte 0
.global g_ap_running_count
g_ap_running_count:
	.byte 0
.global g_ap_stack_loaded
g_ap_stack_loaded:
	.byte 0

.section .text

has_cpuid:
	pushfl
	pushfl
	xorl $0x00200000, (%esp)
	popfl
	pushfl
	popl %eax
	xorl (%esp), %eax
	popfl
	testl $0x00200000, %eax
	ret

has_pae:
	movl $0, %eax
	cpuid
	testl $(1 << 6), %edx
	ret

has_sse:
	movl $1, %eax
	cpuid
	testl $(1 << 25), %edx
	ret

check_requirements:
	call has_cpuid
	jz .exit
	call has_pae
	jz .exit
	call has_sse
	jz .exit
	ret
.exit:
	jmp system_halt

enable_sse:
	movl %cr0, %eax
	andw $0xFFFB, %ax
	orw $0x0002, %ax
	movl %eax, %cr0
	movl %cr4, %eax
	orw $0x0600, %ax
	movl %eax, %cr4
	ret

initialize_paging:
	# enable PAE
	movl %cr4, %ecx
	orl $(1 << 5), %ecx
	movl %ecx, %cr4

	# load page tables
	movl $V2P(boot_pdpt), %ecx
	movl %ecx, %cr3

	# enable paging
	movl %cr0, %ecx
	orl $(1 << 31), %ecx
	movl %ecx, %cr0

	ret

.global _start
.type _start, @function
_start:
	cli; cld

	# save bootloader magic and info
	movl %eax, V2P(bootloader_magic)
	movl %ebx, V2P(bootloader_info)

	# load boot stack
	movl $V2P(boot_stack_top), %esp

	# load boot GDT
	lgdt V2P(boot_gdtr)
	ljmpl $0x08, $V2P(gdt_flush)
gdt_flush:
	# set correct segment registers
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %ss
	movw %ax, %es

	# do processor initialization
	call check_requirements
	call enable_sse
	call initialize_paging

	# load higher half stack pointer
	movl $boot_stack_top, %esp

	# jump to higher half
	leal higher_half, %ecx
	jmp *%ecx

higher_half:
	# call global constuctors
	call _init

	# call to the kernel itself (clear ebp for stacktrace)
	xorl %ebp, %ebp

	subl $8, %esp
	pushl bootloader_info
	pushl bootloader_magic
	call kernel_main
	addl $16, %esp

	# call global destructors
	call _fini

system_halt:
	xchgw %bx, %bx
	cli
1:	hlt
	jmp 1b


#define AP_V2P(vaddr) ((vaddr) - ap_trampoline + 0xF000)

.section .ap_init, "ax"

.code16
.global ap_trampoline
ap_trampoline:
	jmp 1f

.align 8
ap_stack_ptr:
	.skip 4
ap_stack_loaded:
	.skip 1

1:	cli; cld
	ljmpl $0x00, $AP_V2P(ap_cs_clear)

ap_cs_clear:
	# load ap gdt and enter protected mode
	lgdt AP_V2P(ap_gdtr)
	movl %cr0, %eax
	orb $1, %al
	movl %eax, %cr0
	ljmpl $0x08, $AP_V2P(ap_protected_mode)

.code32
ap_protected_mode:
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %ss
	movw %ax, %es

	movl AP_V2P(ap_stack_ptr), %esp
	movb $1, AP_V2P(ap_stack_loaded)

	leal V2P(enable_sse),        %ecx; call *%ecx
	leal V2P(initialize_paging), %ecx; call *%ecx

	# load boot gdt and enter long mode
	lgdt V2P(boot_gdtr)
	ljmpl $0x08, $AP_V2P(ap_flush_gdt)

ap_flush_gdt:
	# move stack pointer to higher half
	movl %esp, %esp
	addl $KERNEL_OFFSET, %esp

	# jump to higher half
	leal ap_higher_half, %ecx
	jmp *%ecx

ap_higher_half:
	# clear rbp for stacktrace
	xorl %ebp, %ebp

1:	pause
	cmpb $0, g_ap_startup_done
	jz 1b

	lock incb g_ap_running_count

	call ap_main
	jmp system_halt

ap_gdt:
	.quad 0x0000000000000000 # null descriptor
	.quad 0x00CF9A000000FFFF # 32 bit code
	.quad 0x00CF92000000FFFF # 32 bit data
ap_gdtr:
	.short . - ap_gdt - 1
	.long ap_gdt
