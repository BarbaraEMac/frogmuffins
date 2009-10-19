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
	#; install the spsr
	msr 	SPSR_cxsf, r2
	#; put the return value in r0
	ldr		r0, [r0, #8]
	#; switch to system mode
	msr 	CPSR_c, #0xDF
	#; restore the active task NOTE: r0 is skipped
	add 	sp,	r3, #4
	#; ldmfd	sp, {r1-r12, lr}
	#; mov		r0, #1
	#; mov		r1, ip
	#; mrs		r1, SPSR
	#; bl		bwputr(PLT)
	ldmfd	sp!, {r1-r12, lr}
	#; switch to supervisor mode
	msr 	CPSR_c, #0xD3
	#; copy the pc to be restored
	mov		lr, ip
	#; switch to system mode
	msr 	CPSR_c, #0xDF
	#; restore the ip
	ldmfd	sp!, {ip}
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
	#; copy the lr, NOTE: ip is scratch
	mov 	ip, lr
	#; switch to system mode
	msr 	CPSR_c, #0xDF
	#; save active task's state 
	#; NOTE: ip holds the pc to restore
	#; NOTE2:pc is where the ip would be save to if we were doing an
	#; 		 interrupt, but ip is scratch in swi so we don't care
	stmfd	sp!, {r0-r12, lr, pc}
	#; temporarily store the user's sp
	mov 	ip, sp
	#; switch to supervisor mode
	msr 	CPSR_c, #0xD3
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
	#; save the active task's lr
	#; switch to system mode
	msr 	CPSR_c, #0xDF
	#; store the ip
	stmfd	sp!, {ip}
	#; switch back to interrupt mode
	msr 	CPSR_c, #0xD2
	#; copy the lr
	#; NOTE: we just made ip scratch
	#; NOTE2: lr is 4 bytes ahead of what we need to return it to
	sub 	ip, lr, #4
	#; switch to system mode
	msr 	CPSR_c, #0xDF
	#; save active task's state 
	#; NOTE: ip holds the pc to restore
	stmfd	sp!, {r0-r12, lr}
	#; temporarily store the user's sp
	mov 	ip, sp
	#; switch back to interrupt mode to get the spsr
	msr 	CPSR_c, #0xD2
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
