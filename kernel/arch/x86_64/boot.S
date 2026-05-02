.set PG_PRESENT,	1<<0
.set PG_READ_WRITE,	1<<1
.set PG_PAGE_SIZE,	1<<7

.set FB_WIDTH,	800
.set FB_HEIGHT,	600
.set FB_BPP,	32

#define KERNEL_OFFSET 0xFFFFFFFF80000000
#define V2P(vaddr) ((vaddr) - KERNEL_OFFSET)

.code32

// custom addresses, video mode info, page align modules
.set multiboot_flags, (1 << 16) | (1 << 2) | (1 << 0)

.section .multiboot, "aw"
multiboot_start:
	.long 0x1BADB002
	.long multiboot_flags
	.long -(0x1BADB002 + multiboot_flags)

	.long V2P(multiboot_start)
	.long V2P(g_kernel_start)
	.long V2P(g_kernel_bss_start)
	.long V2P(g_kernel_end)
	.long V2P(_start)

	.long 0
	.long FB_WIDTH
	.long FB_HEIGHT
	.long FB_BPP
multiboot_end:

.section .multiboot2, "aw"
multiboot2_start:
	.long 0xE85250D6
	.long 0
	.long multiboot2_end - multiboot2_start
	.long -(0xE85250D6 + (multiboot2_end - multiboot2_start))

	# framebuffer tag
	.align 8
	.short 5
	.short 0
	.long 20
	.long FB_WIDTH
	.long FB_HEIGHT
	.long FB_BPP

	# legacy start
	.align 8
	.short 3
	.short 0
	.long 12
	.long V2P(_start)

	# page align modules
	.align 8
	.short 6
	.short 0
	.long 8

	.align 8
	.short 0
	.short 0
	.long 8
multiboot2_end:

.section .bananboot, "aw"
bananboot_start:
	.long 0xBABAB007
	.long -(0xBABAB007 + FB_WIDTH + FB_HEIGHT + FB_BPP)
	.long FB_WIDTH
	.long FB_HEIGHT
	.long FB_BPP
bananboot_end:

.section .bss, "aw", @nobits
    .global g_boot_stack_top
	g_boot_stack_bottom:
		.skip 4096 * 4
	g_boot_stack_top:

	.global g_kernel_cmdline
	g_kernel_cmdline:
		.skip 4096

	bootloader_magic:
		.skip 8
	bootloader_info:
		.skip 8

.section .data

# Map first GiB to 0x00000000 and 0xFFFFFFFF80000000
.align 4096
boot_pml4:
	.quad V2P(boot_pdpt_lo) + (PG_READ_WRITE | PG_PRESENT)
	.skip 510 * 8
	.quad V2P(boot_pdpt_hi) + (PG_READ_WRITE | PG_PRESENT)
boot_pdpt_lo:
	.quad V2P(boot_pd) + (PG_READ_WRITE | PG_PRESENT)
	.skip 511 * 8
boot_pdpt_hi:
	.skip 510 * 8
	.quad V2P(boot_pd) + (PG_READ_WRITE | PG_PRESENT)
	.skip 8
boot_pd:
	.set i, 0
	.rept 511
		.quad i + (PG_PAGE_SIZE | PG_READ_WRITE | PG_PRESENT)
		.set i, i + 0x200000
	.endr
	.quad V2P(g_boot_fast_page_pt) + (PG_READ_WRITE | PG_PRESENT)
.global g_boot_fast_page_pt
g_boot_fast_page_pt:
	.skip 512 * 8

boot_gdt:
	.quad 0x0000000000000000 # null descriptor
	.quad 0x00AF9A000000FFFF # kernel code
	.quad 0x00AF92000000FFFF # kernel data
boot_gdtr:
	.short . - boot_gdt - 1
	.quad V2P(boot_gdt)

.global g_ap_startup_done
g_ap_startup_done:
	.byte 0
.global g_ap_running_count
g_ap_running_count:
	.byte 0

.section .text

has_cpuid:
	pushfl
	pushfl
	xorl $0x00200000, (%esp)
	popfl
	pushfl
	popl %eax
	xorl (%esp), %eax
	popfl
	testl $0x00200000, %eax
	ret

is_64_bit:
	movl $0x80000000, %eax
	cpuid
	cmpl $0x80000001, %eax
	jl .no_extension
	movl $0x80000001, %eax
	cpuid
	testl $(1 << 29), %edx
	ret
.no_extension:
	cmpl %eax, %eax
	ret

check_requirements:
	call has_cpuid
	jz .exit
	call is_64_bit
	jz .exit
	ret
