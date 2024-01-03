# FIXME: don't assume 512 byte sectors
.set SECTOR_SIZE_SHIFT,	9
.set SECTOR_SIZE,		1 << SECTOR_SIZE_SHIFT

.code16

.section .stage1

.global stage2_start
.global stage2_end

# check that drive has int13 ext
#	dl: drive number
# returns only if drive does have the extension
drive_has_int13_ext:
	pusha

	movb $0x41, %ah
	movw $0x55AA, %bx
	int $0x13
	jc .drive_has_int13_ext_no_int13_ext

	popa
	ret

 .drive_has_int13_ext_no_int13_ext:
	mov $no_int13_ext_msg, %si
	jmp print_and_halt


# read sectors from disk
# bx:eax:	lba start
# cx:		lba count (has to less than 0x80)
# dl:		drive number
# ds:di:	physical address
# returns only on success
.global read_from_disk
read_from_disk:
	pusha

	call drive_has_int13_ext

	# prepare disk read packet
	movw $disk_address_packet, %si
	movb $0x10,	0x00(%si)	# packet size
	movb $0x00,	0x01(%si)	# always 0
	movw %cx,	0x02(%si)	# lba count
	movw %di,	0x04(%si)	# offset
	movw %ds,	0x06(%si)	# segment
	movl %eax,	0x08(%si)	# 32 bit lower lba
	movw %bx,	0x0C(%si)	# 16 bit upper lba
	movw $0,	0x0E(%si)	# zero

	# issue read command
	mov $0x42, %ah
	int $0x13
	jc .read_from_disk_failed

	popa
	ret

 .read_from_disk_failed:
	mov $read_from_disk_msg, %si
	jmp print_and_halt


# Reads GPT header into gpt_header buffer
#	dl: drive number
# return:
#	ax: 1 if has GPT header, 0 otherwise
.global read_gpt_header
read_gpt_header:
	pushw %bx
	pushw %cx
	pushw %di

	xorw %bx, %bx
	movl $1, %eax
	movw $1, %cx
	movw $gpt_header, %di
	call read_from_disk

	xorw %bx, %bx
	movw $1, %ax

	# check if header starts with 'EFI PART'
	cmpl $0x20494645, (gpt_header + 0)
	cmovnew %bx, %ax
	cmpl $0x54524150, (gpt_header + 4)
	cmovnew %bx, %ax

	popw %di
	popw %cx
	popw %bx
	ret


# Find bios boot partition from boot drive
# returns:
#	bx:eax:	first lba
#	cx:		sector count
find_stage2_partition:
	# read boot disk GPT header
	movb (boot_disk_number), %dl
	call read_gpt_header

	testb %al, %al
	jz .find_stage2_partition_not_gpt

	# eax := entry_count
	movl (gpt_header + 80), %eax
	test %eax, %eax
	jz .find_stage2_partition_not_found

	# edx:eax := eax * entry_size
	mull (gpt_header + 84)
	test %edx, %edx
	jnz .find_stage2_partition_too_big_entries

	# FIXME: read one entry array section at a time

	# sector count := (arr_size + SECTOR_SIZE - 1) / SECTOR_SIZE
	movl %eax, %ecx
	shrl $SECTOR_SIZE_SHIFT, %ecx

	# start lba
	movl (gpt_header + 72), %eax
	movw (gpt_header + 76), %bx

	movw $gpt_entry_data, %di
	movw $bios_boot_guid, %si
	movb (boot_disk_number), %dl

	call read_from_disk

	# NOTE: 'only' 0xFFFF partitions supported,
	#       although read will fail with more than 0x80
	movw (gpt_header + 80), %cx

 .find_stage2_partition_loop_gpt_entries:
	pushw %cx
	movw $16, %cx
	call memcmp
	popw %cx

	testb %al, %al
	jnz .find_stage2_partition_found

	# add entry size to entry pointer
	addw (gpt_header + 84), %di

	loop .find_stage2_partition_loop_gpt_entries

	# fall through to not found case
 .find_stage2_partition_not_found:
	movw $no_bios_boot_partition_msg, %si
	jmp print_and_halt

 .find_stage2_partition_not_gpt:
	movw $not_gpt_partition_msg, %si
	jmp print_and_halt

 .find_stage2_partition_too_big_entries:
	movw $too_gpt_big_entries_msg, %si
	jmp print_and_halt

 .find_stage2_partition_found:
	# first lba
	movl 32(%di), %eax
	movw 36(%di), %bx

	# count := last lba - first lba + 1
	movl 40(%di), %ecx
	subl %eax, %ecx
	incl %ecx

	ret

