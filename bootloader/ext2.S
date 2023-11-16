# FIXME: don't assume 512 byte sectors
.set SECTOR_SHIFT,	9
.set SECTOR_SIZE,	1 << SECTOR_SHIFT

# FIXME: don't assume 1024 byte blocks
.set EXT2_BLOCK_SHIFT,		10
.set EXT2_BLOCK_SIZE,		1 << EXT2_BLOCK_SHIFT
.set EXT2_SUPERBLOCK_SIZE,	264
.set EXT2_BGD_SHIFT,		5
.set EXT2_BGD_SIZE,			1 << EXT2_BGD_SHIFT
.set EXT2_INODE_SIZE_MAX,	256
.set EXT2_ROOT_INO,			2
.set EXT2_GOOD_OLD_REV,		0

# inode types
.set EXT2_S_IMASK,			0xF000
.set EXT2_S_IFDIR,			0x4000
.set EXT2_S_IFREG,			0x8000

# superblock offsets
.set s_log_block_size,		24
.set s_inodes_per_group,	40
.set s_magic,				56
.set s_rev_level,			76
.set s_inode_size,			88

# block group descriptor offsets
.set bg_inode_table,		8

# inode offsets
.set i_mode,				0
.set i_size,				4
.set i_block,				40


.code16
.section .stage2

# checks whether partition contains ext2 filesystem.
# fills ext2_superblock_buffer
#	dl:		drive number
#	ecx:	sector count
#	bx:eax:	first sector
# return:
#	al: 1 if is ext2, 0 otherwise
#	si: error message on error
.global has_ext2_filesystem
has_ext2_filesystem:
	pushl %ecx
	pushw %bx
	pushw %di

	# fill ext2_partition_first_sector
	movw $0,   (ext2_partition_first_sector + 6)
	movw %bx,  (ext2_partition_first_sector + 4)
	movl %eax, (ext2_partition_first_sector + 0)

	# fill ext2_drive_number
	movb %dl, (ext2_drive_number)

	cmpl $3, %ecx
	jb .has_ext2_filesystem_does_not_fit

	# one sector
	movw $1, %cx

	# from byte offset 1024
	addl $(1024 / SECTOR_SIZE), %eax
	jnc .has_ext2_filesystem_no_overflow
	incw %bx
 .has_ext2_filesystem_no_overflow:

	# into sector buffer
	movw $ext2_block_buffer, %di

	call read_from_disk

	# copy superblock to its buffer
	movw $ext2_block_buffer, %si
	movw $ext2_superblock_buffer, %di
	movw $EXT2_SUPERBLOCK_SIZE, %cx
	rep movsb

	# verify magic
	cmpw $0xEF53, (ext2_superblock_buffer + s_magic)
	jne .has_ext2_filesystem_invalid_magic

	# verify block size
	  # verify shift fits in one byte
	movl (ext2_superblock_buffer + s_log_block_size), %ecx
	testl $0xFFFFFF00, %ecx
	jnz .has_ext2_filesystem_unsupported_block_size
	  # verify 1024 << s_log_block_size == EXT2_BLOCK_SIZE
	movl $1024, %eax
	shll %cl, %eax
	cmpl $EXT2_BLOCK_SIZE, %eax
	jne .has_ext2_filesystem_unsupported_block_size

	# fill inode size
	movl $128, %eax
	cmpl $EXT2_GOOD_OLD_REV, (ext2_superblock_buffer + s_rev_level)
	cmovnel (ext2_superblock_buffer + s_inode_size), %eax
	movl %eax, (ext2_inode_size)

	movb $1, %al
	jmp .has_ext2_filesystem_done

 .has_ext2_filesystem_does_not_fit:
	movw $root_partition_does_not_fit_ext2_filesystem_msg, %si
	movb $0, %al
	jmp .has_ext2_filesystem_done

 .has_ext2_filesystem_invalid_magic:
	movw $root_partition_has_invalid_ext2_magic_msg, %si
	movb $0, %al
	jmp .has_ext2_filesystem_done

 .has_ext2_filesystem_unsupported_block_size:
	movw $root_partition_has_unsupported_ext2_block_size_msg, %si
	movb $0, %al
	jmp .has_ext2_filesystem_done

 .has_ext2_filesystem_done:
	popw %di
	popw %bx
	popl %ecx
	ret


