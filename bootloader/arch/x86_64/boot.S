# FIXME: don't assume 512 byte sectors
.set SECTOR_SIZE_SHIFT,	9
.set SECTOR_SIZE,		1 << SECTOR_SIZE_SHIFT

.set SCREEN_WIDTH,	80
.set SCREEN_HEIGHT,	25

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

stage1_start:
	jmp stage1_main

# prints character to screen
# al:		ascii character to print
putc:
	pusha
	movb $0x0E, %ah
	movb $0x00, %bh
	int $0x10
	popa
	ret

# prints null terminated string to screen
# ds:si:	string address
puts:
	pushw %si
	pushw %bx
	pushw %ax

	movb $0x0E, %ah
	movb $0x00, %bh

.puts_loop:
	lodsb

	test %al, %al
	jz .puts_done

	int $0x10
	jmp .puts_loop

.puts_done:
	popw %ax
	popw %bx
	popw %si
	ret

# compares memory between addresses
# si:		ptr1
# di:		ptr2
# cx:		bytes count
# return:
#	al: 1 if equal, 0 otherwise
memcmp:
	pushw %si
	pushw %di

	cld
	repe cmpsb
	setzb %al

	popw %di
	popw %si
	ret


# read sectors from disk
# bx:eax:	lba start
# cx:		lba count (has to less than 0x80)
# dl:		drive number
# ds:di:	physical address
# returns only on success
read_from_disk:
	push %ax
	push %si

	# prepare disk read packet
	mov $disk_address_packet, %si
	movb $0x10,	0x00(%si)	# packet size
	movb $0x00,	0x01(%si)	# always 0
	movw %cx,	0x02(%si)	# lba count
	movw %di,	0x04(%si)	# offset
	movw %ds,	0x06(%si)	# segment
	movl %eax,	0x08(%si)	# 32 bit lower lba
	movw %bx,	0x0C(%si)	# 16 bit upper lba
	movw $0,	0x0E(%si)	# zero

	# issue read command
	clc
	mov $0x42, %ah
	int $0x13
	jc .read_failed

	pop %si
	pop %ax
	ret


# Validates that GPT header is valid and usable
# doesn't return on invalid header
validate_gpt_header:
	# confirm header (starts with 'EFI PART')
	cmpl $0x20494645, (gpt_header + 0)
	jne .not_gpt_partition
	cmpl $0x54524150, (gpt_header + 4)
	jne .not_gpt_partition
	ret


# Find partition entry with type guid
# si:	address of type guid
# dl:	drive number
# returns:
#   al: non-zero if found
#	di: address of partition entry
find_gpt_partition_entry:
	# eax := entry_count
	movl (gpt_header + 80), %eax
	test %eax, %eax
	jz .no_gpt_partition_found

	# edx:eax := eax * entry_size
	pushw %dx
	mull (gpt_header + 84)
	test %edx, %edx
	jnz .too_gpt_big_entries
	popw %dx

	# FIXME: read one entry array section at a time

	# sector count := (arr_size + SECTOR_SIZE - 1) / SECTOR_SIZE
	movl %eax, %ecx
	shrl $SECTOR_SIZE_SHIFT, %ecx

	# start lba
	movl (gpt_header + 72), %eax
	movw (gpt_header + 76), %bx

	movw $gpt_entry_data, %di

	call read_from_disk

	# NOTE: 'only' 0xFFFF partitions supported,
	#       although read will fail with more than 0x80
	movw (gpt_header + 80), %cx

.loop_gpt_entries:
	pushw %cx
	movw $16, %cx
	call memcmp
	popw %cx

	testb %al, %al
	jnz .gpt_partition_found

	# add entry size to entry pointer
	addw (gpt_header + 84), %di

	loop .loop_gpt_entries

.no_gpt_partition_found:
	movb $0, %al
	ret

.gpt_partition_found:
	movb $1, %al
	ret


