.code16

.section .stage2

# fills command line buffer
# NO REGISTERS SAVED
.global read_user_command_line
read_user_command_line:
	# print initial command line
	movw $command_line_enter_msg, %si
	call puts
	movw $command_line_buffer, %si
	call puts

	# prepare registers for input
	movw $command_line_enter_msg, %si
	movw $command_line_buffer, %di
 .read_user_command_line_goto_end:
	cmpb $0, (%di)
	jz .read_user_command_line_loop
	incw %di
	jmp .read_user_command_line_goto_end

 .read_user_command_line_loop:
	call getc

	cmpb $'\b', %al
	je .read_user_command_line_backspace

	# Not sure if some BIOSes return '\n' as enter, but check it just in case
	cmpb $'\r', %al
	je .read_user_command_line_done
	cmpb $'\n', %al
	je .read_user_command_line_done

	pushw %ax

	call isprint
	testb %al, %al
	jz .read_user_command_line_loop

	popw %ax

	# put byte to buffer
	movb %al, (%di)
	incw %di

	# print byte
	call putc

	jmp .read_user_command_line_loop

 .read_user_command_line_backspace:
	# don't do anything if at the beginning
	cmpw $command_line_buffer, %di
	je .read_user_command_line_loop

	# decrement buffer pointer
	decw %di

	# erase byte in display
	call print_backspace

	jmp .read_user_command_line_loop

 .read_user_command_line_done:
	# null terminate command line
	movb $0, (%di)

	call print_newline

	ret


command_line_enter_msg:
	.asciz "cmdline: "

.global command_line
command_line:
# 100 character command line
command_line_buffer:
	.ascii "root=/dev/sda2"
	.skip 100 - 28