# reads block in to ext2_block_buffer
#	eax:	block number
ext2_read_block:
	pushl %eax
	pushl %ebx
	pushw %cx
	pushl %edx
	pushw %di

	# NOTE: this assumes 1024 block size
	# eax := (block * block_size) / sector_size := (eax << EXT2_BLOCK_SHIFT) >> SECTOR_SHIFT
	xorl %edx, %edx
	shll $EXT2_BLOCK_SHIFT, %eax
	shrl $SECTOR_SHIFT, %eax

	# ebx:eax := eax + (ext2_partition_first_sector)
	movl (ext2_partition_first_sector + 4), %ebx
	addl (ext2_partition_first_sector + 0), %eax
	jnc .ext2_read_block_no_carry
	incl %ebx
 .ext2_read_block_no_carry:

	# sectors per block
	movw $(EXT2_BLOCK_SIZE / SECTOR_SIZE), %cx

	movw $ext2_block_buffer, %di

	movb (ext2_drive_number), %dl
	call read_from_disk

	popw %di
	popl %edx
	popw %cx
	popl %ebx
	popl %eax
	ret


# reads block group descrtiptor into ext2_block_group_descriptor
#	eax:	block group
ext2_read_block_group_descriptor:
	pushal

	# eax := bgd_byte_offset := 2048 + EXT2_BGD_SIZE * eax := (eax << EXT2_BGD_SHIFT) + 2048
	shll $EXT2_BGD_SHIFT, %eax
	addl $2048, %eax

	# eax: bgd_block  := bgd_byte_offset / EXT2_BLOCK_SIZE
	# ebx: bgd_offset := bgd_byte_offset % EXT2_BLOCK_SIZE
	xorl %edx, %edx
	movl $EXT2_BLOCK_SIZE, %ebx
	divl %ebx
	movl %edx, %ebx

	call ext2_read_block

	# esi := &ext2_block_buffer + bgd_offset := ebx + &ext2_block_buffer
	# edi := &ext2_block_group_descriptor_buffer
	movl %ebx, %esi
	addl $ext2_block_buffer, %esi
	movl $ext2_block_group_descriptor_buffer, %edi
	movw $EXT2_BGD_SIZE, %cx
	rep movsb

	popal
	ret


# reads inode into ext2_inode_buffer
#	eax:	ino
ext2_read_inode:
	pushal

	# eax := block_group = (ino - 1) / s_inodes_per_group
	# ebx := inode_index = (ino - 1) % s_inodes_per_group
	xorl %edx, %edx
	decl %eax
	movl (ext2_superblock_buffer + s_inodes_per_group), %ebx
	divl %ebx
	movl %edx, %ebx

	call ext2_read_block_group_descriptor

	# eax := inode_table_block  := (inode_index * inode_size) / EXT2_BLOCK_SIZE
	# ebx := inode_table_offset := (inode_index * inode_size) % EXT2_BLOCK_SIZE
	xorl %edx, %edx
	movl %ebx, %eax
	movl (ext2_inode_size), %ebx
	mull %ebx
	movl $EXT2_BLOCK_SIZE, %ebx
	divl %ebx
	movl %edx, %ebx

	# eax := file system block := eax + bg_inode_table
	addl (ext2_block_group_descriptor_buffer + bg_inode_table), %eax

	movb (ext2_drive_number), %dl
	call ext2_read_block

	# copy inode memory
	  # esi := inode_table_offset + ext2_block_buffer := edx + ext2_block_buffer
	movl %ebx, %esi
	addl $ext2_block_buffer, %esi
	  # edi := ext2_inode_buffer
	movl $ext2_inode_buffer, %edi
	  # cx := inode_size
	movw (ext2_inode_size), %cx
	rep movsb

	popal
	ret


