/* i686 crti.s */
.section .init
.global _init
.type _init, @function
_init:
	pushl %ebp
	movl %esp, %ebp
	/* gcc will nicely put the contents of crtbegin.o's .init section here. */

.section .fini
.global _fini
.type _fini, @function
_fini:
	pushl %ebp
	movl %esp, %ebp
	/* gcc will nicely put the contents of crtbegin.o's .fini section here. */

.section .init_array
.global __init_array_start
__init_array_start:

.section .fini_array
.global __fini_array_start
__fini_array_start:
