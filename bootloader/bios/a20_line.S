.code16
.section .stage2

# checks whether A20 line is enabled or disabled
# return
#	ax: 1 if enabled, 0 otherwise
check_a20:
	pushf
	pushw %si
	pushw %di
	pushw %ds
	pushw %es

	cli

	xorw %ax, %ax
	movw %ax, %es
	notw %ax
	movw %ax, %ds

	movw $0x0500, %di
	movw $0x0510, %si

	movb %es:(%di), %al
	pushw %ax

	movb %ds:(%si), %al
	pushw %ax

	movb $0x00, %es:(%di)
	movb $0xFF, %ds:(%si)

	cmpb $0xFF, %es:(%di)

	pop %ax
	movb %al, %ds:(%si)

	pop %ax
	movb %al, %es:(%di)

	movw $0, %ax
	je .check_a20_done

	movw $1, %ax

 .check_a20_done:
	popw %es
	popw %ds
	popw %di
	popw %si
	popf
	ret


# Try to enable A20 using PS2 controller
enable_a20_ps2:
	pushf
	pushw %ax

	cli

	# disable first port
	call .enable_a20_ps2_wait1
	movb $0xAD, %al
	outb %al, $0x64

	# read controller output
	call .enable_a20_ps2_wait1
	movb $0xD0, %al
	outb %al, $0x64

	call .enable_a20_ps2_wait2
	inb $0x60, %al
	pushw %ax

	# write controller output
	call .enable_a20_ps2_wait1
	movb $0xD1, %al
	outb %al, $0x64

	call .enable_a20_ps2_wait1
	popw %ax
	orw $2, %ax
	outb %al, $0x60

	# enable first port
	call .enable_a20_ps2_wait1
	movb $0xAE, %al
	outb %al, $0x64

	call .enable_a20_ps2_wait1

	popw %ax
	popf
	ret

 .enable_a20_ps2_wait1:
	inb $0x64, %al
	test $2, %al
	jnz .enable_a20_ps2_wait1
	ret

 .enable_a20_ps2_wait2:
	inb $0x64, %al
	test $1, %al
	jnz .enable_a20_ps2_wait1
	ret


# Check if A20 line is disabled. If it is, try to enable it
.global enable_a20
enable_a20:
	pushw %ax
	pushw %si

	call check_a20
	testw %ax, %ax
	jnz .enable_a20_done

	movw $a20_line_disabled_msg, %si
	call puts; call print_newline

	# Try to enable A20 line using bios interrupt
	movw $0x2401, %ax
	int $0x15
	call check_a20
	testw %ax, %ax
	jnz .enable_a20_done

	# Try to enable A20 line using ps2 controller
	call enable_a20_ps2
	call check_a20
	testw %ax, %ax
	jnz .enable_a20_done

	# Try to enable A20 line using fast A20 gate
	inb $0x92, %al
	testb $2, %al
	jnz .enable_a20_fast_done
	orb $2, %al
	outb %al, $0x92
 .enable_a20_fast_done:

	call check_a20
	testw %ax, %ax
	jnz .enable_a20_done

	movw $a20_could_not_enable_msg, %si
	call print_and_halt

 .enable_a20_done:
	movw $a20_line_enabled_msg, %si
	call puts; call print_newline

	popw %si
	popw %ax
	ret

.section .data

a20_line_disabled_msg:
	.asciz "A20 line disabled. Trying to enable it"

a20_line_enabled_msg:
	.asciz "A20 line enabled"

a20_could_not_enable_msg:
	.asciz "Could not enable A20 line"
