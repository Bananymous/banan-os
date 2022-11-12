# Declare constants for the multiboot header
.set ALIGN,			1<<0					# align loaded modules on page boundaries
.set MEMINFO,		1<<1					# provide memory map
.set MB_FLAGS,		ALIGN | MEMINFO			# this is the Multiboot 'flag' field
.set MB_MAGIC,		0x1BADB002				# 'magic number' lets bootloader find the header
.set MB_CHECKSUM,	-(MB_MAGIC + MB_FLAGS)	#checksum of above, to prove we are multiboot

# Multiboot header
.section .multiboot.data, "aw"
	.align 4
	.long MB_MAGIC
	.long MB_FLAGS
	.long MB_CHECKSUM

 # Create stack
.section .bootstrap_stack, "aw", @nobits
	stack_bottom:
	.skip 16384		# 16 KiB
	stack_top:

# Preallocate pages
.section .bss, "aw", @nobits
	.align 4096
	boot_page_directory:
		.skip 4096
	boot_page_table1:
		.skip 4096

# Kernel entrypoint
.section .multiboot.text, "a"
.global _start
.type _start, @function
_start:
	# Physical address of boot_page_table1
	movl $(boot_page_table1 - 0xC0000000), %edi

	# First address to map is 0
	movl $0, %esi

	# Map 1023 pages, 1024th will be VGA memory
	movl $1023, %ecx


1:
	# Only map the kernel
	cmpl $_kernel_start, %esi
	jl 2f
	cmpl $(_kernel_end - 0xC0000000), %esi
	jge 3f

	# Map physical address as "present, writable"
	# Note that this maps .text and .rodata as writable. Mind security and map them as non-writable.
	movl %esi, %edx
	orl $0x003, %edx
	movl %edx, (%edi)

2:
	# Size of page is 4096 bytes
	addl $4096, %esi
	# Size of entry in boot_page_table1 is 4 bytes
	addl $4, %edi
	# Loop to next entry if we haven't finished
	loop 1b

3:
	# Map VGA memory to 0xC03FF000 as "present, writable"
	movl $(0x000B8000 | 0x003), boot_page_table1 - 0xC0000000 + 1023 * 4

	# The page table is used at both page directory entry 0 (virtually from 0x0
	# to 0x3FFFFF) (thus identity mapping the kernel) and page directory entry
	# 768 (virtually from 0xC0000000 to 0xC03FFFFF) (thus mapping it in the
	# higher half). The kernel is identity mapped because enabling paging does
	# not change the next instruction, which continues to be physical. The CPU
	# would instead page fault if there was no identity mapping.

	# Map the page table to virtual addresses 0x00000000 and 0xC0000000
	movl $(boot_page_table1 - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 0
	movl $(boot_page_table1 - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 768 * 4

	# Set cr3 to the address of the boot_page_directory
	movl $(boot_page_directory - 0xC0000000), %ecx
	movl %ecx, %cr3

	# Enable paging and the write-protect bit
	movl %cr0, %ecx
	orl $0x80010000, %ecx
	movl %ecx, %cr0

	# Jump to higher half with an absolute jump
	lea 4f, %ecx
	jmp *%ecx

.section .text

4:
	# Now paging is fully set up and enabled

	# Unmap the identity mapping since it is not unnecessary
	movl $0, boot_page_directory + 0

	# Reload crc3 to force a TLB flush for the changes to take effect
	movl %cr3, %ecx
	movl %ecx, %cr3

	# Setup stack
	mov $stack_top, %esp
 
	# Call into C code
	call kernel_main
 
	# Hang if kernel_main returns
	cli
1:	hlt
	jmp 1b