# gets block index from n'th data block in inode stored in ext2_inode_buffer
#	eax:	data block index
# return:
#	eax:	block index
ext2_data_block_index:
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %esi

	# calculate max data blocks
	movl (ext2_inode_buffer + i_size), %ecx
	addl (ext2_inode_size), %ecx
	decl %ecx
	shll $EXT2_BLOCK_SHIFT, %ecx

	# verify data block is within bounds
	cmpl %ecx, %eax
	jae .ext2_data_block_index_out_of_bounds

	# check if this is direct block access
	cmpl $12, %eax
	jb .ext2_data_block_index_direct
	subl $12, %eax

	# check if this is singly indirect block access
	cmpl $(EXT2_BLOCK_SIZE / 4), %eax
	jb .ext2_data_block_index_singly_indirect
	subl $(EXT2_BLOCK_SIZE / 4), %eax

	# check if this is doubly indirect block access
	cmpl $((EXT2_BLOCK_SIZE / 4) * (EXT2_BLOCK_SIZE / 4)), %eax
	jb .ext2_data_block_index_doubly_indirect
	subl $((EXT2_BLOCK_SIZE / 4) * (EXT2_BLOCK_SIZE / 4)), %eax

	# check if this is triply indirect block access
	cmpl $((EXT2_BLOCK_SIZE / 4) * (EXT2_BLOCK_SIZE / 4) * (EXT2_BLOCK_SIZE / 4)), %eax
	jb .ext2_data_block_index_triply_indirect

	# otherwise this is invalid access
	jmp .ext2_data_block_index_invalid

 .ext2_data_block_index_direct:
	movl $(ext2_inode_buffer + i_block), %esi
	movl (%esi, %eax, 4), %eax
	jmp .ext2_data_block_index_done

 .ext2_data_block_index_singly_indirect:
	movl %eax, %ebx
	movl (ext2_inode_buffer + i_block + 12 * 4), %eax
	movw $1, %cx
	jmp .ext2_data_block_index_indirect

 .ext2_data_block_index_doubly_indirect:
	movl %eax, %ebx
	movl (ext2_inode_buffer + i_block + 13 * 4), %eax
	movw $2, %cx
	jmp .ext2_data_block_index_indirect

 .ext2_data_block_index_triply_indirect:
	movl %eax, %ebx
	movl (ext2_inode_buffer + i_block + 14 * 4), %eax
	movw $3, %cx
	jmp .ext2_data_block_index_indirect

	# eax := current block
	# ebx := index
	# cx  := depth
 .ext2_data_block_index_indirect:
	call ext2_read_block

	# store depth and index
	pushw %cx
	pushl %ebx

	cmpw $1, %cx
	jbe .ext2_data_block_index_no_shift

	# cl := shift
	movb $(EXT2_BLOCK_SHIFT - 2), %al
	decb %cl
	mulb %cl
	movb %al, %cl

	# ebx := ebx >> cl
	shrl %cl, %ebx

 .ext2_data_block_index_no_shift:
	# edx := index of next block
	movl %ebx, %eax
	xorl %edx, %edx
	movl $(EXT2_BLOCK_SIZE / 4), %ebx
	divl %ebx

	# eax := next block
	movl $ext2_block_buffer, %esi
	movl (%esi, %edx, 4), %eax

	# restore depth and index
	popl %ebx
	popw %cx

	loop .ext2_data_block_index_indirect

	jmp .ext2_data_block_index_done

 .ext2_data_block_index_out_of_bounds:
	movw $ext2_data_block_index_out_of_bounds_msg, %si
	call puts; call print_newline
	movl $0, %eax
	jmp .ext2_data_block_index_done

 .ext2_data_block_index_invalid:
	movw $ext2_data_block_index_invalid_msg, %si
	call puts; call print_newline
	movl $0, %eax
	jmp .ext2_data_block_index_done

 .ext2_data_block_index_done:
	popl %esi
	popl %edx
	popl %ecx
	popl %ebx
	ret


# find inode in inside directory inode stored in ext2_inode_buffer
# store the found inode in ext2_inode_buffer
#	si:		name string
#	cx:		name length
# return:
#	eax:	ino if inode was found, 0 otherwise
ext2_directory_find_inode:
	pushl %ebx
	pushw %cx
	pushw %dx
	pushw %si
	pushw %di

	pushl %ebp
	movl %esp, %ebp
	subl $8, %esp

	# 0(%esp) := name length
	movw %cx, 0(%esp)

	# 2(%esp) := name string
	movw %si, 2(%esp)

	# verify that the name is <= 0xFF bytes
	cmpw $0xFF, %cx
	ja .ext2_directory_find_inode_not_found

	# ebx := max data blocks: ceil(i_size / EXT2_BLOCK_SIZE)
	movl (ext2_inode_buffer + i_size), %ebx
	addl $EXT2_BLOCK_SHIFT, %ebx
	decl %ebx
	shrl $EXT2_BLOCK_SHIFT, %ebx
	jz .ext2_directory_find_inode_not_found

	# 4(%esp) := current block
	movl $0, 4(%esp)

 .ext2_directory_find_inode_block_read_loop:
	# get next block index
	movl 4(%esp), %eax
	call ext2_data_block_index
	test %eax, %eax
	jz .ext2_directory_find_inode_next_block

	# read current block
	call ext2_read_block

	# dx := current entry pointer
	movw $ext2_block_buffer, %si

 .ext2_directory_find_inode_loop_entries:
	# temporarily store entry pointer in dx
	movw %si, %dx

	# check if name length matches
	# cx := name length
	movw 0(%esp), %cx
	cmpb 6(%si), %cl
	jne .ext2_directory_find_inode_next_entry

	# si := entry name
	addw $8, %si

	# di := asked name
	movw 2(%esp), %di

	# check if name matches
	call memcmp
	test %al, %al
	# NOTE: dx contains entry pointer
	jnz .ext2_directory_find_inode_found

 .ext2_directory_find_inode_next_entry:
	# restore si
	movw %dx, %si

	# go to next entry if this block contains one
	addw 4(%si), %si
	cmpw $(ext2_block_buffer + EXT2_BLOCK_SIZE), %si
	jb .ext2_directory_find_inode_loop_entries

 .ext2_directory_find_inode_next_block:
	incl 4(%esp)
	cmpl %ebx, 4(%esp)
	jb .ext2_directory_find_inode_block_read_loop

 .ext2_directory_find_inode_not_found:
	movb $0, %al
	jmp .ext2_directory_find_inode_done

 .ext2_directory_find_inode_found:
	# extract ino and read it to ext2_inode_buffer
	movw %dx, %si
	movl 0(%si), %eax
	call ext2_read_inode

 .ext2_directory_find_inode_done:
	leavel
	popw %di
	popw %si
	popw %dx
	popw %cx
	popl %ebx
	ret


