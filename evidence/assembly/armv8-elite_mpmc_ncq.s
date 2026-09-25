	.text
	.file	"elite_mpmc_ncq.c"
	.globl	el_ncq_enqueue                  // -- Begin function el_ncq_enqueue
	.p2align	2
	.type	el_ncq_enqueue,@function
el_ncq_enqueue:                         // @el_ncq_enqueue
// %bb.0:
	stp	x29, x30, [sp, #-16]!           // 16-byte Folded Spill
	mov	w8, #1152                       // =0x480
	ldr	x9, [x0, #552]
	tst	w1, #0x1
	mov	w10, #896                       // =0x380
	mov	w11, #600                       // =0x258
	mov	w12, #592                       // =0x250
	csel	x8, x10, x8, ne
	csel	x10, x12, x11, ne
	ldr	x11, [x0, #64]
	add	x8, x9, x8
	ldr	x10, [x0, x10]
	ldr	x12, [x0, #144]
	ldar	x9, [x8]
	mov	x29, sp
	cmp	x9, x12
	b.hs	.LBB0_16
// %bb.1:
	sub	x13, x11, #1
	neg	x14, x11
	b	.LBB0_4
.LBB0_2:                                //   in Loop: Header=BB0_4 Depth=1
	clrex
.LBB0_3:                                //   in Loop: Header=BB0_4 Depth=1
	ldar	x9, [x8]
	cmp	x9, x12
	b.hs	.LBB0_16
.LBB0_4:                                // =>This Loop Header: Depth=1
                                        //     Child Loop BB0_11 Depth 2
                                        //     Child Loop BB0_6 Depth 2
	and	x15, x9, x13
	and	x17, x9, x14
	add	x15, x10, x15, lsl #7
	ldar	x16, [x15]
	and	x18, x16, x14
	cmp	x18, x17
	b.ne	.LBB0_8
// %bb.5:                               //   in Loop: Header=BB0_4 Depth=1
	add	x15, x9, #1
.LBB0_6:                                //   Parent Loop BB0_4 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	ldaxr	x16, [x8]
	cmp	x16, x9
	b.ne	.LBB0_2
// %bb.7:                               //   in Loop: Header=BB0_6 Depth=2
	stlxr	w16, x15, [x8]
	cbnz	w16, .LBB0_6
	b	.LBB0_3
.LBB0_8:                                //   in Loop: Header=BB0_4 Depth=1
	subs	x3, x17, x11
	b.lo	.LBB0_3
// %bb.9:                               //   in Loop: Header=BB0_4 Depth=1
	cmp	x18, x3
	b.ne	.LBB0_3
// %bb.10:                              //   in Loop: Header=BB0_4 Depth=1
	orr	x17, x17, x2
.LBB0_11:                               //   Parent Loop BB0_4 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	ldaxr	x18, [x15]
	cmp	x18, x16
	b.ne	.LBB0_2
// %bb.12:                              //   in Loop: Header=BB0_11 Depth=2
	stlxr	w18, x17, [x15]
	cbnz	w18, .LBB0_11
// %bb.13:
	add	x10, x9, #1
.LBB0_14:                               // =>This Inner Loop Header: Depth=1
	ldaxr	x11, [x8]
	cmp	x11, x9
	b.ne	.LBB0_17
// %bb.15:                              //   in Loop: Header=BB0_14 Depth=1
	stlxr	w11, x10, [x8]
	cbnz	w11, .LBB0_14
	b	.LBB0_18
.LBB0_16:
	mov	w1, #9                          // =0x9
	mov	w2, #2                          // =0x2
	bl	el_fail
	ldp	x29, x30, [sp], #16             // 16-byte Folded Reload
	ret
.LBB0_17:
	clrex
.LBB0_18:
	mov	x0, xzr
	tst	w1, #0x1
	mov	x8, #8589934592                 // =0x200000000
	mov	x9, #17179869184                // =0x400000000
	mov	x1, x0
	csel	x0, x9, x8, ne
	ldp	x29, x30, [sp], #16             // 16-byte Folded Reload
	ret
.Lfunc_end0:
	.size	el_ncq_enqueue, .Lfunc_end0-el_ncq_enqueue
                                        // -- End function
	.globl	el_ncq_dequeue                  // -- Begin function el_ncq_dequeue
	.p2align	2
	.type	el_ncq_dequeue,@function
el_ncq_dequeue:                         // @el_ncq_dequeue
// %bb.0:
	stp	x29, x30, [sp, #-16]!           // 16-byte Folded Spill
	mov	w8, #1024                       // =0x400
	ldr	x9, [x0, #552]
	tst	w1, #0x1
	mov	w10, #768                       // =0x300
	mov	w11, #600                       // =0x258
	mov	w12, #592                       // =0x250
	csel	x8, x10, x8, ne
	csel	x10, x12, x11, ne
	ldr	x11, [x0, #144]
	add	x8, x9, x8
	ldr	x9, [x0, x10]
	ldr	x10, [x0, #64]
	ldar	x14, [x8]
	mov	x29, sp
	cmp	x14, x11
	b.ls	.LBB1_2
.LBB1_1:
	mov	w1, #10                         // =0xa
	mov	w2, #4                          // =0x4
	bl	el_fail
	ldp	x29, x30, [sp], #16             // 16-byte Folded Reload
	ret
.LBB1_2:
	sub	x12, x10, #1
	neg	x13, x10
	b	.LBB1_5
.LBB1_3:                                //   in Loop: Header=BB1_5 Depth=1
	clrex
.LBB1_4:                                //   in Loop: Header=BB1_5 Depth=1
	ldar	x14, [x8]
	cmp	x14, x11
	b.hi	.LBB1_1
.LBB1_5:                                // =>This Loop Header: Depth=1
                                        //     Child Loop BB1_8 Depth 2
	and	x15, x14, x12
	and	x17, x14, x13
	add	x15, x9, x15, lsl #7
	ldar	x15, [x15]
	and	x16, x15, x13
	cmp	x16, x17
	b.ne	.LBB1_10
// %bb.6:                               //   in Loop: Header=BB1_5 Depth=1
	cmp	x14, x11
	b.hs	.LBB1_14
// %bb.7:                               //   in Loop: Header=BB1_5 Depth=1
	add	x16, x14, #1
.LBB1_8:                                //   Parent Loop BB1_5 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	ldaxr	x17, [x8]
	cmp	x17, x14
	b.ne	.LBB1_3
// %bb.9:                               //   in Loop: Header=BB1_8 Depth=2
	stlxr	w17, x16, [x8]
	cbnz	w17, .LBB1_8
	b	.LBB1_13
.LBB1_10:                               //   in Loop: Header=BB1_5 Depth=1
	subs	x14, x17, x10
	b.lo	.LBB1_4
// %bb.11:                              //   in Loop: Header=BB1_5 Depth=1
	cmp	x16, x14
	b.ne	.LBB1_4
// %bb.12:
	mov	x0, xzr
	tst	w1, #0x1
	mov	w8, #1                          // =0x1
	mov	x1, x0
	cinc	x0, x8, ne
	ldp	x29, x30, [sp], #16             // 16-byte Folded Reload
	ret
.LBB1_13:
	and	x8, x15, x12
	mov	x0, xzr
	mov	x1, xzr
	str	x8, [x2]
	ldp	x29, x30, [sp], #16             // 16-byte Folded Reload
	ret
.LBB1_14:
	mov	w1, #9                          // =0x9
	mov	w2, #2                          // =0x2
	bl	el_fail
	ldp	x29, x30, [sp], #16             // 16-byte Folded Reload
	ret
.Lfunc_end1:
	.size	el_ncq_dequeue, .Lfunc_end1-el_ncq_dequeue
                                        // -- End function
	.globl	el_ncq_reserve                  // -- Begin function el_ncq_reserve
	.p2align	2
	.type	el_ncq_reserve,@function
el_ncq_reserve:                         // @el_ncq_reserve
// %bb.0:
	stp	x29, x30, [sp, #-80]!           // 16-byte Folded Spill
	ldr	x8, [x0, #552]
	str	x25, [sp, #16]                  // 8-byte Folded Spill
	mov	x29, sp
	stp	x24, x23, [sp, #32]             // 16-byte Folded Spill
	ldr	x9, [x0, #592]
	ldr	x10, [x0, #64]
	stp	x22, x21, [sp, #48]             // 16-byte Folded Spill
	add	x8, x8, #768
	mov	x21, x1
	stp	x20, x19, [sp, #64]             // 16-byte Folded Spill
	ldr	x11, [x0, #144]
	mov	x20, x2
	ldar	x14, [x8]
	mov	x19, x0
	cmp	x14, x11
	b.ls	.LBB2_3
.LBB2_1:
	mov	x0, x19
	mov	w1, #10                         // =0xa
	mov	w2, #4                          // =0x4
	bl	el_fail
	cbz	w0, .LBB2_16
.LBB2_2:
	ldp	x20, x19, [sp, #64]             // 16-byte Folded Reload
	ldr	x25, [sp, #16]                  // 8-byte Folded Reload
	ldp	x22, x21, [sp, #48]             // 16-byte Folded Reload
	ldp	x24, x23, [sp, #32]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #80             // 16-byte Folded Reload
	ret
.LBB2_3:
	sub	x12, x10, #1
	neg	x13, x10
	mov	w0, #2                          // =0x2
	b	.LBB2_6
.LBB2_4:                                //   in Loop: Header=BB2_6 Depth=1
	clrex
.LBB2_5:                                //   in Loop: Header=BB2_6 Depth=1
	ldar	x14, [x8]
	cmp	x14, x11
	b.hi	.LBB2_1
.LBB2_6:                                // =>This Loop Header: Depth=1
                                        //     Child Loop BB2_9 Depth 2
	and	x15, x14, x12
	and	x17, x14, x13
	add	x15, x9, x15, lsl #7
	ldar	x15, [x15]
	and	x16, x15, x13
	cmp	x16, x17
	b.ne	.LBB2_11
// %bb.7:                               //   in Loop: Header=BB2_6 Depth=1
	cmp	x14, x11
	b.hs	.LBB2_15
// %bb.8:                               //   in Loop: Header=BB2_6 Depth=1
	add	x16, x14, #1
.LBB2_9:                                //   Parent Loop BB2_6 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	ldaxr	x17, [x8]
	cmp	x17, x14
	b.ne	.LBB2_4
// %bb.10:                              //   in Loop: Header=BB2_9 Depth=2
	stlxr	w17, x16, [x8]
	cbnz	w17, .LBB2_9
	b	.LBB2_14
.LBB2_11:                               //   in Loop: Header=BB2_6 Depth=1
	subs	x14, x17, x10
	b.lo	.LBB2_5
// %bb.12:                              //   in Loop: Header=BB2_6 Depth=1
	cmp	x16, x14
	b.ne	.LBB2_5
// %bb.13:
	mov	x1, xzr
	ldp	x20, x19, [sp, #64]             // 16-byte Folded Reload
	ldr	x25, [sp, #16]                  // 8-byte Folded Reload
	ldp	x22, x21, [sp, #48]             // 16-byte Folded Reload
	ldp	x24, x23, [sp, #32]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #80             // 16-byte Folded Reload
	ret
.LBB2_14:
	and	x22, x15, x12
	b	.LBB2_17
.LBB2_15:
	mov	x0, x19
	mov	w1, #9                          // =0x9
	mov	w2, #2                          // =0x2
	bl	el_fail
	cbnz	w0, .LBB2_2
.LBB2_16:
	mov	x22, xzr
.LBB2_17:
	mov	x0, x19
	mov	x1, x22
	mov	x2, xzr
	mov	w3, #1                          // =0x1
	bl	el_begin_token
	add	x24, x19, #616
	ldr	q0, [x24]
	str	q0, [x21]
	ldp	q1, q0, [x24, #48]
	ldp	q2, q3, [x24, #16]
	stp	q1, q0, [x21, #48]
	stp	q2, q3, [x21, #16]
	ldr	x8, [x19, #584]
	add	x25, x8, x22, lsl #7
	add	x8, x25, #8
	ldar	x8, [x8]
	tst	x8, #0x3
	b.ne	.LBB2_20
// %bb.18:
	lsr	x8, x8, #2
	ldr	x9, [x25]
	cmp	x9, x8
	b.ne	.LBB2_20
// %bb.19:
	ldr	x9, [x19, #152]
	cmp	x8, x9
	b.ls	.LBB2_22
.LBB2_20:
	mov	x0, x19
	mov	w1, #10                         // =0xa
	mov	w2, #1                          // =0x1
.LBB2_21:
	bl	el_fail
	ldp	x20, x19, [sp, #64]             // 16-byte Folded Reload
	ldr	x25, [sp, #16]                  // 8-byte Folded Reload
	ldp	x22, x21, [sp, #48]             // 16-byte Folded Reload
	ldp	x24, x23, [sp, #32]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #80             // 16-byte Folded Reload
	ret
.LBB2_22:
	b.hs	.LBB2_24
// %bb.23:
	add	x23, x8, #1
	mov	x0, x19
	mov	x1, x23
	str	x23, [x25], #8
	bl	el_set_epoch
	ldr	q0, [x24]
	mov	w8, #1                          // =0x1
	mov	x1, xzr
	orr	x8, x8, x23, lsl #2
	mov	x0, #4294967296                 // =0x100000000
	str	q0, [x21]
	ldp	q1, q0, [x24, #48]
	ldp	q2, q3, [x24, #16]
	stp	q1, q0, [x21, #48]
	stp	q2, q3, [x21, #16]
	stlr	x8, [x25]
	ldr	x8, [x19, #88]
	ldr	x9, [x19, #608]
	madd	x8, x8, x22, x9
	ldr	w9, [x19, #72]
	str	w9, [x20, #8]
	str	x8, [x20]
	ldp	x20, x19, [sp, #64]             // 16-byte Folded Reload
	ldr	x25, [sp, #16]                  // 8-byte Folded Reload
	ldp	x22, x21, [sp, #48]             // 16-byte Folded Reload
	ldp	x24, x23, [sp, #32]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #80             // 16-byte Folded Reload
	ret
.LBB2_24:
	mov	x0, x19
	mov	w1, #9                          // =0x9
	mov	w2, #2                          // =0x2
	b	.LBB2_21
.Lfunc_end2:
	.size	el_ncq_reserve, .Lfunc_end2-el_ncq_reserve
                                        // -- End function
	.globl	el_ncq_commit                   // -- Begin function el_ncq_commit
	.p2align	2
	.type	el_ncq_commit,@function
el_ncq_commit:                          // @el_ncq_commit
// %bb.0:
	stp	x29, x30, [sp, #-32]!           // 16-byte Folded Spill
	ldr	x8, [x0, #584]
	ldr	x9, [x0, #720]
	mov	x29, sp
	stp	x20, x19, [sp, #16]             // 16-byte Folded Spill
	add	x19, x8, x9, lsl #7
	ldr	x9, [x0, #728]
	ldr	x8, [x19]
	cmp	x8, x9
	b.ne	.LBB3_4
// %bb.1:
	add	x8, x19, #8
	mov	w10, #1                         // =0x1
	ldar	x9, [x8]
	ldr	x8, [x0, #728]
	bfi	x10, x8, #2, #62
	cmp	x9, x10
	b.ne	.LBB3_4
// %bb.2:
	ldr	w9, [x0, #48]
	stp	w1, w2, [x19, #16]
	str	x3, [x19, #32]
	cmp	w9, #1
	b.ne	.LBB3_6
// %bb.3:
	ldr	x8, [x0, #720]
	ldr	x9, [x0, #88]
	mov	w1, w1
	ldr	x10, [x0, #608]
	mov	x20, x0
	madd	x8, x9, x8, x10
	mov	x0, x8
	bl	elite_crc64
	ldr	x8, [x20, #728]
	lsl	x9, x8, #2
	mov	x8, x0
	mov	x0, x20
	b	.LBB3_7
.LBB3_4:
	mov	w1, #10                         // =0xa
	mov	w2, #1                          // =0x1
	bl	el_fail
	mov	x19, x0
	lsr	x8, x0, #32
.LBB3_5:
	bfi	x19, x8, #32, #32
	mov	x0, x19
	ldp	x20, x19, [sp, #16]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #32             // 16-byte Folded Reload
	ret
.LBB3_6:
	lsl	x9, x8, #2
	mov	x8, xzr
.LBB3_7:
	str	x8, [x19, #24]
	orr	x8, x9, #0x2
	add	x9, x19, #8
	stlr	x8, [x9]
	ldr	x8, [x0, #552]
	ldr	x10, [x0, #720]
	ldr	x11, [x0, #600]
	ldr	x12, [x0, #64]
	ldr	x13, [x0, #144]
	add	x8, x8, #1152
	ldar	x9, [x8]
	cmp	x9, x13
	b.hs	.LBB3_24
// %bb.8:
	sub	x14, x12, #1
	neg	x15, x12
	b	.LBB3_11
.LBB3_9:                                //   in Loop: Header=BB3_11 Depth=1
	clrex
.LBB3_10:                               //   in Loop: Header=BB3_11 Depth=1
	ldar	x9, [x8]
	cmp	x9, x13
	b.hs	.LBB3_24
.LBB3_11:                               // =>This Loop Header: Depth=1
                                        //     Child Loop BB3_18 Depth 2
                                        //     Child Loop BB3_13 Depth 2
	and	x16, x9, x14
	and	x18, x9, x15
	add	x16, x11, x16, lsl #7
	ldar	x17, [x16]
	and	x1, x17, x15
	cmp	x1, x18
	b.ne	.LBB3_15
// %bb.12:                              //   in Loop: Header=BB3_11 Depth=1
	add	x16, x9, #1
.LBB3_13:                               //   Parent Loop BB3_11 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	ldaxr	x17, [x8]
	cmp	x17, x9
	b.ne	.LBB3_9
// %bb.14:                              //   in Loop: Header=BB3_13 Depth=2
	stlxr	w17, x16, [x8]
	cbnz	w17, .LBB3_13
	b	.LBB3_10
.LBB3_15:                               //   in Loop: Header=BB3_11 Depth=1
	subs	x2, x18, x12
	b.lo	.LBB3_10
// %bb.16:                              //   in Loop: Header=BB3_11 Depth=1
	cmp	x1, x2
	b.ne	.LBB3_10
// %bb.17:                              //   in Loop: Header=BB3_11 Depth=1
	orr	x18, x18, x10
.LBB3_18:                               //   Parent Loop BB3_11 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	ldaxr	x1, [x16]
	cmp	x1, x17
	b.ne	.LBB3_9
// %bb.19:                              //   in Loop: Header=BB3_18 Depth=2
	stlxr	w1, x18, [x16]
	cbnz	w1, .LBB3_18
// %bb.20:
	add	x10, x9, #1
.LBB3_21:                               // =>This Inner Loop Header: Depth=1
	ldaxr	x11, [x8]
	cmp	x11, x9
	b.ne	.LBB3_25
// %bb.22:                              //   in Loop: Header=BB3_21 Depth=1
	stlxr	w11, x10, [x8]
	cbnz	w11, .LBB3_21
// %bb.23:
	mov	x1, xzr
	b	.LBB3_26
.LBB3_24:
	mov	w1, #9                          // =0x9
	mov	w2, #2                          // =0x2
	mov	x20, x0
	bl	el_fail
	mov	x19, x0
	mov	x0, x20
	lsr	x8, x19, #32
	cmp	x8, #2
	b.ne	.LBB3_5
	b	.LBB3_27
.LBB3_25:
	mov	x1, xzr
	clrex
.LBB3_26:
	mov	x19, #8589934592                // =0x200000000
.LBB3_27:
	mov	x20, x1
	bl	el_end_token
	mov	x1, x20
	mov	w8, #2                          // =0x2
	bfi	x19, x8, #32, #32
	mov	x0, x19
	ldp	x20, x19, [sp, #16]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #32             // 16-byte Folded Reload
	ret
.Lfunc_end3:
	.size	el_ncq_commit, .Lfunc_end3-el_ncq_commit
                                        // -- End function
	.globl	el_ncq_borrow                   // -- Begin function el_ncq_borrow
	.p2align	2
	.type	el_ncq_borrow,@function
el_ncq_borrow:                          // @el_ncq_borrow
// %bb.0:
	stp	x29, x30, [sp, #-48]!           // 16-byte Folded Spill
	ldr	x8, [x0, #552]
	str	x21, [sp, #16]                  // 8-byte Folded Spill
	mov	x21, x1
	stp	x20, x19, [sp, #32]             // 16-byte Folded Spill
	ldr	x9, [x0, #600]
	ldr	x10, [x0, #64]
	add	x8, x8, #1024
	ldr	x11, [x0, #144]
	mov	x19, x2
	ldar	x14, [x8]
	mov	x20, x0
	mov	x29, sp
	cmp	x14, x11
	b.ls	.LBB4_3
.LBB4_1:
	mov	x0, x20
	mov	w1, #10                         // =0xa
	mov	w2, #4                          // =0x4
	bl	el_fail
	cbz	w0, .LBB4_16
.LBB4_2:
	ldp	x20, x19, [sp, #32]             // 16-byte Folded Reload
	ldr	x21, [sp, #16]                  // 8-byte Folded Reload
	ldp	x29, x30, [sp], #48             // 16-byte Folded Reload
	ret
.LBB4_3:
	sub	x12, x10, #1
	neg	x13, x10
	mov	w0, #1                          // =0x1
	b	.LBB4_6
.LBB4_4:                                //   in Loop: Header=BB4_6 Depth=1
	clrex
.LBB4_5:                                //   in Loop: Header=BB4_6 Depth=1
	ldar	x14, [x8]
	cmp	x14, x11
	b.hi	.LBB4_1
.LBB4_6:                                // =>This Loop Header: Depth=1
                                        //     Child Loop BB4_9 Depth 2
	and	x15, x14, x12
	and	x17, x14, x13
	add	x15, x9, x15, lsl #7
	ldar	x15, [x15]
	and	x16, x15, x13
	cmp	x16, x17
	b.ne	.LBB4_11
// %bb.7:                               //   in Loop: Header=BB4_6 Depth=1
	cmp	x14, x11
	b.hs	.LBB4_15
// %bb.8:                               //   in Loop: Header=BB4_6 Depth=1
	add	x16, x14, #1
.LBB4_9:                                //   Parent Loop BB4_6 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	ldaxr	x17, [x8]
	cmp	x17, x14
	b.ne	.LBB4_4
// %bb.10:                              //   in Loop: Header=BB4_9 Depth=2
	stlxr	w17, x16, [x8]
	cbnz	w17, .LBB4_9
	b	.LBB4_14
.LBB4_11:                               //   in Loop: Header=BB4_6 Depth=1
	subs	x14, x17, x10
	b.lo	.LBB4_5
// %bb.12:                              //   in Loop: Header=BB4_6 Depth=1
	cmp	x16, x14
	b.ne	.LBB4_5
// %bb.13:
	mov	x1, xzr
	ldp	x20, x19, [sp, #32]             // 16-byte Folded Reload
	ldr	x21, [sp, #16]                  // 8-byte Folded Reload
	ldp	x29, x30, [sp], #48             // 16-byte Folded Reload
	ret
.LBB4_14:
	and	x1, x15, x12
	b	.LBB4_17
.LBB4_15:
	mov	x0, x20
	mov	w1, #9                          // =0x9
	mov	w2, #2                          // =0x2
	bl	el_fail
	cbnz	w0, .LBB4_2
.LBB4_16:
	mov	x1, xzr
.LBB4_17:
	mov	x0, x20
	mov	x2, xzr
	mov	w3, #2                          // =0x2
	bl	el_begin_token
	add	x8, x20, #616
	mov	x0, x20
	mov	x1, x21
	ldr	q0, [x8]
	mov	x2, x19
	str	q0, [x21]
	ldp	q1, q0, [x8, #48]
	ldp	q2, q3, [x8, #16]
	stp	q1, q0, [x21, #48]
	stp	q2, q3, [x21, #16]
	bl	el_read_metadata
	ldp	x20, x19, [sp, #32]             // 16-byte Folded Reload
	ldr	x21, [sp, #16]                  // 8-byte Folded Reload
	ldp	x29, x30, [sp], #48             // 16-byte Folded Reload
	ret
.Lfunc_end4:
	.size	el_ncq_borrow, .Lfunc_end4-el_ncq_borrow
                                        // -- End function
	.globl	el_ncq_release                  // -- Begin function el_ncq_release
	.p2align	2
	.type	el_ncq_release,@function
el_ncq_release:                         // @el_ncq_release
// %bb.0:
	mov	w1, #3                          // =0x3
	b	el_ncq_return
.Lfunc_end5:
	.size	el_ncq_release, .Lfunc_end5-el_ncq_release
                                        // -- End function
	.p2align	2                               // -- Begin function el_ncq_return
	.type	el_ncq_return,@function
el_ncq_return:                          // @el_ncq_return
// %bb.0:
	stp	x29, x30, [sp, #-32]!           // 16-byte Folded Spill
	ldr	x8, [x0, #584]
	ldr	x9, [x0, #720]
	mov	x29, sp
	ldr	x10, [x0, #728]
	stp	x20, x19, [sp, #16]             // 16-byte Folded Spill
	add	x8, x8, x9, lsl #7
	ldr	x9, [x8]
	cmp	x9, x10
	b.ne	.LBB6_4
// %bb.1:
	add	x8, x8, #8
	ldar	x10, [x8]
	ldr	x9, [x0, #728]
	lsl	x9, x9, #2
	orr	x11, x9, x1
	cmp	x10, x11
	b.ne	.LBB6_4
// %bb.2:
	stlr	x9, [x8]
	ldr	x8, [x0, #552]
	ldr	x10, [x0, #720]
	ldr	x11, [x0, #592]
	ldr	x12, [x0, #64]
	ldr	x13, [x0, #144]
	add	x8, x8, #896
	ldar	x9, [x8]
	cmp	x9, x13
	b.hs	.LBB6_21
// %bb.3:
	sub	x14, x12, #1
	neg	x15, x12
	b	.LBB6_8
.LBB6_4:
	mov	w1, #10                         // =0xa
	mov	w2, #1                          // =0x1
	bl	el_fail
	mov	x19, x0
	lsr	x8, x0, #32
.LBB6_5:
	bfi	x19, x8, #32, #32
	mov	x0, x19
	ldp	x20, x19, [sp, #16]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #32             // 16-byte Folded Reload
	ret
.LBB6_6:                                //   in Loop: Header=BB6_8 Depth=1
	clrex
.LBB6_7:                                //   in Loop: Header=BB6_8 Depth=1
	ldar	x9, [x8]
	cmp	x9, x13
	b.hs	.LBB6_21
.LBB6_8:                                // =>This Loop Header: Depth=1
                                        //     Child Loop BB6_15 Depth 2
                                        //     Child Loop BB6_10 Depth 2
	and	x16, x9, x14
	and	x18, x9, x15
	add	x16, x11, x16, lsl #7
	ldar	x17, [x16]
	and	x1, x17, x15
	cmp	x1, x18
	b.ne	.LBB6_12
// %bb.9:                               //   in Loop: Header=BB6_8 Depth=1
	add	x16, x9, #1
.LBB6_10:                               //   Parent Loop BB6_8 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	ldaxr	x17, [x8]
	cmp	x17, x9
	b.ne	.LBB6_6
// %bb.11:                              //   in Loop: Header=BB6_10 Depth=2
	stlxr	w17, x16, [x8]
	cbnz	w17, .LBB6_10
	b	.LBB6_7
.LBB6_12:                               //   in Loop: Header=BB6_8 Depth=1
	subs	x2, x18, x12
	b.lo	.LBB6_7
// %bb.13:                              //   in Loop: Header=BB6_8 Depth=1
	cmp	x1, x2
	b.ne	.LBB6_7
// %bb.14:                              //   in Loop: Header=BB6_8 Depth=1
	orr	x18, x18, x10
.LBB6_15:                               //   Parent Loop BB6_8 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	ldaxr	x1, [x16]
	cmp	x1, x17
	b.ne	.LBB6_6
// %bb.16:                              //   in Loop: Header=BB6_15 Depth=2
	stlxr	w1, x18, [x16]
	cbnz	w1, .LBB6_15
// %bb.17:
	add	x10, x9, #1
.LBB6_18:                               // =>This Inner Loop Header: Depth=1
	ldaxr	x11, [x8]
	cmp	x11, x9
	b.ne	.LBB6_22
// %bb.19:                              //   in Loop: Header=BB6_18 Depth=1
	stlxr	w11, x10, [x8]
	cbnz	w11, .LBB6_18
// %bb.20:
	mov	x1, xzr
	b	.LBB6_23
.LBB6_21:
	mov	w1, #9                          // =0x9
	mov	w2, #2                          // =0x2
	mov	x20, x0
	bl	el_fail
	mov	x19, x0
	mov	x0, x20
	lsr	x8, x19, #32
	cmp	x8, #4
	b.ne	.LBB6_5
	b	.LBB6_24
.LBB6_22:
	mov	x1, xzr
	clrex
.LBB6_23:
	mov	x19, #17179869184               // =0x400000000
.LBB6_24:
	mov	x20, x1
	bl	el_end_token
	mov	x1, x20
	mov	w8, #4                          // =0x4
	bfi	x19, x8, #32, #32
	mov	x0, x19
	ldp	x20, x19, [sp, #16]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #32             // 16-byte Folded Reload
	ret
.Lfunc_end6:
	.size	el_ncq_return, .Lfunc_end6-el_ncq_return
                                        // -- End function
	.globl	el_ncq_abort                    // -- Begin function el_ncq_abort
	.p2align	2
	.type	el_ncq_abort,@function
el_ncq_abort:                           // @el_ncq_abort
// %bb.0:
	mov	w1, #1                          // =0x1
	b	el_ncq_return
.Lfunc_end7:
	.size	el_ncq_abort, .Lfunc_end7-el_ncq_abort
                                        // -- End function
	.ident	"clang version 17.0.0 (https://github.com/swiftlang/llvm-project.git 10999b6d034fe318f3d56c83bddb6572593a8bb0)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
