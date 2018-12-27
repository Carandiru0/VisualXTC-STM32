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

	.extern globalasm_DMA2DFrameBuffer;
	.extern globalasm_dithertable;
	.extern globalasm_DitherTargetFrameBuffer;
	
	.section .ramfunc, "ax"
	.globl   asmDitherBlit
	.p2align 2
	.thumb_func
	.type    asmDitherBlit,%function	// no input arguments
		
asmDitherBlit: // wow ~100us, > 7X improvement over c function
	push {r4-r11}
	
	ldr     r9,=globalasm_DMA2DFrameBuffer		// (u32*)
	
	pld	    =globalasm_DitherTargetFrameBuffer	// (u8**)
	
	// int32_t yy(oOLED::Height - 1);
	mov		r0, #63									// hard-coded ! 64 - 1
.rows:

	// pd=_sram_dithertable+(((yy&7)<<3)+7);
	//mov		r2, #7
	//and		r3, r0, r2				// yy&7
	//add		r2, r2, r3, lsl #3		// 7 + ((yy&7)<<3)
	// or:
	movs	r2, #7
	bfi		r2, r0, #3, #3
	
	ldr		r1, =globalasm_dithertable			// (u8[64])
	add		r1, r2
	// reset dithertable 
	
	ldr 	r2, =globalasm_DitherTargetFrameBuffer	// *(u8**)
	rsb		r3, r0, r0, lsl #4						// (y*16)-y
	ldr		r2, [r2]								// *(u8*)
	add		r2, r3, lsl #4  						// globalasm_DitherTargetFrameBuffer + (y*15)*16
	add		r2, #56									// globalasm_DitherTargetFrameBuffer + oOLED::StartXOffset
	// Moved r2 to desired row

	//uint32_t xxx(oOLED::Width);
	mov		r3, #256
