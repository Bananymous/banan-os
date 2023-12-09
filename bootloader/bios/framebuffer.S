.code16
.section .stage2

# kernel framebuffer information format
#	.align 8
#	.long 0xBABAB007
#	.long -(0xBABAB007 + width + height + bpp)
#	.long width		(2 bytes used, 4 bytes for ease of calculation)
#	.long height	(2 bytes used, 4 bytes for ease of calculation)
#	.long bpp		(1 bytes used, 4 bytes for ease of calculation)

# scan memory 0x100000 -> 0x200000 for framebuffer information
# return:
#	ax: target width
#	bx: target height
#	cx: target bpp
vesa_scan_kernel_image:
	pushl %edx
	pushl %esi

	movl $0x100000, %esi

 .vesa_scan_kernel_image_loop:
	# check magic
	cmpl $0xBABAB007, (%esi)
	jne .vesa_scan_kernel_image_next_addr

	# check checksum
	movl 0x00(%esi), %edx
	addl 0x04(%esi), %edx
	addl 0x08(%esi), %edx
	addl 0x0C(%esi), %edx
	addl 0x10(%esi), %edx
	testl %edx, %edx
	jnz .vesa_scan_kernel_image_next_addr

	# set return registers
	movw 0x08(%esi), %ax
	movw 0x0C(%esi), %bx
	movw 0x10(%esi), %cx
	jmp .vesa_scan_kernel_image_done

 .vesa_scan_kernel_image_next_addr:
	addl $8, %esi
	cmpl $0x200000, %esi
	jb .vesa_scan_kernel_image_loop

	# zero out return registers
	xorw %ax, %ax
	xorw %bx, %bx
	xorw %cx, %cx

 .vesa_scan_kernel_image_done:
	popl %esi
	popl %edx
	ret

# Find suitable video mode and save it in (vesa_target_mode)
vesa_find_video_mode:
	pushw %ax
	pushw %bx
	pushw %cx
	pushw %di
	pushl %esi

	pushl %ebp
	movl %esp, %ebp
	subl $6, %esp

	# clear target mode and frame buffer
	movw $0, (vesa_target_mode)
	movl $0, (framebuffer + 0)
	movl $0, (framebuffer + 4)
	movl $0, (framebuffer + 8)
	movl $0, (framebuffer + 12)
	movw $0, (framebuffer + 16)

	call vesa_scan_kernel_image
	testw %ax, %ax
	jz .vesa_find_video_mode_loop_modes_done

	# save arguments in stack
	movw %ax, -2(%ebp)
	movw %bx, -4(%ebp)
	movw %cx, -6(%ebp)

	# get vesa information
	movw $0x4F00, %ax
	movw $vesa_info_buffer, %di
	int $0x10
	cmpb $0x4F, %al; jne .vesa_unsupported
	cmpb $0x00, %ah; jne .vesa_error

	# confirm that response starts with 'VESA'
	cmpl $0x41534556, (vesa_info_buffer)
	jne .vesa_error

	# confirm that version is atleast 2.0
	cmpw $0x0200, (vesa_info_buffer + 0x04)
	jb .vesa_unsupported_version

	movl $(vesa_info_buffer + 0x0E), %esi
	movl (%esi), %esi
 .vesa_find_video_mode_loop_modes:
	cmpw $0xFFFF, (%esi)
	je .vesa_find_video_mode_loop_modes_done

	# get info of next mode
	movw $0x4F01, %ax
	movw (%esi), %cx
	movw $vesa_mode_info_buffer, %di
	int $0x10
	cmpb $0x4F, %al; jne .vesa_unsupported
	cmpb $0x00, %ah; jne .vesa_error

	# check whether in graphics mode
	testb $0x10, (vesa_mode_info_buffer + 0)
	jz .vesa_find_video_mode_next_mode

	# compare mode's dimensions
	movw -2(%ebp), %ax; cmpw %ax, (vesa_mode_info_buffer + 0x12)
	jne .vesa_find_video_mode_next_mode
	movw -4(%ebp), %ax; cmpw %ax, (vesa_mode_info_buffer + 0x14)
	jne .vesa_find_video_mode_next_mode
	movb -6(%ebp), %al; cmpb %al, (vesa_mode_info_buffer + 0x19)
	jne .vesa_find_video_mode_next_mode

	# set address, pitch, type
	movl (vesa_mode_info_buffer + 0x28), %esi
	movl %esi, (framebuffer + 0)
	movw (vesa_mode_info_buffer + 0x10), %ax
	movw %ax,  (framebuffer + 4)
	movb $1,   (framebuffer + 17)

	# set width, height, bpp
	movw -2(%ebp), %ax; movw %ax, (framebuffer +  8)
	movw -4(%ebp), %ax; movw %ax, (framebuffer + 12)
	movw -6(%ebp), %ax; movb %al, (framebuffer + 16)

	movw %cx, (vesa_target_mode)
	jmp .vesa_find_video_mode_loop_modes_done

 .vesa_find_video_mode_next_mode:
	addl $2, %esi
	jmp .vesa_find_video_mode_loop_modes

 .vesa_find_video_mode_loop_modes_done:
	leavel
	popl %esi
	popw %di
	popw %cx
	popw %bx
	popw %ax
	ret

 .vesa_unsupported:
	movw $vesa_unsupported_msg, %si
	jmp print_and_halt
 .vesa_unsupported_version:
	movw $vesa_unsupported_version_msg, %si
	jmp print_and_halt
 .vesa_error:
	movw $vesa_error_msg, %si
	jmp print_and_halt


# scan for video mode in kernel memory and set the correct one.
# when video mode is not found or does not exists,
# set it to 80x25 text mode to clear the screen.
.global vesa_set_video_mode
vesa_set_video_mode:
	pushw %ax
	pushw %bx

	call vesa_find_video_mode

	movw (vesa_target_mode), %bx
	testw %bx, %bx
	jz .vesa_set_target_mode_generic

	movw $0x4F02, %ax
	orw $0x4000, %bx
	int $0x10

	jmp .set_video_done

 .vesa_set_target_mode_generic:
	movb $0x03, %al
	movb $0x00, %ah
	int $0x10

 .set_video_done:
	popw %bx
	popw %ax
	ret


vesa_error_msg:
	.asciz "VESA error"
vesa_unsupported_msg:
	.asciz "VESA unsupported"
vesa_unsupported_version_msg:
	.asciz "VESA unsupported version"
vesa_success_msg:
	.asciz "VESA success"

.section .bss

vesa_info_buffer:
	.skip 512

vesa_mode_info_buffer:
	.skip 256

vesa_target_mode:
	.skip 2

.global framebuffer
framebuffer:
	.skip 4	# address
	.skip 4	# pitch
	.skip 4	# width
	.skip 4 # height
	.skip 1 # bpp
	.skip 1 # type
