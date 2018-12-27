/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

.eabi_attribute Tag_CPU_unaligned_access, 0		// no unaligned accesses
.eabi_attribute Tag_ABI_VFP_args, 1				// hardfp

// ############################################################################## //

// ############################################################################## //

	.section .text, "ax"
	.globl   memcpy32
	.p2align 2
	.thumb_func
	.type    memcpy32,%function		// r0 = (u32)dst*, r1 = (u32)src*, r2 = (u32)length (not size, just number of elements)
		
memcpy32:
	and     r12, r2, #7     // r12= residual word count
    movs    r2, r2, lsr #3  // r2=block count
    beq     .res_cpy32
    push    {r4-r10}
    // Copy 32byte chunks with 8fold xxmia
    // r2 in [1,inf>
.main_cpy32:
	ldmia   r1!, {r3-r10}   
    stmia   r0!, {r3-r10}
    subs    r2, #1
    bne     .main_cpy32
    pop     {r4-r10}
    //@ And the residual 0-7 words. r12 in [0,7]
.res_cpy32:
    subs    r12, #1
	itt		cs
    ldrcs   r3, [r1], #4
    strcs   r3, [r0], #4
    bcs     .res_cpy32
	bx		lr	
.end_memcpy32:
.size memcpy32, .end_memcpy32-memcpy32
// this is the proper way to declare function, no exception bull shit with using fnstar, fnend etc

// ############################################################################## //

// ############################################################################## //

	.section .text, "ax"
	.globl   memcpy8
	.p2align 2
	.thumb_func
	.type    memcpy8,%function		// r0 = (u32)dst*, r1 = (u32)src*, r2 = (u32)length (not size, just number of elements)
		
memcpy8:
.loop_memcpy8:
	ldrb 	r3, [r1], #1
	strb 	r3, [r0], #1
	subs 	r2, #1
	bne 	.loop_memcpy8
	bx 		lr
.end_memcpy8:
.size memcpy8, .end_memcpy8-memcpy8
// this is the proper way to declare function, no exception bull shit with using fnstar, fnend etc

// ############################################################################## //
// asmConv_L8L8L8L8_To_L8 : input and output must be divisiable by 8
// ############################################################################## //

	.section .text, "ax"
	.globl   asmConv_L8L8L8L8_To_L8
	.p2align 2
	.thumb_func
	.type    asmConv_L8L8L8L8_To_L8,%function		// r0 = (u8)dst*, r1 = (u32)src*, r2 = (u32)length (not size, just number of elements)
		
asmConv_L8L8L8L8_To_L8:
	push    {r4-r10}			// using 8 of the registers for ldmia r3-r12
.loop_converttoL8:
	ldmia   r1!, {r3-r10} 
	strb 	r3, [r0], #1
	strb 	r4, [r0], #1
	strb 	r5, [r0], #1
	strb 	r6, [r0], #1
	strb 	r7, [r0], #1
	strb 	r8, [r0], #1
	strb 	r9, [r0], #1
	strb 	r10, [r0], #1
	subs 	r2, #8
	bne 	.loop_converttoL8
	pop     {r4-r10}
	bx 		lr
.end_asmConv_L8L8L8L8_To_L8:
.size asmConv_L8L8L8L8_To_L8, .end_asmConv_L8L8L8L8_To_L8-asmConv_L8L8L8L8_To_L8
// this is the proper way to declare function, no exception bull shit with using fnstar, fnend etc

// ############################################################################## //