# reads stage2 into memory
#	dl: boot drive number
# returns only on success
.global read_stage2_into_memory
read_stage2_into_memory:
	movb %dl, (boot_disk_number)

	# push stage2 sector count
	movl $stage2_end, %eax
	subl $stage2_start, %eax
	addl $(SECTOR_SIZE - 1), %eax
	movl $SECTOR_SIZE, %ecx
	xorl %edx, %edx
	divl %ecx
	pushl %eax

	call find_stage2_partition

	movb (boot_disk_number), %dl
	popl %ecx # FIXME: validate that partition has enough sectors
	movw $stage2_start, %di
	call read_from_disk

	ret

# 21686148-6449-6E6F-744E-656564454649
.align 4
bios_boot_guid:
	.long 0x21686148		# little endian
	.word 0x6449			# little endian
	.word 0x6E6F			# little endian
	.word 0x4E74			# big endian
	.quad 0x494645646565	# big endian

boot_disk_number:
	.skip 1

read_from_disk_msg:
	.asciz "read error"

no_int13_ext_msg:
	.asciz "no INT13 ext"

no_bios_boot_partition_msg:
	.asciz "no bios boot"

too_gpt_big_entries_msg:
	.asciz "too big GPT array"

not_gpt_partition_msg:
	.asciz "not GPT"


.section .stage2

# check if drive exists
#	dl: drive number
# return:
#	al: 1 if disk is usable, 0 otherwise
drive_exists:
	pusha

	movb $0x48, %ah
	movw $disk_drive_parameters, %si
	movw $0x1A, (disk_drive_parameters) # set buffer size

	int $0x13
	jc .drive_exists_nope

	popa
	movb $1, %al
	ret

 .drive_exists_nope:
	popa
	movb $0, %al
	ret

# find root disk and populate root_disk_drive_number field
# NO REGISTERS SAVED
.global find_root_disk
find_root_disk:
	movb $0x80, %dl

 .find_root_disk_loop:
	call drive_exists
	testb %al, %al
	jz .find_root_disk_not_found

	# read GPT header
	xorw %bx, %bx
	movl $1, %eax
	movw $1, %cx
	movw $gpt_header, %di
	call read_from_disk

	# confirm header (starts with 'EFI PART')
	cmpl $0x20494645, (gpt_header + 0)
	jne .find_root_disk_next_disk
	cmpl $0x54524150, (gpt_header + 4)
	jne .find_root_disk_next_disk

	# compare disk GUID
	movw $root_disk_guid, %si
	movw $(gpt_header + 56), %di
	movw $16, %cx
	call memcmp
	testb %al, %al
	jz .find_root_disk_next_disk

	movw $root_disk_found_msg, %si
	call puts; call print_newline

	movb %dl, (root_disk_drive_number)
	ret

 .find_root_disk_next_disk:
	incb %dl
	jmp .find_root_disk_loop

 .find_root_disk_not_found:
	movw $root_disk_not_found_msg, %si
	jmp print_and_halt


