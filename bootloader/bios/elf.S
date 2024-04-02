.set SECTOR_SIZE,	512

# file header field offsets
.set e_type,		16
.set e_machine,		18
.set e_version,		20
.set e_entry,		24

.set e32_phoff,		28
.set e32_shoff,		32
.set e32_flags,		36
.set e32_ehsize,	40
.set e32_phentsize,	42
.set e32_phnum,		44
.set e32_shentsize,	46
.set e32_shnum,		48
.set e32_shstrndx,	50

.set e64_phoff,		32
.set e64_shoff,		40
.set e64_flags,		48
.set e64_ehsize,	52
.set e64_phentsize,	54
.set e64_phnum,		56
.set e64_shentsize,	58
.set e64_shnum,		60
.set e64_shstrndx,	62

# e_ident offsets
.set EI_CLASS,		4
.set EI_DATA,		5
.set EI_VERSION,	6

# e_ident constants
.set ELFMAGIC,		0x464C457F
.set ELFCLASS32,	1
.set ELFCLASS64,	2
.set ELFDATA2LSB,	1
.set EV_CURRENT,	1

# e_type constants
.set ET_EXEC,		2

# program header field offsets
.set p_type,		0

.set p32_offset,	4
.set p32_vaddr,		8
.set p32_paddr,		12
.set p32_filesz,	16
.set p32_memsz,		20
.set p32_flags,		24
.set p32_align,		28

.set p64_flags,		4
.set p64_offset,	8
.set p64_vaddr,		16
.set p64_paddr,		24
.set p64_filesz,	32
.set p64_memsz,		40
.set p64_align,		48

# p_type constants
.set PT_NULL,		0
.set PT_LOAD,		1

# mask for entry point and segment loading
.set LOAD_MASK,		0x07FFFFFF

.code16
.section .stage2

# Validate file header stored in elf_file_header
# returns only on success
elf_validate_file_header:
	cmpl $ELFMAGIC, (elf_file_header)
	jne .elf_validate_file_header_invalid_magic

	cmpb $ELFCLASS32, (elf_file_header + EI_CLASS)
	je .elf_validate_file_header_class_valid
	cmpb $ELFCLASS64, (elf_file_header + EI_CLASS)
	je .elf_validate_file_header_class_valid
	jmp .elf_validate_file_header_invalid_class
 .elf_validate_file_header_class_valid:

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
 .elf_validate_file_header_invalid_class:
	movw $elf_validate_file_header_invalid_class_msg, %si
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


# sets memory to zero
#   edi: start address
#   ecx: byte count
# on return
#   edi: start address + byte count
#   ecx: 0
elf_memset_zero:
	test %ecx, %ecx
	jz .elf_memset_zero_done
 .elf_memset_zero_loop:
	movb $0, (%edi)
	incl %edi
	decl %ecx
	jnz .elf_memset_zero_loop
 .elf_memset_zero_done:
	ret

# reads memory specified by 32 bit elf_program_header to memory
elf_read_program_header32_to_memory:
	pushal
	pushl %ebp
	movl %esp, %ebp

	# memset p_filesz -> p_memsz to 0
	movl (elf_program_header + p32_filesz), %ebx
	movl (elf_program_header + p32_vaddr),  %edi
	andl $LOAD_MASK, %edi
	addl %ebx, %edi
	movl (elf_program_header + p32_memsz),  %ecx
	subl %ebx, %ecx
	call elf_memset_zero

	# read file specified in program header to memory
	movl (elf_program_header + p32_offset), %eax
	movl (elf_program_header + p32_vaddr),  %edi
	andl $LOAD_MASK, %edi
	movl (elf_program_header + p32_filesz), %ecx
	call *%esi

	leavel
	popal
	ret


