	.text
	.file	"elite_spsc.c"
	.globl	el_spsc_reserve                 // -- Begin function el_spsc_reserve
	.p2align	2
	.type	el_spsc_reserve,@function
el_spsc_reserve:                        // @el_spsc_reserve
// %bb.0:
	stp	x29, x30, [sp, #-48]!           // 16-byte Folded Spill
	ldr	x8, [x0, #704]
	ldr	x9, [x0, #144]
	mov	x29, sp
	stp	x22, x21, [sp, #16]             // 16-byte Folded Spill
	cmp	x8, x9
	stp	x20, x19, [sp, #32]             // 16-byte Folded Spill
	b.hs	.LBB0_6
// %bb.1:
	ldr	x9, [x0, #712]
	subs	x10, x8, x9
	b.hs	.LBB0_3
.LBB0_2:
	mov	w1, #10                         // =0xa
	mov	w2, #4                          // =0x4
	ldp	x20, x19, [sp, #32]             // 16-byte Folded Reload
	ldp	x22, x21, [sp, #16]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #48             // 16-byte Folded Reload
	b	el_fail
.LBB0_3:
	ldr	x9, [x0, #64]
	cmp	x10, x9
	b.hs	.LBB0_7
.LBB0_4:
	sub	x9, x9, #1
	ldr	x10, [x0, #584]
	and	x19, x9, x8
	add	x9, x10, x19, lsl #7
	ldr	x10, [x0, #152]
	ldr	x8, [x9]
	cmp	x8, x10
	b.hs	.LBB0_6
// %bb.5:
	add	x8, x8, #1
	mov	x21, x1
	mov	x1, x19
	mov	x22, x2
	mov	x2, x8
	mov	w3, #1                          // =0x1
	str	x8, [x9]
	mov	x20, x0
	bl	el_begin_token
	add	x8, x20, #616
	mov	x1, xzr
	mov	x0, #4294967296                 // =0x100000000
	ldr	q0, [x8]
	str	q0, [x21]
	ldp	q1, q0, [x8, #48]
	ldp	q2, q3, [x8, #16]
	stp	q1, q0, [x21, #48]
	stp	q2, q3, [x21, #16]
	ldr	x8, [x20, #88]
	ldr	x9, [x20, #608]
	madd	x8, x8, x19, x9
	ldr	w9, [x20, #72]
	str	w9, [x22, #8]
	str	x8, [x22]
	ldp	x20, x19, [sp, #32]             // 16-byte Folded Reload
	ldp	x22, x21, [sp, #16]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #48             // 16-byte Folded Reload
	ret
.LBB0_6:
	mov	w1, #9                          // =0x9
	mov	w2, #2                          // =0x2
	ldp	x20, x19, [sp, #32]             // 16-byte Folded Reload
	ldp	x22, x21, [sp, #16]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #48             // 16-byte Folded Reload
	b	el_fail
.LBB0_7:
	ldr	x10, [x0, #544]
	add	x10, x10, #896
	ldar	x11, [x10]
	subs	x10, x8, x11
	str	x11, [x0, #712]
	b.lo	.LBB0_2
// %bb.8:
	cmp	x10, x9
	b.lo	.LBB0_4
// %bb.9:
	mov	x1, xzr
	mov	w0, #2                          // =0x2
	ldp	x20, x19, [sp, #32]             // 16-byte Folded Reload
	ldp	x22, x21, [sp, #16]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #48             // 16-byte Folded Reload
	ret
.Lfunc_end0:
	.size	el_spsc_reserve, .Lfunc_end0-el_spsc_reserve
                                        // -- End function
	.globl	el_spsc_commit                  // -- Begin function el_spsc_commit
	.p2align	2
	.type	el_spsc_commit,@function
el_spsc_commit:                         // @el_spsc_commit
// %bb.0:
	stp	x29, x30, [sp, #-32]!           // 16-byte Folded Spill
	ldr	x9, [x0, #584]
	ldr	x8, [x0, #720]
	mov	x29, sp
	stp	x20, x19, [sp, #16]             // 16-byte Folded Spill
	ldr	x10, [x0, #728]
	add	x19, x9, x8, lsl #7
	ldr	x9, [x19]
	cmp	x9, x10
	b.ne	.LBB1_3
// %bb.1:
	ldr	w9, [x0, #48]
	stp	w1, w2, [x19, #16]
	str	x3, [x19, #32]
	cmp	w9, #1
	b.ne	.LBB1_4
// %bb.2:
	ldr	x9, [x0, #88]
	ldr	x10, [x0, #608]
	mov	w1, w1
	mov	x20, x0
	madd	x8, x9, x8, x10
	mov	x0, x8
	bl	elite_crc64
	mov	x8, x0
	mov	x0, x20
	b	.LBB1_5
.LBB1_3:
	ldp	x20, x19, [sp, #16]             // 16-byte Folded Reload
	mov	w1, #10                         // =0xa
	mov	w2, #1                          // =0x1
	ldp	x29, x30, [sp], #32             // 16-byte Folded Reload
	b	el_fail
.LBB1_4:
	mov	x8, xzr
.LBB1_5:
	ldr	x9, [x0, #704]
	ldr	x10, [x0, #544]
	str	x8, [x19, #24]
	mov	x19, x0
	add	x8, x9, #1
	add	x9, x10, #768
	stlr	x8, [x9]
	str	x8, [x0, #704]
	bl	el_end_token
	mov	x0, x19
	mov	w1, #1                          // =0x1
	bl	el_notify_connection
	mov	x8, #11                         // =0xb
	cmp	w0, #0
	mov	x9, #8589934592                 // =0x200000000
	movk	x8, #2, lsl #32
	ldp	x20, x19, [sp, #16]             // 16-byte Folded Reload
	csel	x8, x9, x8, eq
	mov	w1, w0
	mov	x0, x8
	ldp	x29, x30, [sp], #32             // 16-byte Folded Reload
	ret
.Lfunc_end1:
	.size	el_spsc_commit, .Lfunc_end1-el_spsc_commit
                                        // -- End function
	.globl	el_spsc_borrow                  // -- Begin function el_spsc_borrow
	.p2align	2
	.type	el_spsc_borrow,@function
el_spsc_borrow:                         // @el_spsc_borrow
// %bb.0:
	ldr	x9, [x0, #712]
	ldr	x8, [x0, #704]
	cmp	x9, x8
	b.hi	.LBB2_2
// %bb.1:
	ldr	x9, [x0, #544]
	add	x9, x9, #768
	ldar	x9, [x9]
	str	x9, [x0, #712]
.LBB2_2:
	subs	x12, x9, x8
	b.lo	.LBB2_5
// %bb.3:
	ldr	x10, [x0, #144]
	cmp	x9, x10
	b.hi	.LBB2_5
// %bb.4:
	ldr	x11, [x0, #64]
	cmp	x12, x11
	b.ls	.LBB2_6
.LBB2_5:
	mov	w1, #10                         // =0xa
	mov	w2, #4                          // =0x4
	b	el_fail
.LBB2_6:
	cmp	x9, x8
	b.ne	.LBB2_8
// %bb.7:
	mov	w0, #1                          // =0x1
	mov	x1, xzr
	ret
.LBB2_8:
	cmp	x8, x10
	b.hs	.LBB2_10
// %bb.9:
	stp	x29, x30, [sp, #-48]!           // 16-byte Folded Spill
	sub	x9, x11, #1
	str	x21, [sp, #16]                  // 8-byte Folded Spill
	mov	x21, x2
	stp	x20, x19, [sp, #32]             // 16-byte Folded Spill
	mov	x19, x1
	and	x1, x9, x8
	mov	x2, xzr
	mov	w3, #2                          // =0x2
	mov	x29, sp
	mov	x20, x0
	bl	el_begin_token
	mov	x0, x20
	mov	x1, x19
	mov	x2, x21
	ldp	x20, x19, [sp, #32]             // 16-byte Folded Reload
	ldr	x21, [sp, #16]                  // 8-byte Folded Reload
	ldp	x29, x30, [sp], #48             // 16-byte Folded Reload
	b	el_read_metadata
.LBB2_10:
	mov	w1, #9                          // =0x9
	mov	w2, #2                          // =0x2
	b	el_fail
.Lfunc_end2:
	.size	el_spsc_borrow, .Lfunc_end2-el_spsc_borrow
                                        // -- End function
	.globl	el_spsc_release                 // -- Begin function el_spsc_release
	.p2align	2
	.type	el_spsc_release,@function
el_spsc_release:                        // @el_spsc_release
// %bb.0:
	ldr	x8, [x0, #720]
	ldr	x9, [x0, #584]
	lsl	x8, x8, #7
	ldr	x8, [x9, x8]
	ldr	x9, [x0, #728]
	cmp	x8, x9
	b.ne	.LBB3_2
// %bb.1:
	stp	x29, x30, [sp, #-32]!           // 16-byte Folded Spill
	ldr	x8, [x0, #704]
	ldr	x9, [x0, #544]
	mov	x29, sp
	str	x19, [sp, #16]                  // 8-byte Folded Spill
	mov	x19, x0
	add	x8, x8, #1
	add	x9, x9, #896
	stlr	x8, [x9]
	str	x8, [x0, #704]
	bl	el_end_token
	mov	x0, x19
	mov	w1, wzr
	bl	el_notify_connection
	mov	x8, #11                         // =0xb
	cmp	w0, #0
	mov	x9, #17179869184                // =0x400000000
	movk	x8, #4, lsl #32
	ldr	x19, [sp, #16]                  // 8-byte Folded Reload
	mov	w1, w0
	csel	x8, x9, x8, eq
	mov	x0, x8
	ldp	x29, x30, [sp], #32             // 16-byte Folded Reload
	ret
.LBB3_2:
	mov	w1, #10                         // =0xa
	mov	w2, #1                          // =0x1
	b	el_fail
.Lfunc_end3:
	.size	el_spsc_release, .Lfunc_end3-el_spsc_release
                                        // -- End function
	.globl	el_spsc_abort                   // -- Begin function el_spsc_abort
	.p2align	2
	.type	el_spsc_abort,@function
el_spsc_abort:                          // @el_spsc_abort
// %bb.0:
	ldr	x8, [x0, #720]
	ldr	x9, [x0, #584]
	lsl	x8, x8, #7
	ldr	x8, [x9, x8]
	ldr	x9, [x0, #728]
	cmp	x8, x9
	b.ne	.LBB4_2
// %bb.1:
	stp	x29, x30, [sp, #-16]!           // 16-byte Folded Spill
	mov	x29, sp
	bl	el_end_token
	mov	x0, #17179869184                // =0x400000000
	mov	x1, xzr
	ldp	x29, x30, [sp], #16             // 16-byte Folded Reload
	ret
.LBB4_2:
	mov	w1, #10                         // =0xa
	mov	w2, #1                          // =0x1
	b	el_fail
.Lfunc_end4:
	.size	el_spsc_abort, .Lfunc_end4-el_spsc_abort
                                        // -- End function
	.ident	"clang version 17.0.0 (https://github.com/swiftlang/llvm-project.git 10999b6d034fe318f3d56c83bddb6572593a8bb0)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