stage1_main:
	# setup segments
	movw $0, %ax
	movw %ax, %ds
	movw %ax, %es

	# setup stack
	movw %ax, %ss
	movw $0x7C00, %sp

	# save boot disk number
	movb %dl, (boot_disk_number)

	# confirm that int 13h extensions are available
	clc
	movb $0x41, %ah
	movw $0x55AA, %bx
	movb (boot_disk_number), %dl
	int $0x13
	jc .no_int13h_ext

	# read gpt header
	movl $1, %eax
	movw $0, %bx
	movw $1, %cx
	movb (boot_disk_number), %dl
	movw $gpt_header, %di
	call read_from_disk

	call validate_gpt_header

	movw $bios_boot_guid, %si
	movb boot_disk_number, %dl
	call find_gpt_partition_entry
	testb %al, %al
	jz .no_bios_boot_partition

	# BIOS boot partiton found!

	# first lba
	movl 32(%di), %eax
	movw $0, %bx

	# count := last lba - first lba + 1
	movl 40(%di), %ecx
	subl %eax, %ecx
	addl $1, %ecx

	# calculate stage2 sector count
	movw $((stage2_end - stage2_start + SECTOR_SIZE - 1) / SECTOR_SIZE), %cx

	movb (boot_disk_number), %dl
	movw $stage2_start, %di

	call read_from_disk
	jmp stage2_main

print_and_halt:
	call puts
halt:
	hlt
	jmp halt

.no_int13h_ext:
	mov $no_int13_ext_msg, %si
	jmp print_and_halt

.read_failed:
	mov $read_failed_msg, %si
	jmp print_and_halt

.not_gpt_partition:
	mov $not_gpt_partition_msg, %si
	jmp print_and_halt

.no_bios_boot_partition:
	mov $no_bios_boot_partition_msg, %si
	jmp print_and_halt

.too_gpt_big_entries:
	mov $too_gpt_big_entries_msg, %si
	jmp print_and_halt


# 21686148-6449-6E6F-744E-656564454649
bios_boot_guid:
	.long 0x21686148		# little endian
	.word 0x6449			# little endian
	.word 0x6E6F			# little endian
	.word 0x4E74			# big endian
	.quad 0x494645646565	# big endian

no_int13_ext_msg:
	.asciz "no INT 13h ext"

read_failed_msg:
	.asciz "read error"

not_gpt_partition_msg:
	.asciz "not GPT"

no_bios_boot_partition_msg:
	.asciz "no bios boot partition"

too_gpt_big_entries_msg:
	.asciz "too big GPT array"

boot_disk_number:
	.skip 1


#########################################
#
# STAGE 2 BOOTLOADER
#
#########################################

.section .stage2

stage2_start:

# read a character from keyboard
# return:
#	al: ascii
#	ah: bios scan code
getc:
	movb $0x00, %ah
	int $0x16
	ret


# prints newline to screen
print_newline:
	pushw %ax
	movb $'\r', %al
	call putc
	movb $'\n', %al
	call putc
	pop %ax
	ret


# prints backspace to screen, can go back a line
print_backspace:
	pusha

	# get cursor position
	movb $0x03, %ah
	movb $0x00, %bh
	int $0x10

	# don't do anyting if on first row
	testb %dh, %dh
	jz .print_backspace_done

	# go one line up if on first column
	test %dl, %dl
	jz .print_backspace_go_line_up

	# otherwise decrease column
	decb %dl
	jmp .print_backspace_do_print

.print_backspace_go_line_up:
	# decrease row and set column to the last one
	decb %dh
	movb $(SCREEN_WIDTH - 1), %dl

.print_backspace_do_print:
	# set cursor position
	movb $0x02, %ah
	int $0x10

	# print 'empty' character (space)
	mov $' ', %al
	call putc

	# set cursor position
	movb $0x02, %ah
	int $0x10

.print_backspace_done:
	popa
	ret


# print number to screen
# ax:	number to print
# bx:	number base
# cx:	min width (zero pads if shorter)
printnum:
	pusha
	pushw %cx

	movw $printnum_buffer, %si
	xorw %cx, %cx

.printnum_fill_loop:
	# fill buffer with all remainders ax % bx
	xorw %dx, %dx
	divw %bx
	movb %dl, (%si)
	incw %si
	incw %cx
	testw %ax, %ax
	jnz .printnum_fill_loop

	# check if zero pad is required
	popw %dx
	cmpw %cx, %dx
	jbe .printnum_print_loop

	xchgw %dx, %cx
	subw %dx, %cx
	movb $'0', %al
.printnum_pad_zeroes:
	call putc
	loop .printnum_pad_zeroes

	movw %dx, %cx

.printnum_print_loop:
	decw %si
	movb (%si), %al
	cmpb $10, %al
	jae 1f
	addb $'0', %al
	jmp 2f
1:	addb $('a' - 10), %al
2:	call putc
	loop .printnum_print_loop

	popa
	ret


# test if character is printable ascii
# al:	character to test
# sets ZF if is printable
isprint:
	cmpb $0x20, %al
	jb .isprint_done
	cmpb $0x7E, %al
	ja .isprint_done
	cmpb %al, %al
.isprint_done:
	ret


