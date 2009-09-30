	.file	"switch.c"
	.text
	.align	2
	.global	kernelEnter
	.type	kernelEnter, %function
kernelEnter:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	#; acquire the arguments
	#; save the active task's lr
	mov 	ip, lr
	#; switch to system mode
	msr 	CPSR_c, #0xDF
	#; save active task's state 
	#; NOTE: ip holds the place to return to
	stmfd	sp!, {r4-r9, sl, fp, ip, lr}
	mov 	ip, sp
	#; switch to supervisor mode
	msr 	CPSR_c, #0xD3
	#; restore the kernel's registers holding the address of TD and Request
	ldmfd	sp!, {r4, r5}
	#; get the request type note lr is the svc's lr
	ldr		r6, [lr, #-4]   
	#; this line is MAGIC (I think the assembler is really smart)
	bic		r6,	r6, #0xFF000000
	#; fill the request with the arguments
	stmia	r5, {r0, r1, r2, r3, r6}
	#; put the sp, and spsr into the active task
	mrs 	r3, SPSR
	stmia	r4, {r3, ip}
	#; restore the rest of kernel's registers
	#; jump back to the kernel -this returns to where kernelExit would return to 
	#; if it were a normal function
	ldmfd	sp!, {r4-r9, sl, fp, pc}
	.size	kernelEnter, .-kernelEnter
	.align	2
	.global	kernelExit
	.type	kernelExit, %function
kernelExit:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	#; store the kernel state
	stmfd	sp!, {r0, r1, r4-r9, sl, fp, lr}
	#; switch to system mode
	msr 	CPSR_c, #0xDF
	#; install the active tasks's sp and copy the spsr to ip and
	ldmia 	r0, {r3, sp}
	#; put the return value in r0
	ldr		r0, [r0, #8]
	#; restore the active task
	ldmfd	sp!, {r4-r9, sl, fp, ip, lr}
	#; switch to supervisor mode
	msr 	CPSR_c, #0xD3
	#; install the spsr
	msr 	SPSR_cxsf, r3
	#; install the pc of the active task
	movs 	pc, ip
	.size	kernelExit, .-kernelExit
	.align	2
	.global	syscall
	.type	syscall, %function
syscall:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	#; set the first byte of r3 to be swi
	bic		r3,	r3, #0xFF000000
	orr		r3, r3, #0xEF000000
	#; put the changes in the next instruction
	str 	r3, [pc, #-4]
	swi	
	mov		pc, lr
	.size	syscall, .-syscall
	.ident	"GCC: (GNU) 4.0.2"