# finds root partition from root disk
# fills root_partition_entry data structure
# NOTE: assumes GPT header is in `gpt_header`
# NO REGISTERS SAVED
# return:
#	dl:		drive number
#	ecx:	sector count (capped at 0xFFFFFFFF)
#	bx:eax:	first sector
.global find_root_partition
find_root_partition:
	pushl %ebp
	movl %esp, %ebp
	subl $16, %esp

	# esp + 0: 8 byte entry array lba
	movl (gpt_header + 72), %eax
	movl %eax, 0(%esp)
	movl (gpt_header + 76), %eax
	movl %eax, 4(%esp)
	# FIXME: check that bits 48-63 are zero

	# esp + 8: 4 byte entries per sector
	xorl %edx, %edx
	movl $SECTOR_SIZE, %eax
	divl (gpt_header + 84)
	movl %eax, 8(%esp)

	# esp + 12: 4 byte entries remaining
	movl (gpt_header + 80), %eax
	testl %eax, %eax
	jz .find_root_partition_not_found
	movl %eax, 12(%esp)

 .find_root_partition_read_entry_section:
	movl 0(%esp), %eax
	movl 4(%esp), %ebx
	movw $1, %cx
	movb (root_disk_drive_number), %dl
	movw $sector_buffer, %di
	call read_from_disk

	# ecx: min(entries per section, entries remaining)
	movl 8(%esp), %ecx
	cmpl 12(%esp), %ecx
	jae .find_root_partition_got_entry_count
	movl 12(%esp), %ecx

 .find_root_partition_got_entry_count:
	# update entries remaining
	subl %ecx, 12(%esp)

	# si: entry pointer
	movw $sector_buffer, %si

 .find_root_partition_loop_entries:
	# temporarily save cx in dx
	movw %cx, %dx

	# check that entry is used
	movw $16, %cx
	movw $zero_guid, %di
	call memcmp
	test %al, %al
	jnz .find_root_partition_next_entry

	# compare entry guid to root guid
	movw $16, %cx
	addw $16, %si
	movw $root_partition_guid, %di
	call memcmp
	subw $16, %si

	testb %al, %al
	jnz .find_root_partition_found

 .find_root_partition_next_entry:

	# restore cx
	movw %dx, %cx

	# entry pointer += entry size
	addw (gpt_header + 84), %si
	loop .find_root_partition_loop_entries

	# entry not found in this sector

	# increment 8 byte entry array lba
	incl 0(%esp)
	adcl $0, 4(%esp)

	# loop to read next section if entries remaining
	cmpl $0, 12(%esp)
	jnz .find_root_partition_read_entry_section

 .find_root_partition_not_found:
	movw $root_partition_not_found_msg, %si
	jmp print_and_halt

 .find_root_partition_found:
	# copy entry to buffer
	movw $root_partition_entry, %di
	movw $128, %cx
	rep movsb

	movw $root_partition_found_msg, %si
	call puts; call print_newline

	# ebx:eax := last lba
	movl (root_partition_entry + 44), %ebx
	movl (root_partition_entry + 40), %eax

	# ebx:eax -= first lba - 1
	subl (root_partition_entry + 36), %ebx
	movl (root_partition_entry + 32), %ecx
	decl %ecx
	subl %ecx, %eax
	sbbl $0, %ebx
 	
	# ecx: min(partition count, 0xFFFFFFFF)
	movl $0xFFFFFFFF, %edx
	movl %eax, %ecx
	testl %ebx, %ebx
	cmovnzl %edx, %ecx

	# ebx:eax := first lba
	# FIXME: confirm ebx bits 16:31 are zero
	movl (root_partition_entry + 36), %ebx
	movl (root_partition_entry + 32), %eax

	movb (root_disk_drive_number), %dl

	leavel
	ret


# print information about root partition
.global print_root_partition_info
print_root_partition_info:
	pushw %ax
	pushw %bx
	pushw %cx
	pushw %si

	movw $root_partition_info_start_msg, %si
	call puts;

	movw $16, %bx
	movw $2,  %cx
	movw (root_partition_entry + 38), %ax; call print_number
	movw (root_partition_entry + 36), %ax; call print_number
	movw (root_partition_entry + 34), %ax; call print_number
	movw (root_partition_entry + 32), %ax; call print_number

	movb $'-', %al; call putc
	movb $'>', %al; call putc

	movw (root_partition_entry + 46), %ax; call print_number
	movw (root_partition_entry + 44), %ax; call print_number
	movw (root_partition_entry + 42), %ax; call print_number
	movw (root_partition_entry + 40), %ax; call print_number

	call print_newline

	popw %si
	popw %cx
	popw %bx
	popw %ax
	ret


# These will be patched during bootloader installation
root_disk_guid:
	.ascii "root disk guid  "
root_partition_guid:
	.ascii "root part guid  "
zero_guid:
	.skip 16, 0

root_disk_found_msg:
	.asciz "Root disk found!"
root_disk_not_found_msg:
	.asciz "Root disk not found"

root_partition_found_msg:
	.asciz "Root partition found!"
root_partition_not_found_msg:
	.asciz "Root partition not found"

root_partition_info_start_msg:
	.asciz "Root partition: "

.section .bss

.align SECTOR_SIZE
gpt_header:
	.skip SECTOR_SIZE
gpt_entry_data:
	.skip SECTOR_SIZE
sector_buffer:
	.skip SECTOR_SIZE

disk_address_packet:
	.skip 16

disk_drive_parameters:
	.skip 0x1A
	.skip 2 # padding

root_disk_drive_number:
	.skip 1
	.skip 3 # padding

root_partition_entry:
	.skip 128
