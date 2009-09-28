	.file	"switch.c"
	.text
	.align	2
	.global	kerEnt
	.type	kerEnt, %function
kerEnt:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	#; acquire the arguments
	#; get the request type
	ldr		r2, [lr, #-4]   
	#; save the active task's lr
	mov 	ip, lr
	#; switch to system mode
	mrs 	r3, CPSR 
	orr		r3, r3, #0x1f
	msr 	CPSR_c, r3
	#; overwrite lr
	mov	 	lr, ip
	#; save active task's state
	mov 	ip, sp
	stmfd	sp!, {r4-r9, sl, fp, lr}
	mov 	ip, sp
	#; switch to supervisor mode
	mrs 	r3, CPSR
	bic		r3, r3, #0x1f
	orr		r3, r3, #0x13
	msr 	CPSR_c, r3
	#; acquire the spc of the active task
	mrs 	r3, SPSR
	#; restore the kernel's registers holding the address of TD and Request
	ldmfd	sp!, {r4, r5}
	#; fill the request with the arguments
	stmia	r5, {r0, r1, r2}
	#; put the sp, and spsr into the active task
	stmia	r4, {r3, ip}
	#; restore the rest of kernel's registers
	#; jump back to the kernel -this returns to where kerExit would return to 
	#; if it were a normal function
	ldmfd	sp!, {r4-r9, sl, fp, pc}
	.size	kerEnt, .-kerEnt
	.align	2
	.global	kerExit
	.type	kerExit, %function
kerExit:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	#; store the kernel state
	stmfd	sp!, {r0, r1, r4-r9, sl, fp, lr}
	#; switch to system mode
	mrs 	r2, CPSR 
	orr		r2, r2, #0x1f
	msr 	CPSR_c, r2
	#; install the active tasks's sp and copy the spsr to ip
	ldmia 	r0, {ip, sp}
	#; restore the active task
	ldmfd	sp!, {r4-r9, sl, fp, lr}
	mov		r1, sp
	mov		r0, #1
	bl		bwputr(PLT)
	mov		r3, lr
	#; put the return value in r0
	ldmia 	r1, {r0}
	#; switch to supervisor mode
	mrs 	r2, CPSR 
	bic		r2, r2, #0x1f
	orr		r2, r2, #0x13
	msr 	CPSR_c, r2
	#; install the spsr
	msr 	SPSR_cxsf, ip
	#; install the pc of the active task
	movs 	pc, r3
	.size	kerExit, .-kerExit
	.ident	"GCC: (GNU) 4.0.2"
