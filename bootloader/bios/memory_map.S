.code16

.section .stage2

# fills memory map data structure
# doesn't return on error
# NO REGISTERS SAVED
.global get_memory_map
get_memory_map:
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
	ret

 .get_memory_map_error:
	movw $memory_map_error_msg, %si
	jmp print_and_halt


# print memory map from memory_map_entries
# NO REGISTERS SAVED
.global print_memory_map
print_memory_map:
	movw $memory_map_msg, %si
	call puts
	call print_newline

	movl (memory_map_entry_count), %edx
	movw $memory_map_entries, %si

	movw $16, %bx
	movw $4, %cx

 .loop_memory_map:
	movb $' ', %al
	call putc; call putc; call putc; call putc

	movw 0x06(%si), %ax
	call print_number
	movw 0x04(%si), %ax
	call print_number
	movw 0x02(%si), %ax
	call print_number
	movw 0x00(%si), %ax
	call print_number

	movb $',', %al
	call putc
	movb $' ', %al
	call putc

	movw 0x0E(%si), %ax
	call print_number
	movw 0x0C(%si), %ax
	call print_number
	movw 0x0A(%si), %ax
	call print_number
	movw 0x08(%si), %ax
	call print_number

	movb $',', %al
	call putc
	movb $' ', %al
	call putc

	movw 0x12(%si), %ax
	call print_number
	movw 0x10(%si), %ax
	call print_number

	call print_newline

	addw $20, %si

	decl %edx
	jnz .loop_memory_map

	ret


memory_map_msg:
	.asciz "memmap:"
memory_map_error_msg:
	.asciz "Failed to get memory map"

.section .bss

.global memory_map
memory_map:
memory_map_entry_count:
	.skip 4
# 100 entries should be enough...
memory_map_entries:
	.skip 20 * 100