.exit:
	jmp system_halt

enable_sse:
	movl %cr0, %eax
	andw $0xFFFB, %ax
	orw $0x0002, %ax
	movl %eax, %cr0
	movl %cr4, %eax
	orw $0x0600, %ax
	movl %eax, %cr4
	ret

enable_tsc:
	# allow userspace to use RDTSC
	movl %cr4, %ecx
	andl $0xFFFFFFFB, %ecx
	movl %ecx, %cr4
	ret

initialize_paging:
	# enable PAE
	movl %cr4, %ecx
	orl $0x20, %ecx
	movl %ecx, %cr4

	# set long mode enable bit
	movl $0x100, %eax
	movl $0x000, %edx
	movl $0xC0000080, %ecx
	wrmsr

	# set address of paging structures
	movl $V2P(boot_pml4), %ecx
	movl %ecx, %cr3

	# enable paging
	movl %cr0, %ecx
	orl $0x80000000, %ecx
	movl %ecx, %cr0

	ret

.global _start
.type _start, @function
_start:
	cli; cld

	# Initialize stack and multiboot info
	movl %eax, V2P(bootloader_magic)
	movl %ebx, V2P(bootloader_info)

	movl $V2P(g_boot_stack_top), %esp

	call check_requirements
	call enable_sse
	call enable_tsc
	call initialize_paging

	# flush gdt and jump to 64 bit
	lgdt V2P(boot_gdtr)
	ljmpl $0x08, $V2P(long_mode)

.code64
long_mode:
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %ss
	movw %ax, %es

	# move stack pointer to higher half
	movl %esp, %esp
	addq $KERNEL_OFFSET, %rsp

	# jump to higher half
	movabsq $higher_half, %rcx
	jmp *%rcx

higher_half:
	# call global constuctors
	call _init

	movq $g_init_array_start, %rbx
	jmp 2f
 1: call *(%rbx)
	addq $8, %rbx
 2: cmpq $g_init_array_end, %rbx
	jne 1b

	# call to the kernel itself (clear rbp for stacktrace)
	xorq %rbp, %rbp

	movl V2P(bootloader_magic), %edi
	movl V2P(bootloader_info),  %esi
	call kernel_main

	# call global destructors
	call _fini

system_halt:
	xchgw %bx, %bx
	cli
1:	hlt
	jmp 1b


#define AP_REL(vaddr) ((vaddr) - ap_trampoline + 0xF000)

.section .ap_init, "ax"

.code16
.global ap_trampoline
ap_trampoline:
	jmp 1f

.align 8
ap_stack_paddr:
	.skip 8
ap_stack_vaddr:
	.skip 8
ap_prepare_paging:
	.skip 8
ap_page_table:
	.skip 8
ap_ready:
	.skip 8

1:	cli; cld
	ljmpl $0x00, $AP_REL(ap_cs_clear)

ap_cs_clear:
	# load ap gdt and enter protected mode
	lgdt AP_REL(ap_gdtr)
	movl %cr0, %eax
	orb $1, %al
	movl %eax, %cr0
	ljmpl $0x08, $AP_REL(ap_protected_mode)

.code32
ap_protected_mode:
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %ss
	movw %ax, %es

	movl AP_REL(ap_stack_paddr), %esp

	leal V2P(enable_sse),        %ecx; call *%ecx
	leal V2P(enable_tsc),        %ecx; call *%ecx
	leal V2P(initialize_paging), %ecx; call *%ecx

	# load boot gdt and enter long mode
	lgdt V2P(boot_gdtr)
	ljmpl $0x08, $AP_REL(ap_long_mode)

.code64
ap_long_mode:
	movq $ap_higher_half, %rax
	jmp *%rax

ap_higher_half:
	movq AP_REL(ap_prepare_paging), %rax
	call *%rax

	# load AP's initial values
	movq AP_REL(ap_stack_vaddr), %rsp
	movq AP_REL(ap_page_table), %rax
	movq $1, AP_REL(ap_ready)
	movq %rax, %cr3

	# clear rbp for stacktrace
	xorq %rbp, %rbp

1:	pause
	cmpb $0, g_ap_startup_done
	je 1b

	lock incb g_ap_running_count

	call ap_main
	jmp system_halt

ap_gdt:
	.quad 0x0000000000000000 # null descriptor
	.quad 0x00CF9A000000FFFF # 32 bit code
	.quad 0x00CF92000000FFFF # 32 bit data
ap_gdtr:
	.short . - ap_gdt - 1
	.quad ap_gdt