# search for kernel file from filesystem
.global ext2_find_kernel
ext2_find_kernel:
	movl $EXT2_ROOT_INO, %eax
	call ext2_read_inode

	movw $kernel_path, %di
 .ext2_find_kernel_loop:
	movw (%di), %si

	# check if this list is done
	testw %si, %si
	jz .ext2_find_kernel_loop_done

	# check that current part is directory
	movw (ext2_inode_buffer + i_mode), %ax
	andw $EXT2_S_IMASK, %ax
	cmpw $EXT2_S_IFDIR, %ax
	jne .ext2_find_kernel_part_not_dir

	# prepare registers for directory finding
	movw 0(%si), %cx
	addw $2, %si

	# print search path
	pushw %si
	movw $ext2_looking_for_msg, %si
	call puts
	popw %si
	call puts; call print_newline

	# search current directory for this file
	call ext2_directory_find_inode
	testl %eax, %eax
	jz .ext2_find_kernel_part_not_found

	# loop to next part
	addw $2, %di
	jmp .ext2_find_kernel_loop

 .ext2_find_kernel_loop_done:

	# check that kernel is a regular file
	movw (ext2_inode_buffer + i_mode), %ax
	andw $EXT2_S_IMASK, %ax
	cmpw $EXT2_S_IFREG, %ax
	jne .ext2_find_kernel_not_reg

	movw $ext2_kernel_found_msg, %si
	call puts; call print_newline

	1: jmp 1b

	ret

 .ext2_find_kernel_part_not_dir:
	movw $ext2_part_not_dir_msg, %si
	jmp print_and_halt

 .ext2_find_kernel_part_not_found:
	movw $ext2_part_not_found_msg, %si
	jmp print_and_halt

 .ext2_find_kernel_not_reg:
	movw $ext2_kernel_not_reg_msg, %si
	jmp print_and_halt


kernel_path:
	.short kernel_path1
	.short kernel_path2
	.short 0
kernel_path1:
	.short 4
	.asciz "boot"
kernel_path2:
	.short 15
	.asciz "banan-os.kernel"


root_partition_does_not_fit_ext2_filesystem_msg:
	.asciz "Root partition is too small to contain ext2 filesystem"
root_partition_has_invalid_ext2_magic_msg:
	.asciz "Root partition doesn't contain ext2 magic number"
root_partition_has_unsupported_ext2_block_size_msg:
	.asciz "Root partition has unsupported ext2 block size (only 1024 supported)"

ext2_part_not_dir_msg:
	.asciz "inode in root path is not directory"
ext2_part_not_found_msg:
	.asciz " not found"
ext2_kernel_not_reg_msg:
	.asciz "kernel is not a regular file"
ext2_kernel_found_msg:
	.asciz "kernel found!"

ext2_data_block_index_out_of_bounds_msg:
	.asciz "data block index out of bounds"
ext2_data_block_index_invalid_msg:
	.asciz "data block index is invalid"

ext2_looking_for_msg:
	.asciz "looking for "

.section .bss

ext2_block_buffer:
	.skip EXT2_BLOCK_SIZE

ext2_partition_first_sector:
	.skip 8

ext2_drive_number:
	.skip 1
	.skip 3 # padding

# NOTE: fits in 2 bytes
ext2_inode_size:
	.skip 4

ext2_superblock_buffer:
	.skip EXT2_SUPERBLOCK_SIZE

ext2_block_group_descriptor_buffer:
	.skip EXT2_BGD_SIZE

ext2_inode_buffer:
	.skip EXT2_INODE_SIZE_MAX
