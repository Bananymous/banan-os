.set SCREEN_WIDTH,	80
.set SCREEN_HEIGHT,	25

.code16

.section .stage1

# prints character to screen
# al:		ascii character to print
.global putc
putc:
	pushw %ax
	pushw %bx
	movb $0x0E, %ah
	xorb %bh, %bh
	int $0x10
	popw %bx
	popw %ax
	ret

# prints null terminated string to screen
# ds:si:	string address
.global puts
puts:
	pushw %si
	pushw %bx
	pushw %ax

	movb $0x0E, %ah
	xorb %bh, %bh

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
.global memcmp
memcmp:
	# NOTE: using pusha + popa to save space
	pusha
	cld
	repe cmpsb
	popa
	setzb %al
	ret


.section .stage2

# read a character from keyboard
# return:
#	al: ascii
#	ah: bios scan code
.global getc
getc:
	movb $0x00, %ah
	int $0x16
	ret

# prints newline to screen
.global print_newline
print_newline:
	pushw %ax
	movb $'\r', %al
	call putc
	movb $'\n', %al
	call putc
	pop %ax
	ret

# prints backspace to screen, can go back a line
.global print_backspace
print_backspace:
	pushw %ax
	pushw %bx
	pushw %cx
	pushw %dx

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
	popw %dx
	popw %cx
	popw %bx
	popw %ax
	ret

# print number to screen
# ax:	number to print
# bx:	number base
# cx:	min width (zero pads if shorter)
.global print_number
print_number:
	pusha
	pushl %ebp
	movl %esp, %ebp

	# save min width
	subl $4, %esp
	movw %cx, (%esp)

	movw $print_number_buffer, %si
	xorw %cx, %cx

 .print_number_fill_loop:
	# fill buffer with all remainders ax % bx
	xorw %dx, %dx
	divw %bx
	movb %dl, (%si)
	incw %si
	incw %cx
	testw %ax, %ax
	jnz .print_number_fill_loop

	# check if zero pad is required
	cmpw (%esp), %cx
	jae .print_number_print_loop

	# dx: saved number count
	# cx: zero pad count
	movw %cx, %dx
	movw (%esp), %cx
	subw %dx, %cx
	movb $'0', %al

 .print_number_pad_zeroes:
	call putc
	loop .print_number_pad_zeroes

	# restore number count
	movw %dx, %cx

 .print_number_print_loop:
	decw %si
	movb (%si), %al
	cmpb $10, %al
	jae .print_number_hex
	addb $'0', %al
	jmp .print_number_do_print
 .print_number_hex:
	addb $('a' - 10), %al
 .print_number_do_print:
	call putc
	loop .print_number_print_loop

	leavel
	popa
	ret

# test if character is printable ascii
#	al: character to test
# return:
#	al: 1 if is printable, 0 otherwise
.global isprint
isprint:
	subb $0x20, %al
	cmpb $(0x7E - 0x20), %al
	ja .isprint_not_printable
	movb $1, %al
	ret
 .isprint_not_printable:
	movb $0, %al
	ret

.section .bss

# enough for base 2 printing
print_number_buffer:
	.skip 16
