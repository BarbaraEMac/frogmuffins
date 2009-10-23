	.file	"switch.c"
	.text
	.align	2
	.global	kernelExit
	.type	kernelExit, %function
kernelExit:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	#; store the kernel state
	stmfd	sp!, {r0, r1, r4-r10, fp, lr}
	#; fetch the active tasks's sp and spsr
	ldmia 	r0, {r2, r3}
	#; read the addres tp return to
	ldr		lr, [r3, #56]
	#; install the spsr
	msr 	SPSR_cxsf, r2
	#; switch to system mode
	msr 	CPSR_c, #0xDF
	#; restore the active task
	mov 	sp,	r3
	#; ldmfd	sp, {r1-r12, lr}
	#; mov		r0, #1
	#; ldr		r1, [r3, #56]
	#; mov		r1, ip
	#; mrs		r1, SPSR
	#; bl		bwputr(PLT)
	ldmfd	sp!, {r0-r12, lr}
	#; we didn't read the pc, so add to the sp
	add		sp, sp, #4
	#; switch to supervisor mode
	msr 	CPSR_c, #0xD3
	#; install the pc of the active task
	movs 	pc, lr
	.size	kernelExit, .-kernelExit
	.align	2
	.global	kernelEnter
	.type	kernelEnter, %function
kernelEnter:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	#; switch to system mode
	msr 	CPSR_c, #0xDF
	#; save active task's state 
	#; NOTE: pc is useless for now but will be overwritten shortly
	stmfd	sp!, {r0-r12, lr, pc}
	#; temporarily store the user's sp
	mov 	ip, sp
	#; switch to back supervisor mode
	msr 	CPSR_c, #0xD3
	#; store the actual pc we want to store (on the user stack)
	str		lr, [ip, #56]
	#; restore the kernel's registers holding the address of TD and Request
	ldmfd	sp!, {r0, r1}
	#; put the sp, and spsr into the active task
	mrs 	r6, SPSR
	stmia	r0, {r6, ip}
	#; get the request type.
	#; NOTE: lr is the svc's lr
	ldr		r6, [lr, #-4]   
	#; this line is MAGIC (I think the assembler is really smart)
	bic		r6,	r6, #0xFF000000
	#; fill the request with the arguments
	stmia	r1, {r6, ip}
	#; restore the rest of kernel's registers
	#; jump back to the kernel -this returns to where kernelExit 
	#; would return to if it was a normal function
	ldmfd	sp!, {r4-r10, fp, pc}
	.size	kernelEnter, .-kernelEnter
	.align	2
	.global	interruptHandler
	.type	interruptHandler, %function
interruptHandler:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	#; switch to system mode
	msr 	CPSR_c, #0xDF
	#; save active task's state 
	#; NOTE: pc is useless for now but will be overwritten shortly
	stmfd	sp!, {r0-r12, lr, pc}
	#; temporarily store the user's sp
	mov 	ip, sp
	#; switch back to interrupt mode
	msr 	CPSR_c, #0xD2
	#; store the actual pc we want to store (on the user stack)
	sub 	lr, lr, #4
	str		lr, [ip, #56]
	#; get the spsr
	mrs 	r6, SPSR
	#; switch to supervisor mode
	msr 	CPSR_c, #0xD3
	#; restore the kernel's registers holding the address of TD and Request
	ldmfd	sp!, {r0, r1}
	#; put the sp, and spsr into the active task
	stmia	r0, {r6, ip}
	#; get the interrupt type
	mov		r6, #99
	#; fill the request with the arguments
	stmia	r1, {r6, r7}
	#; restore the rest of kernel's registers
	#; jump back to the kernel -this returns to where kernelExit 
	#; would return to if it was a normal function
	ldmfd	sp!, {r4-r10, fp, pc}
	.size	interruptHandler, .-interruptHandler
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