# reads memory specified by 64 bit elf_program_header to memory
elf_read_program_header64_to_memory:
	pushal
	pushl %ebp
	movl %esp, %ebp

	# memset p_filesz -> p_memsz to 0
	movl (elf_program_header + p64_filesz), %ebx
	movl (elf_program_header + p64_vaddr),  %edi
	andl $LOAD_MASK, %edi
	addl %ebx, %edi
	movl (elf_program_header + p64_memsz),  %ecx
	subl %ebx, %ecx
	call elf_memset_zero

	# read file specified in program header to memory
	movl (elf_program_header + p64_offset), %eax
	movl (elf_program_header + p64_vaddr),  %edi
	andl $LOAD_MASK, %edi
	movl (elf_program_header + p64_filesz), %ecx
	call *%esi

	leavel
	popal
	ret


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

	# read start of file header
	movl $0, %eax
	movl $24, %ecx
	movl $elf_file_header, %edi
	call *%esi

	call elf_validate_file_header

	# determine file header size
	movl $52, %ecx
	movl $64, %edx
	cmpb $ELFCLASS64, (elf_file_header + EI_CLASS)
	cmovel %edx, %ecx

	# read full file header
	movl $0, %eax
	movl $elf_file_header, %edi
	call *%esi

	# verify that e_phoff fits in 32 bits
	cmpb $ELFCLASS64, (elf_file_header + EI_CLASS)
	jne .elf_read_kernel_to_memory_valid_offset
	cmpl $0, (elf_file_header + e64_phoff + 4)
	jnz .elf_read_kernel_to_memory_unsupported_offset
 .elf_read_kernel_to_memory_valid_offset:

	# read architecture phentsize and phnum to fixed locations
	movw (elf_file_header + e32_phentsize), %ax
	movw (elf_file_header + e32_phnum),     %bx
	movl (elf_file_header + e32_phoff),     %ecx
	cmpb $ELFCLASS64, (elf_file_header + EI_CLASS)
	cmovew (elf_file_header + e64_phentsize), %ax
	cmovew (elf_file_header + e64_phnum),     %bx
	cmovel (elf_file_header + e64_phoff),     %ecx
	movw %ax,  (elf_file_header_phentsize)
	movw %bx,  (elf_file_header_phnum)
	movl %ecx, (elf_file_header_phoff)

	# current program header
	movw $0, -2(%ebp)

 .elf_read_kernel_to_memory_loop_program_headers:
	movw -2(%ebp), %cx
	cmpw (elf_file_header_phnum), %cx
	jae .elf_read_kernel_to_memory_done

	# eax := program_header_index * e_phentsize + e_phoff
	xorl %eax, %eax
	movw %cx, %ax
	xorl %ebx, %ebx
	movw (elf_file_header_phentsize), %bx
	mull %ebx
	addl (elf_file_header_phoff), %eax
	jc .elf_read_kernel_to_memory_unsupported_offset

	# determine program header size
	movl $32, %ecx
	movl $56, %edx
	cmpb $ELFCLASS64, (elf_file_header + EI_CLASS)
	cmovel %edx, %ecx

	# read program header
	movl $elf_program_header, %edi
	call *%esi

	# test if program header is NULL header
	cmpl $PT_NULL, (elf_program_header + p_type)
	je .elf_read_kernel_to_memory_null_program_header

	# confirm that the program header is loadable
	cmpl $PT_LOAD, (elf_program_header + p_type)
	jne .elf_read_kernel_to_memory_not_loadable_header

	# read program header to memory
	movl $elf_read_program_header32_to_memory, %eax
	movl $elf_read_program_header64_to_memory, %ebx
	cmpb $ELFCLASS64, (elf_file_header + EI_CLASS)
	cmovel %ebx, %eax
	call *%eax

 .elf_read_kernel_to_memory_null_program_header:
	incw -2(%ebp)
	jmp .elf_read_kernel_to_memory_loop_program_headers

 .elf_read_kernel_to_memory_done:
	leavel
	popal

	# set kernel entry address
	movl (elf_file_header + e_entry), %eax
	andl $LOAD_MASK, %eax

	ret

 .elf_read_kernel_to_memory_unsupported_offset:
	movw $elf_read_kernel_to_memory_unsupported_offset_msg, %si
	jmp print_and_halt
 .elf_read_kernel_to_memory_not_loadable_header:
	movw $elf_read_kernel_to_memory_not_loadable_header_msg, %si
	jmp print_and_halt

.section .data

elf_validate_file_header_invalid_magic_msg:
	.asciz "ELF: file has invalid ELF magic"
elf_validate_file_header_invalid_class_msg:
	.asciz "ELF: file has invalid ELF class"
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

elf_file_header_phentsize:
	.skip 2
elf_file_header_phnum:
	.skip 2
elf_file_header_phoff:
	.skip 4 # NOTE: only 32 bit offsets are supported

elf_program_header:
	.skip 56
