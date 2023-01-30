# Declare constants for the multiboot header
.set ALIGN,			1<<0						# align loaded modules on page boundaries
.set MEMINFO,		1<<1						# provide memory map
.set VIDEOINFO,		1<<2						# provide video info
.set MB_FLAGS,		ALIGN | MEMINFO | VIDEOINFO # this is the Multiboot 'flag' field
.set MB_MAGIC,		0x1BADB002					# 'magic number' lets bootloader find the header
.set MB_CHECKSUM,	-(MB_MAGIC + MB_FLAGS)		#checksum of above, to prove we are multiboot

.set PG_PRESENT,	1<<0
.set PG_READ_WRITE,	1<<1
.set PG_PAGE_SIZE,	1<<7

# GDT access byte
.set GDT_A_A,	1<<40
.set GDT_A_RW,	1<<41
.set GDT_A_DC,	1<<42
.set GDT_A_E,	1<<43
.set GDT_A_S,	1<<44
.set GDT_A_DPL,	1<<45
.set GDT_A_P,	1<<47
# GDT flags
.set GDT_F_L,	1<<53
.set GDT_F_DB,	1<<54
.set GDT_F_G,	1<<55

.code32

# Multiboot header
.section .multiboot, "aw"
	.align 4
	.long MB_MAGIC
	.long MB_FLAGS
	.long MB_CHECKSUM
	.skip 20

	.long 0
	.long 800
	.long 600
	.long 32

.section .bss, "aw", @nobits
 	# Create stack
	stack_bottom:
		.skip 16384
	stack_top:
	
	.global g_kernel_cmdline
	g_kernel_cmdline:
		.skip 4096

	# Reserve memory for paging structures,
	# we will identity map first 4 MiB

	# 0 MiB -> 1 MiB: bootloader stuff
	# 1 MiB -> 2 MiB: kernel
	# 2 MiB -> 3 MiB: kmalloc
	# 3 MiB -> 4 MiB: kmalloc_eternal
	.align 4096
	boot_pml4:
		.skip 512 * 8
	boot_pdpt1:
		.skip 512 * 8
	boot_pd1:
		.skip 512 * 8

.section .text, "a"

.global g_multiboot_info
g_multiboot_info:
	.skip 8
.global g_multiboot_magic
g_multiboot_magic:
	.skip 8

boot_gdt:
	.quad 0																											# null descriptor
	.quad (GDT_F_G | GDT_F_L ) | (GDT_A_P | (0 * GDT_A_DPL) | GDT_A_S | GDT_A_E | GDT_A_RW) | (0xF << 48 | 0xFFFF)	# kernel code
boot_gdtr:
	.short . - boot_gdt - 1
	.quad boot_gdt

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

is_64_bit:
	movl $0x80000000, %eax
	cpuid
	cmpl $0x80000001, %eax
	jl .no_extension
	movl $0x80000001, %eax
	cpuid
	testl $(1 << 29), %edx
	ret
.no_extension:
	cmpl %eax, %eax
	ret

check_requirements:
	call has_cpuid
	jz .exit
	call is_64_bit
	jz .exit
	ret
.exit:
	jmp system_halt

copy_kernel_commandline:
	pushl %esi
	pushl %edi
	movl g_multiboot_info, %esi
	addl $16, %esi
	movl (%esi), %esi
	movl $1024, %ecx
	movl $g_kernel_cmdline, %edi
	rep movsl
	popl %edi
	popl %esi
	ret

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
	# identity map first 4 MiB
	movl $(0x00000000 + PG_PAGE_SIZE + PG_READ_WRITE + PG_PRESENT), boot_pd1 + 0
	movl $(0x00200000 + PG_PAGE_SIZE + PG_READ_WRITE + PG_PRESENT), boot_pd1 + 8

	# set pdpte1 and pml4e1
	movl $(boot_pd1   + PG_READ_WRITE + PG_PRESENT), boot_pdpt1	
	movl $(boot_pdpt1 + PG_READ_WRITE + PG_PRESENT), boot_pml4

	# enable PAE
	movl %cr4, %ecx
	orl $0x20, %ecx
	movl %ecx, %cr4

	# set long mode enable bit
	movl $0x100, %eax
	movl $0x000, %edx
	movl $0xC0000080, %ecx
	wrmsr

	# set address of paging structures
	movl $boot_pml4, %ecx
	movl %ecx, %cr3

	# enable paging
	movl %cr0, %ecx
	orl $0x80000000, %ecx
	movl %ecx, %cr0

	ret

.global _start
.type _start, @function
_start:
	# Initialize stack and multiboot info
	movl $stack_top, %esp
	movl %eax, g_multiboot_magic
	movl %ebx, g_multiboot_info

	call copy_kernel_commandline
	call check_requirements
	call enable_sse

	call initialize_paging

	# flush gdt and jump to 64 bit
	lgdt boot_gdtr
	ljmpl $0x08, $long_mode

.code64
long_mode:
	# clear segment registers
	movw $0x00, %ax
	movw %ax, %ss
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs

	# call global constuctors
	call _init

	# call to the kernel itself (clear ebp for stacktrace)
	xorq %rbp, %rbp
	call kernel_main

	# call global destructors
	call _fini

system_halt:
	xchgw %bx, %bx
	cli
1:	hlt
	jmp 1b