# fills memory map data structure
# doesn't return on error
get_memory_map:
	pusha

	movl $0, (memory_map_entry_count)

	movl $0x0000E820, %eax
	movl $0x534D4150, %edx
	xorl %ebx, %ebx
	movl $20, %ecx
	movw $memory_map_entries, %di

	clc
	int $0x15
	# If first call returs with CF set, the call failed
	jc .get_memory_map_error

.get_memory_map_rest:
	cmpl $0x534D4150, %eax
	jne .get_memory_map_error

	# FIXME: don't assume BIOS to always return 20 bytes
	cmpl $20, %ecx
	jne .get_memory_map_error

	# increment entry count
	incl (memory_map_entry_count)

	# increment entry pointer
	addw %cx, %di

	# BIOS can indicate end of list by 0 in ebx
	testl %ebx, %ebx
	jz .get_memory_map_done

	movl $0x0000E820, %eax
	movl $0x534D4150, %edx

	clc
	int $0x15
	# BIOS can indicate end of list by setting CF
	jnc .get_memory_map_rest

.get_memory_map_done:
	popa
	ret

.get_memory_map_error:
	movw $memory_map_error_msg, %si
	jmp print_and_halt


# fills command line buffer
get_command_line:
	pusha

	movw $command_line_enter_msg, %si
	call puts

	movw $command_line_buffer, %di

.get_command_line_loop:
	call getc

	cmpb $'\b', %al
	je .get_command_line_backspace

	# Not sure if some BIOSes return '\n' as enter, but check it just in case
	cmpb $'\r', %al
	je .get_command_line_done
	cmpb $'\n', %al
	je .get_command_line_done

	call isprint
	jnz .get_command_line_loop

	# put byte to buffer
	movb %al, (%di)
	incw %di

	# print byte
	call putc

	jmp .get_command_line_loop

.get_command_line_backspace:
	# don't do anything if at the beginning
	cmpw $command_line_buffer, %di
	je .get_command_line_loop

	# decrement buffer pointer
	decw %di

	# erase byte in display
	call print_backspace

	jmp .get_command_line_loop

.get_command_line_done:
	# null terminate command line
	movb $0, (%di)

	call print_newline

	popa
	ret


stage2_main:
	# clear screen and enter 80x25 text mode
	movb $0x03, %al
	movb $0x00, %ah
	int $0x10

	# print hello message
	movw $hello_msg, %si
	call puts
	call print_newline

	call get_memory_map
	call get_command_line

	call print_newline

	movw $start_kernel_load_msg, %si
	call puts
	call print_newline

	movw $command_line_msg, %si
	call puts
	movw $command_line_buffer, %si
	call puts
	call print_newline

	movw $memory_map_msg, %si
	call puts
	call print_newline

	movl (memory_map_entry_count), %edx
	movw $memory_map_entries, %si

	movw $16, %bx
	movw $4, %cx

.loop_memory_map:
	movb $' ', %al
	call putc
	call putc
	call putc
	call putc

	movw 0x06(%si), %ax
	call printnum
	movw 0x04(%si), %ax
	call printnum
	movw 0x02(%si), %ax
	call printnum
	movw 0x00(%si), %ax
	call printnum

	movb $',', %al
	call putc
	movb $' ', %al
	call putc

	movw 0x0E(%si), %ax
	call printnum
	movw 0x0C(%si), %ax
	call printnum
	movw 0x0A(%si), %ax
	call printnum
	movw 0x08(%si), %ax
	call printnum

	movb $',', %al
	call putc
	movb $' ', %al
	call putc

	movw 0x12(%si), %ax
	call printnum
	movw 0x10(%si), %ax
	call printnum

	call print_newline

	addw $20, %si

	decl %edx
	jnz .loop_memory_map


	jmp halt


hello_msg:
	.asciz "This is banan-os bootloader"

command_line_msg:
command_line_enter_msg:
	.asciz "cmdline: "

memory_map_msg:
	.asciz "memmap:"

memory_map_error_msg:
	.asciz "Failed to get memory map"

start_kernel_load_msg:
	.asciz "Starting to load kernel"

stage2_end:


.section .bss

.align SECTOR_SIZE
gpt_header:
	.skip SECTOR_SIZE
gpt_entry_data:
	.skip SECTOR_SIZE

disk_address_packet:
	.skip 16

printnum_buffer:
	.skip 10

memory_map_entry_count:
	.skip 4
# 100 entries should be enough...
memory_map_entries:
	.skip 20 * 100

# 100 character command line
command_line_buffer:
	.skip 100
