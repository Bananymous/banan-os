/* x86-64 crtn.s */
.section .init
	/* gcc will nicely put the contents of crtend.o's .init section here. */
	popq %rbp
	ret

.section .fini
	/* gcc will nicely put the contents of crtend.o's .fini section here. */
	popq %rbp
	ret

.section .init_array
.global __init_array_end
__init_array_end:

.section .fini_array
.global __fini_array_end
__fini_array_end:
