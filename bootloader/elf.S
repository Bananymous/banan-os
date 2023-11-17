.set SECTOR_SIZE,	512

# file header field offsets
.set e_type,		16
.set e_machine,		18
.set e_version,		20
.set e_entry,		24
.set e_phoff,		32
.set e_shoff,		40
.set e_flags,		48
.set e_ehsize,		52
.set e_phentsize,	54
.set e_phnum,		56
.set e_shentsize,	58
.set e_shnum,		60
.set e_shstrndx,	62

# e_ident offsets
.set EI_CLASS,		4
.set EI_DATA,		5
.set EI_VERSION,	6

# e_ident constants
.set ELFMAGIC,		0x464C457F
.set ELFCLASS64,	2
.set ELFDATA2LSB,	1
.set EV_CURRENT,	1

# e_type constants
.set ET_EXEC,		2

# program header field offsets
.set p_type,		0
.set p_flags,		4
.set p_offset,		8
.set p_vaddr,		16
.set p_paddr,		24
.set p_filesz,		32
.set p_memsz,		40
.set p_align,		48

# p_type constants
.set PT_NULL,		0
.set PT_LOAD,		1

.code16
.section .stage2

# Validate file header stored in elf_file_header
# returns only on success
elf_validate_file_header:
	cmpl $ELFMAGIC, (elf_file_header)
	jne .elf_validate_file_header_invalid_magic

	cmpb $ELFCLASS64, (elf_file_header + EI_CLASS)
	jne .elf_validate_file_header_only_64bit_supported

	cmpb $ELFDATA2LSB, (elf_file_header + EI_DATA)
	jne .elf_validate_file_header_only_little_endian_supported

	cmpb $EV_CURRENT, (elf_file_header + EI_VERSION)
	jne .elf_validate_file_header_not_current_version

	cmpl $EV_CURRENT, (elf_file_header + e_version)
	jne .elf_validate_file_header_not_current_version

	cmpw $ET_EXEC, (elf_file_header + e_type)
	jne .elf_validate_file_header_not_executable

	ret

 .elf_validate_file_header_invalid_magic:
	movw $elf_validate_file_header_invalid_magic_msg, %si
	jmp print_and_halt
 .elf_validate_file_header_only_64bit_supported:
	movw $elf_validate_file_header_only_64bit_supported_msg, %si
	jmp print_and_halt
 .elf_validate_file_header_only_little_endian_supported:
	movw $elf_validate_file_header_only_little_endian_supported_msg, %si
	jmp print_and_halt
 .elf_validate_file_header_not_current_version:
	movw $elf_validate_file_header_not_current_version_msg, %si
	jmp print_and_halt
 .elf_validate_file_header_not_executable:
	movw $elf_validate_file_header_not_executable_msg, %si
	jmp print_and_halt


# read callback format
#	eax: first byte
#	ecx: byte count
#	edi: buffer
# returns only on success


# reads kernel to memory
#	esi:	callback for reading from kernel image
# return:
#	eax:	kernel entry address
.global elf_read_kernel_to_memory
elf_read_kernel_to_memory:
	pushal
	pushl %ebp
	movl %esp, %ebp
	subl $2, %esp

	# read file header
	movl $0, %eax
	movl $64, %ecx
	movl $elf_file_header, %edi
	call *%esi

	call elf_validate_file_header

	cmpl $0, (elf_file_header + e_phoff + 4)
	jnz .elf_read_kernel_to_memory_unsupported_offset

	# current program header
	movw $0, -2(%ebp)

 .elf_read_kernel_to_memory_loop_program_headers:
	movw -2(%ebp), %cx
	cmpw (elf_file_header + e_phnum), %cx
	jae .elf_read_kernel_to_memory_done

	# eax := program_header_index * e_phentsize + e_phoff
	xorl %eax, %eax
	movw %cx, %ax
	xorl %ebx, %ebx
	movw (elf_file_header + e_phentsize), %bx
	mull %ebx
	addl (elf_file_header + e_phoff), %eax
	jc .elf_read_kernel_to_memory_unsupported_offset

	# setup program header size and address
	movl $56, %ecx
	movl $elf_program_header, %edi

	# read the program header
	call *%esi

	# test if program header is empty
	cmpl $PT_NULL, (elf_program_header + p_type)
	je .elf_read_kernel_to_memory_null_program_header

	# confirm that the program header is loadable
	cmpl $PT_LOAD, (elf_program_header + p_type)
	jne .elf_read_kernel_to_memory_not_loadable_header

	# memset p_filesz -> p_memsz to 0
	movl (elf_program_header + p_filesz), %ebx

	movl (elf_program_header + p_vaddr),  %edi
	andl $0x7FFFFFFF, %edi
	addl %ebx, %edi

	movl (elf_program_header + p_memsz),  %ecx
	subl %ebx, %ecx
	jz .elf_read_kernel_to_memory_no_memset

 .elf_read_kernel_to_memory_memset:
	movb $0, (%edi)
	decl %ecx
	jnz .elf_read_kernel_to_memory_memset
 .elf_read_kernel_to_memory_no_memset:

	# read file specified in program header to memory
	movl (elf_program_header + p_offset), %eax
	movl (elf_program_header + p_vaddr),  %edi
	andl $0x7FFFFFFF, %edi
	movl (elf_program_header + p_filesz), %ecx

	#call print_hex32; call print_newline

	call *%esi

 .elf_read_kernel_to_memory_null_program_header:
	incw -2(%ebp)
	jmp .elf_read_kernel_to_memory_loop_program_headers

 .elf_read_kernel_to_memory_done:
	leavel
	popal

	# set kernel entry address
	movl (elf_file_header + e_entry), %eax
	andl $0x7FFFFF, %eax

	ret

 .elf_read_kernel_to_memory_unsupported_offset:
	movw $elf_read_kernel_to_memory_unsupported_offset_msg, %si
	jmp print_and_halt
 .elf_read_kernel_to_memory_not_loadable_header:
	movw $elf_read_kernel_to_memory_not_loadable_header_msg, %si
	jmp print_and_halt


elf_validate_file_header_invalid_magic_msg:
	.asciz "ELF: file has invalid ELF magic"
elf_validate_file_header_only_64bit_supported_msg:
	.asciz "ELF: file is not targettint 64 bit"
elf_validate_file_header_only_little_endian_supported_msg:
	.asciz "ELF: file is not in little endian format"
elf_validate_file_header_not_current_version_msg:
	.asciz "ELF: file is not in current ELF version"
elf_validate_file_header_not_executable_msg:
	.asciz "ELF: file is not an executable"

elf_read_kernel_to_memory_unsupported_offset_msg:
	.asciz "ELF: unsupported offset (only 32 bit offsets supported)"
elf_read_kernel_to_memory_not_loadable_header_msg:
	.asciz "ELF: kernel contains non-loadable program header"

.section .bss

elf_file_header:
	.skip 64

elf_program_header:
	.skip 56
