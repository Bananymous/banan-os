# Declare constants for the multiboot header
.set ALIGN,			1<<0						# align loaded modules on page boundaries
.set MEMINFO,		1<<1						# provide memory map
.set VIDEOINFO,		1<<2						# provide video info
.set MB_FLAGS,		ALIGN | MEMINFO | VIDEOINFO # this is the Multiboot 'flag' field
.set MB_MAGIC,		0x1BADB002					# 'magic number' lets bootloader find the header
.set MB_CHECKSUM,	-(MB_MAGIC + MB_FLAGS)		#checksum of above, to prove we are multiboot

# Multiboot header
.section .multiboot
	.align 4
	.long MB_MAGIC
	.long MB_FLAGS
	.long MB_CHECKSUM
	.skip 20

	.long 0
	.long 800
	.long 600
	.long 32

 # Create stack
.section .bss
	stack_bottom:
	.skip 16384		# 16 KiB
	stack_top:

.section .text
.global _start
.type _start, @function
_start:
	movl $stack_top, %esp

	pushl %eax
	pushl %ebx

	call _init

	call kernel_main

	cli
1:
	hlt
	jmp 1b

.size _start, . - _start
