.include "common.S"

.code16

#########################################
#
# STAGE 1 BOOTLOADER
#
# its sole purpose is to load stage2 from
# bios boot partition
#
#########################################

.section .stage1

.global stage1_main
stage1_main:
	# setup segments and stack
	xorw %ax, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movl $0x7C00, %esp

	# save boot disk number
	call read_stage2_into_memory

	jmp stage2_main

.global print_and_halt
print_and_halt:
	call puts
halt:
	hlt
	jmp halt


#########################################
#
# STAGE 2 BOOTLOADER
#
#########################################

.section .stage2

stage2_main:
	# clear screen and enter 80x25 text mode
	movb $0x03, %al
	movb $0x00, %ah
	int $0x10

	# print hello message
	movw $hello_msg, %si
	call puts; call print_newline

	lgdt gdtr

	call enter_unreal_mode
	movw $unreal_enter_msg, %si
	call puts; call print_newline

	call enable_a20

	call get_memory_map

	call print_newline
	call read_user_command_line

	call print_newline

	movw $start_kernel_load_msg, %si
	call puts; call print_newline

	call print_memory_map

	call find_root_disk
	call find_root_partition

	call print_root_partition_info
	call print_newline

	call has_ext2_filesystem
	testb %al, %al
	jz print_and_halt

	call ext2_find_kernel
	movl $ext2_inode_read_bytes, %esi

	call elf_read_kernel_to_memory

	call vesa_set_video_mode

	cli

	# kernel entry point
	movl %eax, %ecx

	# setup kernel parameters
	movl $0xD3C60CFF, %eax
	movl $banan_boot_info, %ebx

	# setup protected mode
	movl %cr0, %edx
	orb $1, %dl
	movl %edx, %cr0

	# jump to protected mode
	ljmpl $GDT_CODE32, $protected_mode

.code32
protected_mode:
	# setup protected mode segments
	movw $GDT_DATA32, %dx
	movw %dx, %ds
	movw %dx, %es
	movw %dx, %fs
	movw %dx, %gs
	movw %dx, %ss

	# jump to kernel entry
	jmp *%ecx


.code16
enter_unreal_mode:
	cli
	pushw %ds

	movl %cr0, %eax
	orb $1, %al
	movl %eax, %cr0
	ljmpl $GDT_CODE16, $.enter_unreal_mode_pmode

 .enter_unreal_mode_pmode:
	movw $GDT_DATA32, %bx
	movw %bx, %ds

	andb $0xFE, %al
	movl %eax, %cr0
	ljmpl $0x00, $.enter_unreal_mode_unreal

 .enter_unreal_mode_unreal:
	popw %ds
	sti

	ret

.section .data

hello_msg:
	.asciz "This is banan-os bootloader"

unreal_enter_msg:
	.asciz "Entered unreal mode"

start_kernel_load_msg:
	.asciz "Starting to load kernel"

gdt:
	.quad 0x0000000000000000
	.quad 0x008F9A000000FFFF # 16-bit code
	.quad 0x00CF92000000FFFF # 32-bit data
	.quad 0x00CF9A000000FFFF # 32-bit code
gdtr:
	.short . - gdt - 1
	.long gdt

banan_boot_info:
	boot_command_line:
		.long command_line
	boot_framebuffer:
		.long framebuffer
	boot_memory_map:
		.long memory_map
