	.file	"main.c"
	.text
	.align	2
	.global	kerxit
	.type	kerxit, %function
kerxit:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	ldmfd	sp, {fp, sp, pc}
	.size	kerxit, .-kerxit
	.section	.rodata
	.align	2
.LC0:
	.ascii	"Task ending.\015\012\000"
	.align	2
.LC1:
	.ascii	"Task starting\015\012\000"
	.text
	.align	2
	.global	test
	.type	test, %function
test:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {sl, fp, ip, lr, pc}
	sub	fp, ip, #4
	ldr	sl, .L7
.L6:
	add	sl, pc, sl
.L4:
	mov	r0, #1
	ldr	r3, .L7+4
	add	r3, sl, r3
	mov	r1, r3
	bl	bwputstr(PLT)
	swi
	mov	r0, #1
	ldr	r3, .L7+8
	add	r3, sl, r3
	mov	r1, r3
	bl	bwputstr(PLT)
	b	.L4
.L8:
	.align	2
.L7:
	.word	_GLOBAL_OFFSET_TABLE_-(.L6+8)
	.word	.LC0(GOTOFF)
	.word	.LC1(GOTOFF)
	.size	test, .-test
	.section	.rodata
	.align	2
	.type	C.3.1171, %object
	.size	C.3.1171, 16
C.3.1171:
	.word	2
	.word	3
	.word	4
	.word	5
	.section	.data.rel.ro,"aw",%progbits
	.align	2
	.type	C.1.1169, %object
	.size	C.1.1169, 12
C.1.1169:
	.word	16
	.word	2207744
	.word	test
	.section	.rodata
	.align	2
.LC2:
	.ascii	"Page table base: \000"
	.align	2
.LC3:
	.ascii	"\015\012\000"
	.align	2
.LC4:
	.ascii	"Going into context switch\015\012\000"
	.align	2
.LC5:
	.ascii	"Got back from context switch sp=%x spsr=%x\015\012\000"
	.align	2
.LC6:
	.ascii	"Exiting normally\000"
	.text
	.align	2
	.global	main
	.type	main, %function
main:
	@ args = 0, pretend = 0, frame = 52
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {sl, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #52
	ldr	sl, .L15
.L14:
	add	sl, pc, sl
	str	r0, [fp, #-64]
	str	r1, [fp, #-68]
	mov	r3, #40
	str	r3, [fp, #-20]
	ldr	r3, .L15+4
	ldr	r3, [sl, r3]
	mov	r2, r3
	ldr	r3, [fp, #-20]
	str	r2, [r3, #0]
	ldr	r3, .L15+8
	ldr	r3, [sl, r3]
	sub	ip, fp, #44
	ldmia	r3, {r0, r1, r2}
	stmia	ip, {r0, r1, r2}
	ldr	r3, .L15+12
	str	r3, [fp, #-28]
	ldr	r3, .L15+16
	ldr	r3, [sl, r3]
	mov	r2, r3
	ldr	r3, [fp, #-28]
	str	r2, [r3, #0]
	ldr	r3, .L15+20
	ldr	r3, [sl, r3]
	sub	ip, fp, #60
	ldmia	r3, {r0, r1, r2, r3}
	stmia	ip, {r0, r1, r2, r3}
	ldr	r3, [fp, #-28]
	str	r3, [fp, #-32]
	mov	r0, #1
	mov	r1, #0
	bl	bwsetfifo(PLT)
	mov	r0, #1
	ldr	r3, .L15+24
	add	r3, sl, r3
	mov	r1, r3
	bl	bwputstr(PLT)
	ldr	r3, [fp, #-32]
	mov	r0, #1
	mov	r1, r3
	bl	bwputr(PLT)
	mov	r0, #1
	ldr	r3, .L15+28
	add	r3, sl, r3
	mov	r1, r3
	bl	bwputstr(PLT)
	ldr	r3, .L15+16
	ldr	r3, [sl, r3]
	mov	r0, #1
	mov	r1, r3
	bl	bwputr(PLT)
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L10
.L11:
	mov	r0, #1
	ldr	r3, .L15+32
	add	r3, sl, r3
	mov	r1, r3
	bl	bwputstr(PLT)
	sub	r3, fp, #44
	sub	r2, fp, #60
	mov	r0, r3
	mov	r1, r2
	bl	kerExit(PLT)
	ldr	r2, [fp, #-40]
	ldr	ip, [fp, #-44]
	mov	r0, #1
	ldr	r3, .L15+36
	add	r3, sl, r3
	mov	r1, r3
	mov	r3, ip
	bl	bwprintf(PLT)
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L10:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L11
	mov	r0, #1
	ldr	r3, .L15+40
	add	r3, sl, r3
	mov	r1, r3
	bl	bwputstr(PLT)
	mov	r3, #0
	mov	r0, r3
	sub	sp, fp, #16
	ldmfd	sp, {sl, fp, sp, pc}
.L16:
	.align	2
.L15:
	.word	_GLOBAL_OFFSET_TABLE_-(.L14+8)
	.word	kerEnt(GOT)
	.word	C.1.1169(GOT)
	.word	2207776
	.word	test(GOT)
	.word	C.3.1171(GOT)
	.word	.LC2(GOTOFF)
	.word	.LC3(GOTOFF)
	.word	.LC4(GOTOFF)
	.word	.LC5(GOTOFF)
	.word	.LC6(GOTOFF)
	.size	main, .-main
	.ident	"GCC: (GNU) 4.0.2"
