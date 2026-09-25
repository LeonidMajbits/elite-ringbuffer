	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 14, 4
	.globl	_el_ncq_enqueue                 ; -- Begin function el_ncq_enqueue
	.p2align	2
_el_ncq_enqueue:                        ; @el_ncq_enqueue
; %bb.0:
	stp	x29, x30, [sp, #-16]!           ; 16-byte Folded Spill
	mov	x29, sp
	ldr	x8, [x0, #552]
	cmp	w1, #0
	mov	w9, #1152                       ; =0x480
	mov	w10, #896                       ; =0x380
	csel	x9, x10, x9, ne
	mov	w10, #600                       ; =0x258
	mov	w11, #592                       ; =0x250
	csel	x10, x11, x10, ne
	add	x12, x8, x9
	ldr	x10, [x0, x10]
	ldr	x11, [x0, #64]
	ldr	x13, [x0, #144]
	ldar	x12, [x12]
	cmp	x12, x13
	b.hs	LBB0_9
; %bb.1:
	sub	x14, x11, #1
	neg	x15, x11
	b	LBB0_4
LBB0_2:                                 ;   in Loop: Header=BB0_4 Depth=1
	add	x16, x12, #1
	add	x17, x8, x9
	casal	x12, x16, [x17]
LBB0_3:                                 ;   in Loop: Header=BB0_4 Depth=1
	add	x12, x8, x9
	ldar	x12, [x12]
	cmp	x12, x13
	b.hs	LBB0_9
LBB0_4:                                 ; =>This Inner Loop Header: Depth=1
	and	x16, x12, x15
	and	x17, x12, x14
	add	x17, x10, x17, lsl #7
	ldar	x3, [x17]
	and	x4, x3, x15
	cmp	x4, x16
	b.eq	LBB0_2
; %bb.5:                                ;   in Loop: Header=BB0_4 Depth=1
	subs	x5, x16, x11
	b.lo	LBB0_3
; %bb.6:                                ;   in Loop: Header=BB0_4 Depth=1
	cmp	x4, x5
	b.ne	LBB0_3
; %bb.7:                                ;   in Loop: Header=BB0_4 Depth=1
	orr	x16, x16, x2
	mov	x4, x3
	casal	x4, x16, [x17]
	cmp	x4, x3
	b.ne	LBB0_3
; %bb.8:
	mov	x0, #0                          ; =0x0
	add	x10, x12, #1
	add	x8, x8, x9
	casal	x12, x10, [x8]
	cmp	w1, #0
	mov	x1, x0
	mov	x8, #8589934592                 ; =0x200000000
	mov	x9, #17179869184                ; =0x400000000
	csel	x0, x9, x8, ne
	ldp	x29, x30, [sp], #16             ; 16-byte Folded Reload
	ret
LBB0_9:
	mov	w1, #9                          ; =0x9
	mov	w2, #2                          ; =0x2
	bl	_el_fail
	ldp	x29, x30, [sp], #16             ; 16-byte Folded Reload
	ret
                                        ; -- End function
	.globl	_el_ncq_dequeue                 ; -- Begin function el_ncq_dequeue
	.p2align	2
_el_ncq_dequeue:                        ; @el_ncq_dequeue
; %bb.0:
	stp	x29, x30, [sp, #-16]!           ; 16-byte Folded Spill
	mov	x29, sp
	ldr	x8, [x0, #552]
	cmp	w1, #0
	mov	w9, #1024                       ; =0x400
	mov	w10, #768                       ; =0x300
	csel	x9, x10, x9, ne
	mov	w10, #600                       ; =0x258
	mov	w11, #592                       ; =0x250
	csel	x10, x11, x10, ne
	add	x13, x8, x9
	ldr	x10, [x0, x10]
	ldr	x11, [x0, #64]
	ldr	x12, [x0, #144]
	ldar	x15, [x13]
	cmp	x15, x12
	b.ls	LBB1_2
LBB1_1:
	mov	w1, #10                         ; =0xa
	mov	w2, #4                          ; =0x4
	bl	_el_fail
	ldp	x29, x30, [sp], #16             ; 16-byte Folded Reload
	ret
LBB1_2:
	sub	x13, x11, #1
	neg	x14, x11
	b	LBB1_4
LBB1_3:                                 ;   in Loop: Header=BB1_4 Depth=1
	add	x15, x8, x9
	ldar	x15, [x15]
	cmp	x15, x12
	b.hi	LBB1_1
LBB1_4:                                 ; =>This Inner Loop Header: Depth=1
	and	x17, x15, x14
	and	x16, x15, x13
	add	x16, x10, x16, lsl #7
	ldar	x16, [x16]
	and	x3, x16, x14
	cmp	x3, x17
	b.ne	LBB1_7
; %bb.5:                                ;   in Loop: Header=BB1_4 Depth=1
	cmp	x15, x12
	b.hs	LBB1_10
; %bb.6:                                ;   in Loop: Header=BB1_4 Depth=1
	add	x17, x15, #1
	add	x3, x8, x9
	mov	x4, x15
	casal	x4, x17, [x3]
	cmp	x4, x15
	b.ne	LBB1_3
	b	LBB1_11
LBB1_7:                                 ;   in Loop: Header=BB1_4 Depth=1
	subs	x15, x17, x11
	b.lo	LBB1_3
; %bb.8:                                ;   in Loop: Header=BB1_4 Depth=1
	cmp	x3, x15
	b.ne	LBB1_3
; %bb.9:
	mov	x0, #0                          ; =0x0
	cmp	w1, #0
	mov	x1, x0
	mov	w8, #1                          ; =0x1
	cinc	x0, x8, ne
	ldp	x29, x30, [sp], #16             ; 16-byte Folded Reload
	ret
LBB1_10:
	mov	w1, #9                          ; =0x9
	mov	w2, #2                          ; =0x2
	bl	_el_fail
	ldp	x29, x30, [sp], #16             ; 16-byte Folded Reload
	ret
LBB1_11:
	mov	x0, #0                          ; =0x0
	mov	x1, #0                          ; =0x0
	and	x8, x16, x13
	str	x8, [x2]
	ldp	x29, x30, [sp], #16             ; 16-byte Folded Reload
	ret
                                        ; -- End function
	.globl	_el_ncq_reserve                 ; -- Begin function el_ncq_reserve
	.p2align	2
_el_ncq_reserve:                        ; @el_ncq_reserve
; %bb.0:
	stp	x26, x25, [sp, #-80]!           ; 16-byte Folded Spill
	stp	x24, x23, [sp, #16]             ; 16-byte Folded Spill
	stp	x22, x21, [sp, #32]             ; 16-byte Folded Spill
	stp	x20, x19, [sp, #48]             ; 16-byte Folded Spill
	stp	x29, x30, [sp, #64]             ; 16-byte Folded Spill
	add	x29, sp, #64
	mov	x20, x2
	mov	x21, x1
	mov	x19, x0
	ldr	x8, [x0, #552]
	add	x12, x8, #768
	ldr	x9, [x0, #592]
	ldr	x10, [x0, #64]
	ldr	x11, [x0, #144]
	ldar	x14, [x12]
	cmp	x14, x11
	b.ls	LBB2_3
LBB2_1:
	mov	x0, x19
	mov	w1, #10                         ; =0xa
	mov	w2, #4                          ; =0x4
	bl	_el_fail
	cbz	w0, LBB2_12
LBB2_2:
	ldp	x29, x30, [sp, #64]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #48]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #32]             ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #16]             ; 16-byte Folded Reload
	ldp	x26, x25, [sp], #80             ; 16-byte Folded Reload
	ret
LBB2_3:
	sub	x12, x10, #1
	neg	x13, x10
	mov	w0, #2                          ; =0x2
	b	LBB2_5
LBB2_4:                                 ;   in Loop: Header=BB2_5 Depth=1
	add	x14, x8, #768
	ldar	x14, [x14]
	cmp	x14, x11
	b.hi	LBB2_1
LBB2_5:                                 ; =>This Inner Loop Header: Depth=1
	and	x16, x14, x13
	and	x15, x14, x12
	add	x15, x9, x15, lsl #7
	ldar	x15, [x15]
	and	x17, x15, x13
	cmp	x17, x16
	b.ne	LBB2_8
; %bb.6:                                ;   in Loop: Header=BB2_5 Depth=1
	cmp	x14, x11
	b.hs	LBB2_11
; %bb.7:                                ;   in Loop: Header=BB2_5 Depth=1
	add	x16, x14, #1
	add	x17, x8, #768
	mov	x1, x14
	casal	x1, x16, [x17]
	cmp	x1, x14
	b.ne	LBB2_4
	b	LBB2_13
LBB2_8:                                 ;   in Loop: Header=BB2_5 Depth=1
	subs	x14, x16, x10
	b.lo	LBB2_4
; %bb.9:                                ;   in Loop: Header=BB2_5 Depth=1
	cmp	x17, x14
	b.ne	LBB2_4
; %bb.10:
	mov	x1, #0                          ; =0x0
	ldp	x29, x30, [sp, #64]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #48]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #32]             ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #16]             ; 16-byte Folded Reload
	ldp	x26, x25, [sp], #80             ; 16-byte Folded Reload
	ret
LBB2_11:
	mov	x0, x19
	mov	w1, #9                          ; =0x9
	mov	w2, #2                          ; =0x2
	bl	_el_fail
	cbnz	w0, LBB2_2
LBB2_12:
	mov	x22, #0                         ; =0x0
	b	LBB2_14
LBB2_13:
	and	x22, x15, x12
LBB2_14:
	mov	x0, x19
	mov	x1, x22
	mov	x2, #0                          ; =0x0
	mov	w3, #1                          ; =0x1
	bl	_el_begin_token
	add	x24, x19, #616
	ldr	q0, [x24]
	str	q0, [x21]
	ldp	q0, q1, [x24, #16]
	ldp	q2, q3, [x24, #48]
	stp	q2, q3, [x21, #48]
	stp	q0, q1, [x21, #16]
	ldr	x8, [x19, #584]
	add	x25, x8, x22, lsl #7
	ldapur	x8, [x25, #8]
	tst	x8, #0x3
	b.ne	LBB2_17
; %bb.15:
	lsr	x8, x8, #2
	ldr	x9, [x25]
	cmp	x9, x8
	b.ne	LBB2_17
; %bb.16:
	ldr	x9, [x19, #152]
	cmp	x8, x9
	b.ls	LBB2_19
LBB2_17:
	mov	x0, x19
	mov	w1, #10                         ; =0xa
	mov	w2, #1                          ; =0x1
LBB2_18:
	bl	_el_fail
	ldp	x29, x30, [sp, #64]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #48]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #32]             ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #16]             ; 16-byte Folded Reload
	ldp	x26, x25, [sp], #80             ; 16-byte Folded Reload
	ret
LBB2_19:
	b.hs	LBB2_21
; %bb.20:
	add	x23, x8, #1
	str	x23, [x25], #8
	mov	x0, x19
	mov	x1, x23
	bl	_el_set_epoch
	mov	x1, #0                          ; =0x0
	ldr	q0, [x24]
	str	q0, [x21]
	ldp	q0, q1, [x24, #16]
	ldp	q2, q3, [x24, #48]
	stp	q2, q3, [x21, #48]
	stp	q0, q1, [x21, #16]
	mov	w8, #1                          ; =0x1
	orr	x8, x8, x23, lsl #2
	stlr	x8, [x25]
	ldr	x8, [x19, #88]
	ldr	x9, [x19, #608]
	madd	x8, x8, x22, x9
	str	x8, [x20]
	ldr	w8, [x19, #72]
	str	w8, [x20, #8]
	mov	x0, #4294967296                 ; =0x100000000
	ldp	x29, x30, [sp, #64]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #48]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #32]             ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #16]             ; 16-byte Folded Reload
	ldp	x26, x25, [sp], #80             ; 16-byte Folded Reload
	ret
LBB2_21:
	mov	x0, x19
	mov	w1, #9                          ; =0x9
	mov	w2, #2                          ; =0x2
	b	LBB2_18
                                        ; -- End function
	.globl	_el_ncq_commit                  ; -- Begin function el_ncq_commit
	.p2align	2
_el_ncq_commit:                         ; @el_ncq_commit
; %bb.0:
	stp	x20, x19, [sp, #-32]!           ; 16-byte Folded Spill
	stp	x29, x30, [sp, #16]             ; 16-byte Folded Spill
	add	x29, sp, #16
	ldr	x8, [x0, #584]
	ldr	x9, [x0, #720]
	add	x20, x8, x9, lsl #7
	ldr	x8, [x20]
	ldr	x9, [x0, #728]
	cmp	x8, x9
	b.ne	LBB3_4
; %bb.1:
	ldapur	x8, [x20, #8]
	ldr	x9, [x0, #728]
	mov	w10, #1                         ; =0x1
	bfi	x10, x9, #2, #62
	cmp	x8, x10
	b.ne	LBB3_4
; %bb.2:
	stp	w1, w2, [x20, #16]
	str	x3, [x20, #32]
	ldr	w8, [x0, #48]
	cmp	w8, #1
	mov	x19, x0
	b.ne	LBB3_6
; %bb.3:
	ldr	x8, [x0, #720]
	ldr	x9, [x0, #88]
	ldr	x10, [x0, #608]
	madd	x0, x9, x8, x10
	mov	w1, w1
	bl	_elite_crc64
	mov	x8, x0
	mov	x0, x19
	ldr	x9, [x19, #728]
	b	LBB3_7
LBB3_4:
	mov	w1, #10                         ; =0xa
	mov	w2, #1                          ; =0x1
	bl	_el_fail
	lsr	x8, x0, #32
LBB3_5:
	bfi	x0, x8, #32, #32
	ldp	x29, x30, [sp, #16]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp], #32             ; 16-byte Folded Reload
	ret
LBB3_6:
	mov	x8, #0                          ; =0x0
LBB3_7:
	lsl	x9, x9, #2
	str	x8, [x20, #24]
	orr	x8, x9, #0x2
	stlur	x8, [x20, #8]
	ldr	x9, [x0, #720]
	ldr	x8, [x0, #552]
	add	x12, x8, #1152
	ldr	x10, [x0, #600]
	ldr	x11, [x0, #64]
	ldr	x13, [x0, #144]
	ldar	x12, [x12]
	cmp	x12, x13
	b.hs	LBB3_16
; %bb.8:
	sub	x14, x11, #1
	neg	x15, x11
	b	LBB3_11
LBB3_9:                                 ;   in Loop: Header=BB3_11 Depth=1
	add	x16, x12, #1
	add	x17, x8, #1152
	casal	x12, x16, [x17]
LBB3_10:                                ;   in Loop: Header=BB3_11 Depth=1
	add	x12, x8, #1152
	ldar	x12, [x12]
	cmp	x12, x13
	b.hs	LBB3_16
LBB3_11:                                ; =>This Inner Loop Header: Depth=1
	and	x16, x12, x15
	and	x17, x12, x14
	add	x17, x10, x17, lsl #7
	ldar	x1, [x17]
	and	x2, x1, x15
	cmp	x2, x16
	b.eq	LBB3_9
; %bb.12:                               ;   in Loop: Header=BB3_11 Depth=1
	subs	x3, x16, x11
	b.lo	LBB3_10
; %bb.13:                               ;   in Loop: Header=BB3_11 Depth=1
	cmp	x2, x3
	b.ne	LBB3_10
; %bb.14:                               ;   in Loop: Header=BB3_11 Depth=1
	orr	x16, x16, x9
	mov	x2, x1
	casal	x2, x16, [x17]
	cmp	x2, x1
	b.ne	LBB3_10
; %bb.15:
	mov	x1, #0                          ; =0x0
	add	x9, x12, #1
	add	x8, x8, #1152
	casal	x12, x9, [x8]
	mov	x0, #8589934592                 ; =0x200000000
	b	LBB3_17
LBB3_16:
	mov	w1, #9                          ; =0x9
	mov	w2, #2                          ; =0x2
	bl	_el_fail
	lsr	x8, x0, #32
	cmp	x8, #2
	b.ne	LBB3_5
LBB3_17:
	mov	x20, x0
	mov	x0, x19
	mov	x19, x1
	bl	_el_end_token
	mov	x0, x20
	mov	x1, x19
	mov	w8, #2                          ; =0x2
	bfi	x0, x8, #32, #32
	ldp	x29, x30, [sp, #16]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp], #32             ; 16-byte Folded Reload
	ret
                                        ; -- End function
	.globl	_el_ncq_borrow                  ; -- Begin function el_ncq_borrow
	.p2align	2
_el_ncq_borrow:                         ; @el_ncq_borrow
; %bb.0:
	stp	x22, x21, [sp, #-48]!           ; 16-byte Folded Spill
	stp	x20, x19, [sp, #16]             ; 16-byte Folded Spill
	stp	x29, x30, [sp, #32]             ; 16-byte Folded Spill
	add	x29, sp, #32
	mov	x19, x2
	mov	x20, x1
	mov	x21, x0
	ldr	x8, [x0, #552]
	add	x12, x8, #1024
	ldr	x9, [x0, #600]
	ldr	x10, [x0, #64]
	ldr	x11, [x0, #144]
	ldar	x14, [x12]
	cmp	x14, x11
	b.ls	LBB4_3
LBB4_1:
	mov	x0, x21
	mov	w1, #10                         ; =0xa
	mov	w2, #4                          ; =0x4
	bl	_el_fail
	cbz	w0, LBB4_12
LBB4_2:
	ldp	x29, x30, [sp, #32]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #16]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp], #48             ; 16-byte Folded Reload
	ret
LBB4_3:
	sub	x12, x10, #1
	neg	x13, x10
	mov	w0, #1                          ; =0x1
	b	LBB4_5
LBB4_4:                                 ;   in Loop: Header=BB4_5 Depth=1
	add	x14, x8, #1024
	ldar	x14, [x14]
	cmp	x14, x11
	b.hi	LBB4_1
LBB4_5:                                 ; =>This Inner Loop Header: Depth=1
	and	x16, x14, x13
	and	x15, x14, x12
	add	x15, x9, x15, lsl #7
	ldar	x15, [x15]
	and	x17, x15, x13
	cmp	x17, x16
	b.ne	LBB4_8
; %bb.6:                                ;   in Loop: Header=BB4_5 Depth=1
	cmp	x14, x11
	b.hs	LBB4_11
; %bb.7:                                ;   in Loop: Header=BB4_5 Depth=1
	add	x16, x14, #1
	add	x17, x8, #1024
	mov	x1, x14
	casal	x1, x16, [x17]
	cmp	x1, x14
	b.ne	LBB4_4
	b	LBB4_13
LBB4_8:                                 ;   in Loop: Header=BB4_5 Depth=1
	subs	x14, x16, x10
	b.lo	LBB4_4
; %bb.9:                                ;   in Loop: Header=BB4_5 Depth=1
	cmp	x17, x14
	b.ne	LBB4_4
; %bb.10:
	mov	x1, #0                          ; =0x0
	ldp	x29, x30, [sp, #32]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #16]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp], #48             ; 16-byte Folded Reload
	ret
LBB4_11:
	mov	x0, x21
	mov	w1, #9                          ; =0x9
	mov	w2, #2                          ; =0x2
	bl	_el_fail
	cbnz	w0, LBB4_2
LBB4_12:
	mov	x1, #0                          ; =0x0
	b	LBB4_14
LBB4_13:
	and	x1, x15, x12
LBB4_14:
	mov	x0, x21
	mov	x2, #0                          ; =0x0
	mov	w3, #2                          ; =0x2
	bl	_el_begin_token
	add	x8, x21, #616
	ldr	q0, [x8]
	str	q0, [x20]
	ldp	q0, q1, [x8, #16]
	ldp	q2, q3, [x8, #48]
	stp	q2, q3, [x20, #48]
	stp	q0, q1, [x20, #16]
	mov	x0, x21
	mov	x1, x20
	mov	x2, x19
	bl	_el_read_metadata
	ldp	x29, x30, [sp, #32]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #16]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp], #48             ; 16-byte Folded Reload
	ret
                                        ; -- End function
	.globl	_el_ncq_release                 ; -- Begin function el_ncq_release
	.p2align	2
_el_ncq_release:                        ; @el_ncq_release
; %bb.0:
	mov	w1, #3                          ; =0x3
	b	_el_ncq_return
                                        ; -- End function
	.p2align	2                               ; -- Begin function el_ncq_return
_el_ncq_return:                         ; @el_ncq_return
; %bb.0:
	stp	x20, x19, [sp, #-32]!           ; 16-byte Folded Spill
	stp	x29, x30, [sp, #16]             ; 16-byte Folded Spill
	add	x29, sp, #16
	ldr	x8, [x0, #584]
	ldr	x9, [x0, #720]
	add	x8, x8, x9, lsl #7
	ldr	x9, [x8]
	ldr	x10, [x0, #728]
	cmp	x9, x10
	b.ne	LBB6_11
; %bb.1:
	ldapur	x10, [x8, #8]
	ldr	x9, [x0, #728]
	lsl	x9, x9, #2
	orr	x11, x9, x1
	cmp	x10, x11
	b.ne	LBB6_11
; %bb.2:
	stlur	x9, [x8, #8]
	ldr	x9, [x0, #720]
	ldr	x8, [x0, #552]
	add	x12, x8, #896
	ldr	x10, [x0, #592]
	ldr	x11, [x0, #64]
	ldr	x13, [x0, #144]
	ldar	x12, [x12]
	cmp	x12, x13
	b.hs	LBB6_13
; %bb.3:
	sub	x14, x11, #1
	neg	x15, x11
	b	LBB6_6
LBB6_4:                                 ;   in Loop: Header=BB6_6 Depth=1
	add	x16, x12, #1
	add	x17, x8, #896
	casal	x12, x16, [x17]
LBB6_5:                                 ;   in Loop: Header=BB6_6 Depth=1
	add	x12, x8, #896
	ldar	x12, [x12]
	cmp	x12, x13
	b.hs	LBB6_13
LBB6_6:                                 ; =>This Inner Loop Header: Depth=1
	and	x16, x12, x15
	and	x17, x12, x14
	add	x17, x10, x17, lsl #7
	ldar	x1, [x17]
	and	x2, x1, x15
	cmp	x2, x16
	b.eq	LBB6_4
; %bb.7:                                ;   in Loop: Header=BB6_6 Depth=1
	subs	x3, x16, x11
	b.lo	LBB6_5
; %bb.8:                                ;   in Loop: Header=BB6_6 Depth=1
	cmp	x2, x3
	b.ne	LBB6_5
; %bb.9:                                ;   in Loop: Header=BB6_6 Depth=1
	orr	x16, x16, x9
	mov	x2, x1
	casal	x2, x16, [x17]
	cmp	x2, x1
	b.ne	LBB6_5
; %bb.10:
	mov	x19, x0
	mov	x1, #0                          ; =0x0
	add	x9, x12, #1
	add	x8, x8, #896
	casal	x12, x9, [x8]
	mov	x0, #17179869184                ; =0x400000000
	b	LBB6_14
LBB6_11:
	mov	w1, #10                         ; =0xa
	mov	w2, #1                          ; =0x1
	bl	_el_fail
	lsr	x8, x0, #32
LBB6_12:
	bfi	x0, x8, #32, #32
	ldp	x29, x30, [sp, #16]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp], #32             ; 16-byte Folded Reload
	ret
LBB6_13:
	mov	x19, x0
	mov	w1, #9                          ; =0x9
	mov	w2, #2                          ; =0x2
	bl	_el_fail
	lsr	x8, x0, #32
	cmp	x8, #4
	b.ne	LBB6_12
LBB6_14:
	mov	x20, x0
	mov	x0, x19
	mov	x19, x1
	bl	_el_end_token
	mov	x0, x20
	mov	x1, x19
	mov	w8, #4                          ; =0x4
	bfi	x0, x8, #32, #32
	ldp	x29, x30, [sp, #16]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp], #32             ; 16-byte Folded Reload
	ret
                                        ; -- End function
	.globl	_el_ncq_abort                   ; -- Begin function el_ncq_abort
	.p2align	2
_el_ncq_abort:                          ; @el_ncq_abort
; %bb.0:
	mov	w1, #1                          ; =0x1
	b	_el_ncq_return
                                        ; -- End function
.subsections_via_symbols
