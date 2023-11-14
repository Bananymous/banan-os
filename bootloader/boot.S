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
	# setup segments
	movw $0, %ax
	movw %ax, %ds
	movw %ax, %es

	# setup stack
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

	call get_memory_map
	call read_user_command_line

	call print_newline

	movw $start_kernel_load_msg, %si
	call puts; call print_newline

	call print_memory_map

	call find_root_disk
	call find_root_partition

	call print_root_partition_info

	jmp halt

hello_msg:
	.asciz "This is banan-os bootloader"

start_kernel_load_msg:
	.asciz "Starting to load kernel"
