.set TARGET_WIDTH,	800
.set TARGET_HEIGHT,	600
.set TARGET_BPP,	32

.code16
.section .stage2

# Find suitable video mode
# return:
#	ax: video mode number if found, 0 otherwise
.global vesa_find_video_mode
vesa_find_video_mode:
	pushw %ax
	pushw %cx
	pushw %di
	pushl %esi

	# clear target mode and frame buffer
	movw $0, (vesa_target_mode)
	movl $0, (framebuffer + 0)
	movl $0, (framebuffer + 4)
	movl $0, (framebuffer + 8)
	movl $0, (framebuffer + 12)
	movw $0, (framebuffer + 16)

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
	cmpw $TARGET_WIDTH, (vesa_mode_info_buffer + 0x12)
	jne .vesa_find_video_mode_next_mode
	cmpw $TARGET_HEIGHT, (vesa_mode_info_buffer + 0x14)
	jne .vesa_find_video_mode_next_mode
	cmpb $TARGET_BPP, (vesa_mode_info_buffer + 0x19)
	jne .vesa_find_video_mode_next_mode

	movl (vesa_mode_info_buffer + 0x28), %esi
	movl %esi,				(framebuffer + 0)
	movw (vesa_mode_info_buffer + 0x10), %ax
	movw %ax,				(framebuffer + 4)
	movl $TARGET_WIDTH,		(framebuffer + 8)
	movl $TARGET_HEIGHT,	(framebuffer + 12)
	movb $TARGET_BPP,		(framebuffer + 16)
	movb $1,				(framebuffer + 17)

	movw %cx, (vesa_target_mode)
	jmp .vesa_find_video_mode_loop_modes_done

 .vesa_find_video_mode_next_mode:
	addl $2, %esi
	jmp .vesa_find_video_mode_loop_modes

 .vesa_find_video_mode_loop_modes_done:
	popl %esi
	popw %di
	popw %cx
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


# set mode found from vesa_find_video_mode. if no mode
# was found, set it to 80x25 text mode to clear the screen.
.global vesa_set_target_mode
vesa_set_target_mode:
	pushw %ax
	pushw %bx

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
