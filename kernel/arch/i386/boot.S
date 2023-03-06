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
.global g_boot_stack_bottom
g_boot_stack_bottom:
	.skip 16384
.global g_boot_stack_top
g_boot_stack_top:

# 0 MiB -> 1 MiB: bootloader stuff
# 1 MiB -> 2 MiB: kernel
# 2 MiB -> 3 MiB: kmalloc
# 3 MiB -> 4 Mib: kmalloc_fixed
.align 32
boot_page_directory_pointer_table:
	.skip 4 * 8
.align 4096
boot_page_directory1:
	.skip 512 * 8

.global g_kernel_cmdline
g_kernel_cmdline:
	.skip 4096

.global g_multiboot_info
g_multiboot_info:
	.skip 4
.global g_multiboot_magic
g_multiboot_magic:
	.skip 4

.section .text

boot_gdt:
	.quad 0x0000000000000000 # null
	.quad 0x00CF9A000000FFFF # kernel code
	.quad 0x00CF92000000FFFF # kernel data
boot_gdtr:
	.short . - boot_gdt - 1
	.long boot_gdt

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
	# identity map first 6 MiB
	movl $(0x00000000           + 0x83), boot_page_directory1 + 0
	movl $(0x00200000           + 0x83), boot_page_directory1 + 8
	movl $(0x00400000           + 0x83), boot_page_directory1 + 16
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

	ret

initialize_gdt:
	lgdt boot_gdtr

	# flush gdt
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss
	jmp $0x08, $flush
flush:
	ret

.global _start
.type _start, @function
_start:
	# Initialize stack and multiboot info
	movl $g_boot_stack_top, %esp
	movl %eax, g_multiboot_magic
	movl %ebx, g_multiboot_info

	call copy_kernel_commandline
	call check_requirements
	call enable_sse

	call initialize_paging
	call initialize_gdt

	call _init

	# call to the kernel itself (clear ebp for stacktrace)
	xorl %ebp, %ebp
	call kernel_main

	call _fini

system_halt:
	xchgw %bx, %bx
	cli
1:	hlt
	jmp 1b
