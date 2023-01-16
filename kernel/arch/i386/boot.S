# Declare constants for the multiboot header
.set ALIGN,			1<<0						# align loaded modules on page boundaries
.set MEMINFO,		1<<1						# provide memory map
.set VIDEOINFO,		1<<2						# provide video info
.set MB_FLAGS,		ALIGN | MEMINFO | VIDEOINFO # this is the Multiboot 'flag' field
.set MB_MAGIC,		0x1BADB002					# 'magic number' lets bootloader find the header
.set MB_CHECKSUM,	-(MB_MAGIC + MB_FLAGS)		#checksum of above, to prove we are multiboot

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

	# 0 MiB -> 2 MiB: kernel
	# 2 MiB -> 3 MiB: kmalloc
	# 3 MiB -> 4 Mib: kmalloc_eternal
	.align 32
	boot_page_directory_pointer_table:
		.skip 4 * 8
	.align 4096
	boot_page_directory1:
		.skip 512 * 8

.section .text, "a"

.global g_multiboot_info
g_multiboot_info:
	.skip 4
.global g_multiboot_magic
g_multiboot_magic:
	.skip 4

has_cpuid:
	pushfl
	pushfl
	xorl $0x00200000, (%esp)
	popfl
	pushfl
	popl %eax
	xorl (%esp), %eax
	popfl
	andl $0x00200000, %eax
	ret

has_pae:
	call has_cpuid
	cmpl $0, %eax
	jz .exit
	movl $0, %eax
	cpuid
	movl %edx, %eax
	andl $0x40, %eax # PAE is bit 6 in edx
.exit:
	ret

.global _start
.type _start, @function
_start:
	# Initialize stack and multiboot info
	movl $stack_top, %esp
	movl %eax, g_multiboot_magic
	movl %ebx, g_multiboot_info

	# Copy kernel command line to known location
	movl %ebx, %esi
	addl $16, %esi
	movl (%esi), %esi
	movl $1024, %ecx
	movl $g_kernel_cmdline, %edi
	rep movsl

	call has_pae
	cmpl $0, %eax
	jz system_halt

	# identity map first 4 MiB
	movl $(0x00000000           + 0x83), boot_page_directory1 + 0
	movl $(0x00200000           + 0x83), boot_page_directory1 + 8
	movl $(boot_page_directory1 + 0x01), boot_page_directory_pointer_table

	# enable PAE
	movl %cr4, %ecx
	orl $0x20, %ecx
	movl %ecx, %cr4

	# set address of paging structures
	movl $boot_page_directory_pointer_table, %ecx
	movl %ecx, %cr3

	# enable paging
	movl %cr0, %ecx
	orl $0x80000000, %ecx
	movl %ecx, %cr0

	# call global constuctors
	call _init

	# call to the kernel itself
	xorl %ebp, %ebp
	call kernel_main

system_halt:
	xchgw %bx, %bx
	cli
1:	hlt
	jmp 1b
