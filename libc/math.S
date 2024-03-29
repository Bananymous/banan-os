
.global floorl;
floorl:
	fldt	8(%rsp)

	fnstenv	-28(%rsp)		/* store fpu environment */

	/* We use here %edx although only the low 1 bits are defined.
	  But none of the operations should care and they are faster
	  than the 16 bit operations.  */
	movl	$0x400,%edx		/* round towards -oo */
	orl	-28(%rsp),%edx
	andl	$0xf7ff,%edx
	movl	%edx,-32(%rsp)
	fldcw	-32(%rsp)		/* load modified control word */

	frndint				/* round */

	/* Preserve "invalid" exceptions from sNaN input.  */
	fnstsw
	andl	$0x1, %eax
	orl	%eax, -24(%rsp)

	fldenv	-28(%rsp)		/* restore original environment */

	ret