.cols:
	// pd -= 8;
	sub		r1, #8
	
	//###############
	ldmia   r9!, {r4-r7}							// read 4 words - globalasm_DMA2DFrameBuffer (u32*)
	ubfx	r4, r4, #0, #8							// 1 cycle + 4 cycles for ldmia
	ubfx	r5, r5, #0, #8							// + 4 cycles = 9 - however mostly sequential cycles
	ubfx	r6, r6, #0, #8
	ubfx	r7, r7, #0, #8
	//ldrb	r4, [r9], #4							// 8 cycles however all non-sequential cycles
	//ldrb	r5, [r9], #4
	//ldrb	r6, [r9], #4
	//ldrb	r7, [r9], #4
	
	orr		r4, r5, r4, lsl #8						// r4 = (r4 << 8) | r5
	orr		r6, r7, r6, lsl #8						// r6 = (r6 << 8) | r7
	pkhbt	r5, r4, r6, lsl #16  					// r5 = (r4 << 16) | r6
	// now have word of bytes from globalasm_DMA2DFrameBuffer (u32*)
	
	ldr		r10, [r1], #4							// read word of bytes - globalasm_sram_dithertable 
	
	//PixelsQuad const a( (p0&nF0), (p1&nF0), (p2&nF0), (p3&nF0) );
	and 	r6, r5, #0xF0F0F0F0	// a
	
	//o.Set( ((*pd++)   - (p0&n0F)*n255by15)>>4&16,
	//			 ((*pd++) - (p1&n0F)*n255by15)>>4&16,
	//			 ((*pd++) - (p2&n0F)*n255by15)>>4&16,
	//			 ((*pd++) - (p3&n0F)*n255by15)>>4&16 );
	and 	r5, r5, #0x0F0F0F0F
	
	// first 4 pixels
	mov		r11, #0
	mov		r4, #16

	//((*pd)   - (p0&n0F)*n255by15)>>4&16
	ubfx	r8, r5, #24, #8		// r8 = (p0&n0F)
	add		r8, r8, r8, lsl #4	// r8 = (p0&n0F) * 16 + (p0&n0F)
	ubfx	r7, r10, #24, #8	// r7 = MSByte of sram dither table
	sub		r7, r8				// ((*pd) - (p0&n0F)*n255by15)
	and		r7, r4, r7, lsr #4	// (r7>>4)&16
	orr		r11, r7, lsl #24	// accumulate into full word
	
	//((*pd)   - (p1&n0F)*n255by15)>>4&16
	ubfx	r8, r5, #16, #8		// r8 = (p0&n0F)
	add		r8, r8, r8, lsl #4	// r8 = (p0&n0F) * 16 + (p0&n0F)
	ubfx	r7, r10, #16, #8	// r7 = MSByte of sram dither table
	sub		r7, r8				// ((*pd) - (p0&n0F)*n255by15)
	and		r7, r4, r7, lsr #4	// (r7>>4)&16
	orr		r11, r7, lsl #16	// accumulate into full word
	
	//((*pd)   - (p2&n0F)*n255by15)>>4&16
	ubfx	r8, r5, #8, #8		// r8 = (p0&n0F)
	add		r8, r8, r8, lsl #4	// r8 = (p0&n0F) * 16 + (p0&n0F)
	ubfx	r7, r10, #8, #8		// r7 = MSByte of sram dither table
	sub		r7, r8				// ((*pd) - (p0&n0F)*n255by15)
	and		r7, r4, r7, lsr #4	// (r7>>4)&16
	orr		r11, r7, lsl #8		// accumulate into full word
	
	//((*pd)   - (p3&n0F)*n255by15)>>4&16
	ubfx	r8, r5, #0, #8		// r8 = (p0&n0F)
	add		r8, r8, r8, lsl #4	// r8 = (p0&n0F) * 16 + (p0&n0F)
	ubfx	r7, r10, #0, #8		// r7 = MSByte of sram dither table
	sub		r7, r8				// ((*pd) - (p0&n0F)*n255by15)
	and		r7, r4, r7, lsr #4	// (r7>>4)&16
	orr		r11, r7				// accumulate into full word
	
	// saturating add
	//o.v = __UQADD8(a.v, o.v);
	// changed to a = a + o
	uqadd8	r12, r6, r11
	
	//##############
	ldmia   r9!, {r4-r7}							// read 4 words - globalasm_DMA2DFrameBuffer (u32*)
	ubfx	r4, r4, #0, #8							// 1 cycle + 4 cycles for ldmia
	ubfx	r5, r5, #0, #8							// + 4 cycles = 9 - however mostly sequential cycles
	ubfx	r6, r6, #0, #8
	ubfx	r7, r7, #0, #8
	//ldrb	r4, [r9], #4							// 8 cycles however all non-sequential cycles
	//ldrb	r5, [r9], #4
	//ldrb	r6, [r9], #4
	//ldrb	r7, [r9], #4
	
	orr		r4, r5, r4, lsl #8						// r4 = (r4 << 8) | r5
	orr		r6, r7, r6, lsl #8						// r6 = (r6 << 8) | r7
	pkhbt	r5, r4, r6, lsl #16  					// r5 = (r4 << 16) | r6
	// now have word of bytes from globalasm_DMA2DFrameBuffer (u32*)
	
	ldr		r10, [r1], #4		// read word of bytes - globalasm_sram_dithertable 
	
	//PixelsQuad const a( (p0&nF0), (p1&nF0), (p2&nF0), (p3&nF0) );
	and 	r6, r5, #0xF0F0F0F0	// a
	
	//o.Set( ((*pd++)   - (p0&n0F)*n255by15)>>4&16,
	//			 ((*pd++) - (p1&n0F)*n255by15)>>4&16,
	//			 ((*pd++) - (p2&n0F)*n255by15)>>4&16,
	//			 ((*pd++) - (p3&n0F)*n255by15)>>4&16 );
	and 	r5, r5, #0x0F0F0F0F
	
	// second 4 pixels
	mov		r11, #0
	mov		r4, #16

	//((*pd)   - (p0&n0F)*n255by15)>>4&16
	ubfx	r8, r5, #24, #8		// r8 = (p0&n0F)
	add		r8, r8, r8, lsl #4	// r8 = (p0&n0F) * 16 + (p0&n0F)
	ubfx	r7, r10, #24, #8	// r7 = MSByte of sram dither table
	sub		r7, r8				// ((*pd) - (p0&n0F)*n255by15)
	and		r7, r4, r7, lsr #4	// (r7>>4)&16
	orr		r11, r7, lsl #24	// accumulate into full word
	
	//((*pd)   - (p1&n0F)*n255by15)>>4&16
	ubfx	r8, r5, #16, #8		// r8 = (p0&n0F)
	add		r8, r8, r8, lsl #4	// r8 = (p0&n0F) * 16 + (p0&n0F)
	ubfx	r7, r10, #16, #8	// r7 = MSByte of sram dither table
	sub		r7, r8				// ((*pd) - (p0&n0F)*n255by15)
	and		r7, r4, r7, lsr #4	// (r7>>4)&16
	orr		r11, r7, lsl #16	// accumulate into full word
	
	//((*pd)   - (p2&n0F)*n255by15)>>4&16
	ubfx	r8, r5, #8, #8		// r8 = (p0&n0F)
	add		r8, r8, r8, lsl #4	// r8 = (p0&n0F) * 16 + (p0&n0F)
	ubfx	r7, r10, #8, #8		// r7 = MSByte of sram dither table
	sub		r7, r8				// ((*pd) - (p0&n0F)*n255by15)
	and		r7, r4, r7, lsr #4	// (r7>>4)&16
	orr		r11, r7, lsl #8		// accumulate into full word
	
	//((*pd)   - (p3&n0F)*n255by15)>>4&16
	ubfx	r8, r5, #0, #8		// r8 = (p0&n0F)
	add		r8, r8, r8, lsl #4	// r8 = (p0&n0F) * 16 + (p0&n0F)
	ubfx	r7, r10, #0, #8		// r7 = MSByte of sram dither table
	sub		r7, r8				// ((*pd) - (p0&n0F)*n255by15)
	and		r7, r4, r7, lsr #4	// (r7>>4)&16
	orr		r11, r7				// accumulate into full word
	
	// saturating add
	//o.v = __UQADD8(a.v, o.v);
	// changed to a = a + o
	uqadd8	r11, r6, r11
	
	// 1st 4 pixels in r12, 2nd 4 pixels in r11
	mov 	r4, #0
	
	/* doin this 2 times:
	// output the 2 pixels
	*po0++ = (o.pixels.p0 & nF0) | ((o.pixels.p1 >> 4) & n0F);
	// output 2 more pixels
	*po0++ = (o.pixels.p2 & nF0) | ((o.pixels.p3 >> 4) & n0F);
	*/
	
	// 2 pixels
	ubfx	r5, r11, #24, #8
	and		r5, #0xF0
	ubfx	r6, r11, #16, #8
	orr		r5, r5, r6, lsr #4
	orr		r4, r5, lsl #24
	
	// 4 pixels
	ubfx	r5, r11, #8, #8
	and		r5, #0xF0
	ubfx	r6, r11, #0, #8
	orr		r5, r5, r6, lsr #4
	orr		r4, r5, lsl #16
	
	// 6 pixels
	ubfx	r5, r12, #24, #8
	and		r5, #0xF0
	ubfx	r6, r12, #16, #8
	orr		r5, r5, r6, lsr #4
	orr		r4, r5, lsl #8
	
	// 8 pixels
	ubfx	r5, r12, #8, #8
	and		r5, #0xF0
	ubfx	r6, r12, #0, #8
	orr		r5, r5, r6, lsr #4
	orr		r4, r5
		
	// store full word result (8 pixels - 4 bytes/8 nybbles ; 1 pixel/nybble)
	str		r4, [r2], #4	// globalasm_DitherTargetFrameBuffer
	
//.cols:
	subs	r3, #8
	bne    	.cols
	
	//do => while (--yy >= 0);
//.rows:
	subs	r0, #1
	bge    	.rows
	
	pop {r4-r11}
    bx       lr           // Return by branching to the address in the link register.
.end_asmDitherBlit:
.size asmDitherBlit, .end_asmDitherBlit-asmDitherBlit
// this is the proper way to declare function, no exception bull shit with using fnstar, fnend etc




