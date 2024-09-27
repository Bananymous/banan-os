.macro push_userspace
	pushw %gs
	pushw %fs
	pushw %es
	pushw %ds
	pushal
.endm

.macro load_kernel_segments
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs

	movw $0x28, %ax
	movw %ax, %gs
.endm

.macro pop_userspace
	popal
	popw %ds
	popw %es
	popw %fs
	popw %gs
.endm

isr_stub:
	push_userspace
	load_kernel_segments

	movl %cr0, %eax; pushl %eax
	movl %cr2, %eax; pushl %eax
	movl %cr3, %eax; pushl %eax
	movl %cr4, %eax; pushl %eax

	movl %esp,     %eax // register ptr
	leal 64(%esp), %ebx // interrupt stack ptr
	movl 60(%esp), %ecx // error code
	movl 56(%esp), %edx // isr number

	movl %esp, %ebp
	subl $15, %esp
	andl $0xFFFFFFF0, %esp

	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	call cpp_isr_handler

	movl %ebp, %esp
	addl $16, %esp

	pop_userspace
	addl $8, %esp
	iret

irq_stub:
	push_userspace
	load_kernel_segments

	movl 40(%esp), %eax # interrupt number

	movl %esp, %ebp
	subl $15, %esp
	andl $0xFFFFFFF0, %esp

	subl $12, %esp
	pushl %eax
	call cpp_irq_handler

	movl %ebp, %esp

	pop_userspace
	addl $8, %esp
	iret

.global asm_yield_handler
asm_yield_handler:
	# This can only be called from kernel, so no segment saving is needed
	pushal

	movl %esp,     %eax # interrupt registers ptr
	leal 32(%esp), %ebx # interrupt stack ptr

	movl %esp, %ebp
	subl $15, %esp
	andl $0xFFFFFFF0, %esp

	subl $8, %esp
	pushl %eax
	pushl %ebx
	call cpp_yield_handler

	movl %ebp, %esp

	popal
	iret

.global asm_ipi_handler
asm_ipi_handler:
	push_userspace
	load_kernel_segments

	movl %esp, %ebp
	subl $15, %esp
	andl $0xFFFFFFF0, %esp

	call cpp_ipi_handler

	movl %ebp, %esp

	pop_userspace
	iret


.global asm_timer_handler
asm_timer_handler:
	push_userspace
	load_kernel_segments

	movl %esp, %ebp
	subl $15, %esp
	andl $0xFFFFFFF0, %esp

	call cpp_timer_handler

	movl %ebp, %esp

	pop_userspace
	iret

.macro isr n
	.global isr\n
	isr\n:
		pushl $0
		pushl $\n
		jmp isr_stub
.endm

.macro isr_err n
	.global isr\n
	isr\n:
		pushl $\n
		jmp isr_stub
.endm

.macro irq n
	.global irq\n
	irq\n:
		pushl $0
		pushl $\n
		jmp irq_stub
.endm

isr 0
isr 1
isr 2
isr 3
isr 4
isr 5
isr 6
isr 7
isr_err 8
isr 9
isr_err 10
isr_err 11
isr_err 12
isr_err 13
isr_err 14
isr 15
isr 16
isr_err 17
isr 18
isr 19
isr 20
isr 21
isr 22
isr 23
isr 24
isr 25
isr 26
isr 27
isr 28
isr 29
isr 30
isr 31

irq 0
irq 1
irq 2
irq 3
irq 4
irq 5
irq 6
irq 7
irq 8
irq 9
irq 10
irq 11
irq 12
irq 13
irq 14
irq 15
irq 16
irq 17
irq 18
irq 19
irq 20
irq 21
irq 22
irq 23
irq 24
irq 25
irq 26
irq 27
irq 28
irq 29
irq 30
irq 31
irq 32
irq 33
irq 34
irq 35
irq 36
irq 37
irq 38
irq 39
irq 40
irq 41
irq 42
irq 43
irq 44
irq 45
irq 46
irq 47
irq 48
irq 49
irq 50
irq 51
irq 52
irq 53
irq 54
irq 55
irq 56
irq 57
irq 58
irq 59
irq 60
irq 61
irq 62
irq 63
irq 64
irq 65
irq 66
irq 67
irq 68
irq 69
irq 70
irq 71
irq 72
irq 73
irq 74
irq 75
irq 76
irq 77
irq 78
irq 79
irq 80
irq 81
irq 82
irq 83
irq 84
irq 85
irq 86
irq 87
irq 88
irq 89
irq 90
irq 91
irq 92
irq 93
irq 94
irq 95
irq 96
irq 97
irq 98
irq 99
irq 100
irq 101
irq 102
irq 103
irq 104
irq 105
irq 106
irq 107
irq 108
irq 109
irq 110
irq 111
irq 112
irq 113
irq 114
irq 115
irq 116
irq 117
irq 118
irq 119
irq 120
irq 121
irq 122
irq 123
irq 124
irq 125
irq 126
irq 127
irq 128
irq 129
irq 130
irq 131
irq 132
irq 133
irq 134
irq 135
irq 136
irq 137
irq 138
irq 139
irq 140
irq 141
irq 142
irq 143
irq 144
irq 145
irq 146
irq 147
irq 148
irq 149
irq 150
irq 151
irq 152
irq 153
irq 154
irq 155
irq 156
irq 157
irq 158
irq 159
irq 160
irq 161
irq 162
irq 163
irq 164
irq 165
irq 166
irq 167
irq 168
irq 169
irq 170
irq 171
irq 172
irq 173
irq 174
irq 175
irq 176
irq 177
irq 178
irq 179
irq 180
irq 181
irq 182
irq 183
irq 184
irq 185
irq 186
irq 187
irq 188
irq 189
irq 190
irq 191
irq 192
irq 193
irq 194
irq 195
irq 196
irq 197
irq 198
irq 199
irq 200
irq 201
irq 202
irq 203
irq 204
irq 205
irq 206
irq 207
