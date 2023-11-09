# FIXME: don't assume 512 byte sectors
.set SECTOR_SIZE,		512

.set GPT_HEADER_ADDR,	free_memory_start
.set GPT_ENTRY_ADDR,	free_memory_start + SECTOR_SIZE



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
	jmp main

# al:		character to print
putc:
	mov $0x0E, %ah
	int $0x10
	ret

# ds:si:	null terminated string to print
puts:
	push %si
	push %ax

1:
	lodsb

	test %al, %al
	jz 2f

	call putc
	jmp 1b

2:
	mov $'\r', %al
	call putc
	mov $'\n', %al
	call putc

	pop %ax
	pop %si
	ret

# si:		ptr1
# di:		ptr2
# cx:		count
# return:	1 if equal, 0 otherwise
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
#
# bx:eax:	lba start
# cx:		lba count (has to less than 0x80)
# dl:		drive number
# ds:di:	physical address
#
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


main:
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
	movw $GPT_HEADER_ADDR, %di
	call read_from_disk

	# confirm header (starts with 'EFI PART')
	cmpl $0x20494645, (GPT_HEADER_ADDR + 0)
	jne .not_gpt_partition
	cmpl $0x54524150, (GPT_HEADER_ADDR + 4)
	jne .not_gpt_partition

	# eax := entry_count
	movl (GPT_HEADER_ADDR + 80), %eax
	test %eax, %eax
	jz .no_bios_boot_partition

	# edx:eax := eax * entry_size
	mull (GPT_HEADER_ADDR + 84)
	test %edx, %edx
	jnz .too_gpt_big_entries

	# sector count := (arr_size + SECTOR_SIZE - 1) / SECTOR_SIZE
	pushl %eax
	addl $(SECTOR_SIZE - 1), %eax
	movl $SECTOR_SIZE, %ecx
	divl %ecx
	movl %eax, %ecx
	popl %eax

	# start lba
	movl (GPT_HEADER_ADDR + 72), %eax
	movw (GPT_HEADER_ADDR + 76), %bx

	movb (boot_disk_number), %dl
	movw $GPT_ENTRY_ADDR, %di
	call read_from_disk

	# NOTE: 'only' 0xFFFF partitions supported
	movw (GPT_HEADER_ADDR + 80), %cx

.loop_entries:
	push %cx
	movw $16, %cx
	movw $bios_boot_guid, %si
	call memcmp
	pop %cx

	testb %al, %al
	jnz .bios_boot_found

	# add entry size to entry pointer
	addw (GPT_HEADER_ADDR + 84), %di

	loop .loop_entries
	jmp .no_bios_boot_partition

.bios_boot_found:
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
	jmp stage2_start

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
	.asciz "not gpt"

no_bios_boot_partition_msg:
	.asciz "no bios boot partition"

too_gpt_big_entries_msg:
	.asciz "too big GPT array"

boot_disk_number:
	.skip 1

disk_address_packet:
	.skip 16



#########################################
#
# STAGE 2 BOOTLOADER
#
#########################################

.section .stage2
stage2_start:
	# clear screen and enter 80x25 text mode
	movb $0x03, %al
	movb $0x00, %ah
	int $0x10

	# print hello message
	movw $hello_msg, %si
	call puts

1:
	jmp 1b

hello_msg:
	.asciz "This is banan-os bootloader"

stage2_end:
