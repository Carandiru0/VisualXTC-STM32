/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef xDMA2D_H
#define xDMA2D_H
#include "globals.h"
#include "stm32f7xx_ll_dma2d.h"
#include "commonmath.h"

namespace xDMA2D
{
	
static constexpr int32_t const DMA2D_AVAILABLE = 0,
															 DMA2D_BUSY = 1,
															 DMA2D_RESIZE_BUSY = -1;

static constexpr int32_t const DMA2D_DUALFILTERBLUR_DMA2D_AVAILABLE = 0,
															 DMA2D_DUALFILTERBLUR_DMA2D_STARTED = 1,
															 DMA2D_DUALFILTERBLUR_DMA2D_FINISHED = -1;

} // end namespace

typedef struct sDMA2D_Private
{
	atomic_strict_int32_t DMA2D_OpIsBusy,
												DMA2D_DualFilterState;
	
	sDMA2D_Private();
	
} sDMA2D_Private;
extern sDMA2D_Private DMA2D_Private
	__attribute__((section (".dtcm_atomic")));

namespace xDMA2D
{
	static constexpr uint32_t const DMA2D_TIMEOUT = 33; // in ms
	static constexpr uint32_t const BGND = 0,
																	FGND = 1;
	
	typedef union __attribute__((__aligned__(4))) uAlphaLuma
	{
	private:
		struct
		{	
			uint8_t   b,
								g,					 
								r,					
								a;
			
		} comp; // components
	public:	
		struct
		{	
			uint8_t const 	Luma,
											_Luma,	 
											__Luma, 	
											Alpha;
			
		} const pixel;
	
		__SIMDU32_TYPE v;
		
		inline uAlphaLuma()
		: v(0)
		{	}
			
		inline uAlphaLuma( uint32_t const ARGB )
			: v(ARGB)
		{}
		
		inline explicit uAlphaLuma( uint8_t const Alpha, uint8_t const Luma )
		 //: v( __PKHBT( (Alpha << 8) | Luma, (Luma << 8) | Luma, Constants::n16) ) does not work weird rendering
		{
			comp.a = Alpha; comp.b = comp.g = comp.r = Luma;
		}
		
		inline void operator=(uAlphaLuma const& rhs)
		{
			v = rhs.v;
		}
		
		inline void setAlpha(uint8_t const Alpha)
		{
			comp.a = Alpha;
		}
		inline void setLuma(uint8_t const Luma)
		{
			comp.b = comp.g = comp.r = Luma;
		}
		inline void makeOpaque()
		{
			comp.a = 0xFF;
		}
		inline void makeTransparent()
		{
			comp.a = 0;
		}
		inline uint8_t const getConvertColorToLinearGray() const
		{
			// too dark with correct conversion (sr71) can't see details
			//return( __USAT( uint32::__roundf((float)(comp.b + comp.g + comp.r) / 3.0f), Constants::SATBIT_256 ) );
			return((comp.b+comp.g+comp.r+comp.a)>>2); // this looks better dont change
		}
		//__inline void InvertLuma()
		//{
			// Load 255
		//	__SIMDU32_TYPE const v255 = __PKHBT( /*(0 << 8) |*/ Constants::n255, (Constants::n255 << 8) | Constants::n255, Constants::n16);
		//	v = __UQSUB8(v255, v);
		//}
		
		//__inline void InvertAlpha()
		//{
		//	__SIMDU32_TYPE const v255 = __PKHBT( (Constants::n255 << 8), 0, Constants::n16);
		//	v = __UQSUB8(v255, v);
		//}
	} AlphaLuma;


	typedef struct DMA2DBuffers {
		
	// DTCM size is 128KB
	// SRAM1 size is 368KB
	// SRAM2 size is 16KB, total SRAM = 372KB
	// External SRAM size is 512KB
		
		static uint32_t WorkBufferDMA2D_32bit[(OLED::SCREEN_WIDTH)*(OLED::SCREEN_HEIGHT)]					// 32bpp,     64KB      256x64
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".ext_sram.workbuffer32bit")));

		static uint8_t WorkBufferDMA2D_8bit[(OLED::SCREEN_WIDTH)*(OLED::SCREEN_HEIGHT)]						// 8bpp,     	16KB			256x64
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".ext_sram.workbuffer8bit")));

		// Full Size 32bit & 8bit (extra) Effect buffers
		static uint32_t EffectRenderBuffer_32bit[OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT]					// 32bpp,     64KB				256x64
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".bss.effectrenderbuffer32bit")));

		static uint8_t EffectRenderBuffer_8bit[OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT]						// 8bpp,     16KB				256x64
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".bss.effectrenderbuffer8bit")));
		
		// Half Size 32bit (extra) Effect buffers (Intended for downsampling/upsampling DualFilterBlur) 
		static uint32_t DualFilterTarget[(OLED::SCREEN_WIDTH>>1)*(OLED::SCREEN_HEIGHT>>1)]				// 32bpp,     16KB			128x32
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".bss.DualFilterTarget"))); //odd mproblem -white pixel artifacts on center right if this is defined in ext_sram section
	} const xDMA2DBuffers;
	
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint32_t* const __restrict getWorkBuffer_32bit();			// 32bpp,     64KB      	256x64
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint8_t* const __restrict  getWorkBuffer_8bit();			// 8bpp,     	16KB				256x64
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint32_t* const __restrict getEffectBuffer_32bit();	  // 32bpp,     64KB				256x64
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint8_t* const __restrict  getEffectBuffer_8bit();		// 8bpp,     	16KB				256x64
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint32_t* const __restrict getDualFilterTarget();			// 32bpp,     16KB				128x32
	
typedef void (* const initDMA2D_op_staticconfig)(void); // signature for OLED DMA2D "Initial" Configuration
NOINLINE void Init( initDMA2D_op_staticconfig const opConfig );
void					Interrupt_ISR();
	
__attribute__((pure)) STATIC_INLINE bool const IsBusy_DMA2D()
{
	// Test register for DMA2D, and the special case of a resize operation only
	return( LL_DMA2D_IsTransferOngoing(DMA2D) | (DMA2D_Private.DMA2D_OpIsBusy < 0) );
}
template<bool const bUseTimeout = false>
STATIC_INLINE void Wait_DMA2D()
{	
	if constexpr (bUseTimeout)
	{
		uint32_t const tStart = millis();
	
		// Wait foor current operation to finish
		while ( 0 != LL_DMA2D_IsTransferOngoing(DMA2D)) { 
			
			if ( millis() - tStart > DMA2D_TIMEOUT ) {
				LL_DMA2D_Abort(DMA2D);
				break;
			}
			
		}	// unknown, WFI here prevents freeze
	}
	else {
		// Wait foor current operation to finish
		while ( 0 != LL_DMA2D_IsTransferOngoing(DMA2D)) {}
	}
	
	// Check further if resize is currently in progress
	if ( DMA2D_Private.DMA2D_OpIsBusy < 0 ) {	// DMA2D_RESIZE_BUSY == -1
		if constexpr (bUseTimeout)
		{
			uint32_t const tStart = millis();
		
			// Wait foor current operation to finish
			while ( DMA2D_AVAILABLE != DMA2D_Private.DMA2D_OpIsBusy ) { 
				
				if ( millis() - tStart > DMA2D_TIMEOUT ) {
					LL_DMA2D_Abort(DMA2D);
					break;
				}
				
			}	// unknown, WFI here prevents freeze
		}
		else {
			// Wait foor current operation to finish
			while ( DMA2D_AVAILABLE != DMA2D_Private.DMA2D_OpIsBusy ) {}
		}
	}
	DMA2D_Private.DMA2D_OpIsBusy = DMA2D_AVAILABLE;	// Reset upon wait return
}
template<bool const bWaitForCompletion = true, bool const bUseTimeout = false>
STATIC_INLINE void Start_DMA2D()
{		
	/* Start transfer */
	DMA2D_Private.DMA2D_OpIsBusy = DMA2D_BUSY;
	LL_DMA2D_Start(DMA2D);
	
	if constexpr ( bWaitForCompletion ) {
		// Wait foor current operation to finish
		Wait_DMA2D<bUseTimeout>();
	}
}
STATIC_INLINE void DMA2D_SetMemory_Target(uint32_t const MemoryAddress,
																					uint32_t const Offset, uint32_t const NumberOfLine, uint32_t const PixelPerLine) 
{	
	/* Set memory settings */
	
	// TM_INT_DMA2DGRAPHIC_SetMemory(DIS.PixelSize * (y * DIS.Width + x), DIS.Width - width, height, width);
	// Working_address = Target_Address + (X +  Target_Width * Y) * BPP
	// LineOffset = Target_Width - Source_Width
	
	LL_DMA2D_ConfigSize(DMA2D, NumberOfLine, PixelPerLine);
	
	LL_DMA2D_SetOutputMemAddr(DMA2D, MemoryAddress );
	LL_DMA2D_SetLineOffset(DMA2D, Offset);
}
STATIC_INLINE void DMA2D_SetMemory_BGND(uint32_t const MemoryAddress,
																			  uint32_t const Offset) 
{	
	/* Set memory settings */
	
	// background source memory
	LL_DMA2D_BGND_SetMemAddr(DMA2D, MemoryAddress);
	LL_DMA2D_BGND_SetLineOffset(DMA2D, Offset);
}
STATIC_INLINE void DMA2D_SetMemory_TargetIsBGND(uint32_t const MemoryAddress,
																					      uint32_t const Offset, uint32_t const NumberOfLine, uint32_t const PixelPerLine) 
{	
	/* Set memory settings */
	
	// TM_INT_DMA2DGRAPHIC_SetMemory(DIS.PixelSize * (y * DIS.Width + x), DIS.Width - width, height, width);
	// Working_address = Target_Address + (X +  Target_Width * Y) * BPP
	// LineOffset = Target_Width - Source_Width
	
	DMA2D_SetMemory_Target(MemoryAddress, Offset, NumberOfLine, PixelPerLine);
	DMA2D_SetMemory_BGND(MemoryAddress, Offset);
}
STATIC_INLINE void DMA2D_SetMemory_FGND(uint32_t const MemoryAddress,
																			  uint32_t const Offset) 
{	
	/* Set memory settings */
	
	// foreground source memory
	LL_DMA2D_FGND_SetMemAddr(DMA2D, MemoryAddress);
	LL_DMA2D_FGND_SetLineOffset(DMA2D, Offset);
}

template<uint32_t const TargetLayer> // xDMA2D::BGND or xDMA2D::FGND
STATIC_INLINE void ConfigureLayer_ForDefaultBlendMode(DMA2D_TypeDef * const __restrict DMA2Dx) // ARGB8888 background layer setup as needed for simple blending
{
	if constexpr (xDMA2D::BGND == TargetLayer)
  {
		static constexpr uint32_t const DMA2D_REG_BGND_SELECT = (DMA2D_BGPFCCR_ALPHA | DMA2D_BGPFCCR_AI | DMA2D_BGPFCCR_AM | DMA2D_BGPFCCR_CM),
																		DMA2D_REG_BGND_BITS = ((0xFF << DMA2D_BGPFCCR_ALPHA_Pos) | LL_DMA2D_ALPHA_REGULAR | LL_DMA2D_ALPHA_MODE_NO_MODIF | LL_DMA2D_INPUT_MODE_ARGB8888);
		/* Configure the background Alpha value, Alpha mode, Alpha inversion
			 and Color mode */
		MODIFY_REG(DMA2Dx->BGPFCCR, DMA2D_REG_BGND_SELECT, DMA2D_REG_BGND_BITS);

		/* Configure the background color */
		CLEAR_REG(DMA2Dx->BGCOLR);
	}
	else
	{
		static constexpr uint32_t const DMA2D_REG_FGND_SELECT = (DMA2D_FGPFCCR_ALPHA | DMA2D_FGPFCCR_AI | DMA2D_FGPFCCR_AM | DMA2D_FGPFCCR_CM),
																		DMA2D_REG_FGND_BITS = ((0xFF << DMA2D_FGPFCCR_ALPHA_Pos) | LL_DMA2D_ALPHA_REGULAR | LL_DMA2D_ALPHA_MODE_NO_MODIF | LL_DMA2D_INPUT_MODE_ARGB8888);
		/* Configure the background Alpha value, Alpha mode, Alpha inversion
			 and Color mode */
		MODIFY_REG(DMA2Dx->FGPFCCR, DMA2D_REG_FGND_SELECT, DMA2D_REG_FGND_BITS);

		/* Configure the background color */
		CLEAR_REG(DMA2Dx->FGCOLR);
	}
}

template<uint32_t const TargetLayer> // xDMA2D::BGND or xDMA2D::FGND
STATIC_INLINE void ConfigureLayer_ForMemMode(DMA2D_TypeDef * const __restrict DMA2Dx, uint32_t const ColorMode)
{
  if constexpr (xDMA2D::BGND == TargetLayer)
  {
		static constexpr uint32_t const DMA2D_REG_BGND_SELECT = (DMA2D_BGPFCCR_ALPHA | DMA2D_BGPFCCR_AI | DMA2D_BGPFCCR_AM | DMA2D_BGPFCCR_CM),
																	  DMA2D_REG_BGND_BITS = ((0xFF << DMA2D_BGPFCCR_ALPHA_Pos) | LL_DMA2D_ALPHA_REGULAR | LL_DMA2D_ALPHA_MODE_NO_MODIF);
	
		/* Configure the background Alpha value, Alpha mode, Alpha inversion
		 and Color mode */
		MODIFY_REG(DMA2Dx->BGPFCCR, DMA2D_REG_BGND_SELECT, DMA2D_REG_BGND_BITS | ColorMode);
		
    CLEAR_REG(DMA2Dx->BGCOLR);

    /* Configure the background color */
    //MODIFY_REG(DMA2Dx->BGCOLR, (DMA2D_BGCOLR_RED | DMA2D_BGCOLR_GREEN | DMA2D_BGCOLR_BLUE), \
    //         ((Shade << DMA2D_BGCOLR_RED_Pos) | (Shade << DMA2D_BGCOLR_GREEN_Pos) | Shade));
  }
  else
  {

    static constexpr uint32_t const DMA2D_REG_FGND_SELECT = (DMA2D_FGPFCCR_ALPHA | DMA2D_FGPFCCR_AI | DMA2D_FGPFCCR_AM | DMA2D_FGPFCCR_CM),
																	  DMA2D_REG_FGND_BITS = ((0xFF << DMA2D_FGPFCCR_ALPHA_Pos) | LL_DMA2D_ALPHA_REGULAR | LL_DMA2D_ALPHA_MODE_NO_MODIF);
	
		/* Configure the foreground Alpha value, Alpha mode, Alpha inversion
		 and Color mode */
		MODIFY_REG(DMA2Dx->FGPFCCR, DMA2D_REG_FGND_SELECT, DMA2D_REG_FGND_BITS | ColorMode);
		
    CLEAR_REG(DMA2Dx->FGCOLR);

    /* Configure the foreground color */
    //MODIFY_REG(DMA2Dx->BGCOLR, (DMA2D_BGCOLR_RED | DMA2D_BGCOLR_GREEN | DMA2D_BGCOLR_BLUE), \
    //         ((Shade << DMA2D_BGCOLR_RED_Pos) | (Shade << DMA2D_BGCOLR_GREEN_Pos) | Shade));
  }
}

template<uint32_t const TargetLayer> // xDMA2D::BGND or xDMA2D::FGND
STATIC_INLINE void ConfigureLayer_ForBlendMode(DMA2D_TypeDef * const __restrict DMA2Dx, uint32_t const ColorMode, uint32_t const AlphaMode, 
																							 uint32_t const Shade = 0, uint32_t const Alpha = 0xFF )
{
  if constexpr (xDMA2D::BGND == TargetLayer)
  {
    /* Configure the background Alpha value, Alpha mode, Alpha inversion
       CLUT size, CLUT Color mode and Color mode */
    MODIFY_REG(DMA2Dx->BGPFCCR, \
               (DMA2D_BGPFCCR_ALPHA | DMA2D_BGPFCCR_AI | DMA2D_BGPFCCR_AM | \
                DMA2D_BGPFCCR_CM), \
               ((Alpha << DMA2D_BGPFCCR_ALPHA_Pos) | \
                LL_DMA2D_ALPHA_REGULAR | AlphaMode | \
                ColorMode));

    /* Configure the background color */
    MODIFY_REG(DMA2Dx->BGCOLR, (DMA2D_BGCOLR_RED | DMA2D_BGCOLR_GREEN | DMA2D_BGCOLR_BLUE), \
             ((Shade << DMA2D_BGCOLR_RED_Pos) | (Shade << DMA2D_BGCOLR_GREEN_Pos) | Shade));
  }
  else
  {

    /* Configure the foreground Alpha value, Alpha mode, Alpha inversion
       CLUT size, CLUT Color mode and Color mode */
    MODIFY_REG(DMA2Dx->FGPFCCR, \
               (DMA2D_FGPFCCR_ALPHA | DMA2D_FGPFCCR_AI | DMA2D_FGPFCCR_AM | \
                DMA2D_FGPFCCR_CM), \
               ((Alpha << DMA2D_FGPFCCR_ALPHA_Pos) | \
                LL_DMA2D_ALPHA_REGULAR | AlphaMode | \
                ColorMode));

    /* Configure the foreground color */
    MODIFY_REG(DMA2Dx->FGCOLR, (DMA2D_FGCOLR_RED | DMA2D_FGCOLR_GREEN | DMA2D_FGCOLR_BLUE), \
             ((Shade << DMA2D_FGCOLR_RED_Pos) | (Shade << DMA2D_FGCOLR_GREEN_Pos) | Shade));
  }
}

template<bool const bWaitForPreviousDMA2DOp = true>
STATIC_INLINE void DrawVMemLine(uint32_t const MemoryAddressTarget, uint32_t const MemoryAddressSource, uint32_t const NumPixels)
{
	if constexpr ( bWaitForPreviousDMA2DOp ) {
		Wait_DMA2D<false>();	// wait for previous dma2d op to complete
	}
	
	ConfigureLayer_ForMemMode<FGND>(DMA2D, LL_DMA2D_INPUT_MODE_ARGB8888);

	/* Configure DMA2D output color mode */
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_M2M);
	
	DMA2D_SetMemory_FGND(MemoryAddressSource, 0);
	
	DMA2D_SetMemory_Target(MemoryAddressTarget, OLED::SCREEN_WIDTH - 1, NumPixels/*NumberOfLine*/, 1/*PixelPerLine*/);
	
	/* Start transfer */
	Start_DMA2D<false>();
}

STATIC_INLINE void DrawVMemLine_Blended(uint32_t const MemoryAddressTarget, uint32_t const MemoryAddressSource, uint32_t const NumPixels, uint32_t const Shade)
{
	Wait_DMA2D<false>();	// wait for previous dma2d op to complete
	
	ConfigureLayer_ForDefaultBlendMode<BGND>(DMA2D);
	ConfigureLayer_ForBlendMode<FGND>(DMA2D, LL_DMA2D_INPUT_MODE_A8, LL_DMA2D_ALPHA_MODE_NO_MODIF, Shade, 0xFF);

	/* Configure DMA2D output color mode */
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_M2M_BLEND);
	
	DMA2D_SetMemory_FGND(MemoryAddressSource, 0);
	
	DMA2D_SetMemory_TargetIsBGND(MemoryAddressTarget, OLED::SCREEN_WIDTH - 1, NumPixels/*NumberOfLine*/, 1/*PixelPerLine*/);
	
	/* Start transfer */
	Start_DMA2D<false>();
}

template<bool const isBufferDTCM = false, bool const bWaitForCompletion = true, bool const bUseTimeout = false, uint32_t xWIDTH = OLED::SCREEN_WIDTH, uint32_t xHEIGHT = OLED::SCREEN_HEIGHT>
STATIC_INLINE void ClearBuffer_32bit(uint32_t* const __restrict pDstBufferARGB8888)
{
	/* Configure DMA2D output color mode */
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_R2M);
	LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_ARGB8888);	//(1pixels/CLK)
	
	LL_DMA2D_ConfigSize(DMA2D, xHEIGHT, xWIDTH);
	LL_DMA2D_SetLineOffset(DMA2D, 0);
	LL_DMA2D_SetOutputMemAddr(DMA2D, (uint32_t)pDstBufferARGB8888);
	
	LL_DMA2D_SetOutputColor(DMA2D, 0x00000000);
	
	xDMA2D::Start_DMA2D<bWaitForCompletion, bUseTimeout>();
	if constexpr( !isBufferDTCM ) {
		SCB_InvalidateDCache_by_Addr( (uint32_t*)pDstBufferARGB8888, OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT*sizeof(uint32_t)); // not neccessary for DTCM
	}
}
template<bool const isBufferDTCM = false, bool const bWaitForCompletion = true, bool const bUseTimeout = false, uint32_t xWIDTH = OLED::SCREEN_WIDTH, uint32_t xHEIGHT = OLED::SCREEN_HEIGHT>
STATIC_INLINE void ClearBuffer_8bit(uint8_t* const __restrict pDstBufferL8)
{
	/* Configure DMA2D output color mode */
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_R2M);
	LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_ARGB8888);  // lying to dma2d, output is actually L8
																																			// but width/4 to make L8L8L8L8 (4pixels/CLK)
	LL_DMA2D_ConfigSize(DMA2D, xHEIGHT, xWIDTH>>2);						// pixelsperline must be even number, div by 4
	LL_DMA2D_SetLineOffset(DMA2D, 0);
	LL_DMA2D_SetOutputMemAddr(DMA2D, (uint32_t)pDstBufferL8);
	
	LL_DMA2D_SetOutputColor(DMA2D, 0x00000000);
	
	xDMA2D::Start_DMA2D<bWaitForCompletion, bUseTimeout>();
	if constexpr( !isBufferDTCM ) {
		SCB_InvalidateDCache_by_Addr( (uint32_t*)pDstBufferL8, OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT*sizeof(uint8_t)); // not neccessary for DTCM
	}
}

template<bool const isBufferDTCM = false, bool const bWaitForCompletion = true, bool const bUseTimeout = false, uint32_t xWIDTH = OLED::SCREEN_WIDTH, uint32_t xHEIGHT = OLED::SCREEN_HEIGHT>
STATIC_INLINE void FillBuffer_32bit(uint32_t* const __restrict pDstBufferARGB8888, xDMA2D::AlphaLuma const AlphaLuma)
{
	/* Configure DMA2D output color mode */
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_R2M);
	LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_ARGB8888);  
																																			
	LL_DMA2D_ConfigSize(DMA2D, xHEIGHT, xWIDTH);						
	LL_DMA2D_SetLineOffset(DMA2D, 0);
	LL_DMA2D_SetOutputMemAddr(DMA2D, (uint32_t)pDstBufferARGB8888);
	
	LL_DMA2D_SetOutputColor(DMA2D, AlphaLuma.v);
	
	xDMA2D::Start_DMA2D<bWaitForCompletion, bUseTimeout>();
	
	if constexpr( !isBufferDTCM ) {
		SCB_InvalidateDCache_by_Addr( (uint32_t*)pDstBufferARGB8888, xWIDTH*xHEIGHT*sizeof(uint32_t)); // not neccessary for DTCM
	}
}
template<bool const isBufferDTCM = false, bool const bWaitForCompletion = true, bool const bUseTimeout = false, uint32_t xWIDTH = OLED::SCREEN_WIDTH, uint32_t xHEIGHT = OLED::SCREEN_HEIGHT>
STATIC_INLINE void FillBuffer_8bit(uint8_t* const __restrict pDstBufferL8, uint32_t const Luma)
{
	/* Configure DMA2D output color mode */
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_R2M);
	LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_ARGB8888);  // lying to dma2d, output is actually L8
																																			// but width/4 to make L8L8L8L8
	LL_DMA2D_ConfigSize(DMA2D, xHEIGHT, xWIDTH>>2);						// pixelsperline must be even number, div by 4
	LL_DMA2D_SetLineOffset(DMA2D, 0);
	LL_DMA2D_SetOutputMemAddr(DMA2D, (uint32_t)pDstBufferL8);
	
	LL_DMA2D_SetOutputColor(DMA2D, Luma);
	
	xDMA2D::Start_DMA2D<bWaitForCompletion, bUseTimeout>();
	
	if constexpr( !isBufferDTCM ) {
		SCB_InvalidateDCache_by_Addr( (uint32_t*)pDstBufferL8, xWIDTH*xHEIGHT*sizeof(uint8_t)); // not neccessary for DTCM
	}
}

/* used to sequence the operations correctly, for chaining resize ops */
typedef void (* const do_chain_op)(uint32_t const, uint32_t const, uint32_t const, uint32_t const);
/* resize callback - return chain callback if chained to another resize op, 1st: Original Source Address, 2nd: Output Address, 3rd: Output Width, 4th: Output Height*/
typedef do_chain_op (*callback_done)(uint32_t const, uint32_t const, uint32_t const, uint32_t const);


typedef struct
{  
	uint32_t SourcePitch;       /* source pixel pitch */  
  uint32_t SourceColorMode;   /* source color mode */
  uint32_t SourceX;           /* souce X */  
  uint32_t SourceY;           /* sourceY */
  uint32_t SourceWidth;       /* source width */ 
  uint32_t SourceHeight;      /* source height */
  
  uint32_t OutputPitch;       /* output pixel pitch */  
  uint32_t OutputX;           /* output X */  
  uint32_t OutputY;           /* output Y */
  uint32_t OutputWidth;       /* output width */ 
  uint32_t OutputHeight;      /* output height */
	
} RESIZE_InitTypedef;

static constexpr uint32_t const
  D2D_STAGE_IDLE=0,
  D2D_STAGE_FIRST_LOOP=1,
  D2D_STAGE_2ND_LOOP=2,
  D2D_STAGE_DONE=3,
  D2D_STAGE_ERROR=4,
  D2D_STAGE_SETUP_BUSY=5,
  D2D_STAGE_SETUP_DONE=6;


/* resize setup */
uint32_t const D2D_Resize_Setup(uint32_t const OutputBaseAddress, uint32_t const SourceBaseAddress,
																RESIZE_InitTypedef const* const __restrict R, callback_done const	UserCallback_Finished = nullptr);

/* resize stage inquire */
uint32_t const D2D_Resize_Stage(void);

bool const DualFilterBlur_BeginAsync(uint8_t const* const __restrict blurSource);
void DualFilterBlur_EndAsync();

/* hardware accelerated downsample and upsample (by 2) with bilinear interpolation */
STATIC_INLINE void DownSample_8bit(uint32_t* const __restrict target, uint8_t const* const __restrict source,
																	 uint32_t const SourceWidth, uint32_t const SourceHeight, callback_done const onDone = nullptr)
{
	xDMA2D::RESIZE_InitTypedef Resize = {
		
		SourceWidth>>2,     									// source pixel pitch //  ** using LL_DMA2D_INPUT_MODE_ARGB8888 for L8/A8 input, only way this seems to work
		LL_DMA2D_INPUT_MODE_ARGB8888,      		// source color mode //
		0,             												// souce X //  
		0,              											// sourceY //
		SourceWidth>>2,            						// source width // 
		SourceHeight,            							// source height //
		
		SourceWidth>>1,            						// output pixel pitch //  
		0,              											// output X //  
		0,              											// output Y //
		SourceWidth>>1,            						// output width // 
		SourceHeight>>1            						// output height //
	};
	// output bitmap Base Address // source bitmap Base Address // resize desc // "done" callback (optional) //
	xDMA2D::D2D_Resize_Setup((uint32_t)target, (uint32_t)source, &Resize, onDone);
}

STATIC_INLINE void DownSample_32bit(uint32_t* const __restrict target, uint32_t const* const __restrict source,
																	  uint32_t const SourceWidth, uint32_t const SourceHeight, callback_done const onDone = nullptr)
{
	xDMA2D::RESIZE_InitTypedef Resize = {
			
		SourceWidth,     											// source pixel pitch //
		LL_DMA2D_INPUT_MODE_ARGB8888,      		// source color mode //
		0,             												// souce X //  
		0,              											// sourceY //
		SourceWidth,            							// source width // 
		SourceHeight,            							// source height //
		
		SourceWidth>>1,            						// output pixel pitch //  
		0,              											// output X //  
		0,              											// output Y //
		SourceWidth>>1,            						// output width // 
		SourceHeight>>1            						// output height //
	};

	// output bitmap Base Address // source bitmap Base Address // resize desc // "done" callback (optional) //
	xDMA2D::D2D_Resize_Setup((uint32_t)target, (uint32_t)source, &Resize, onDone);
}

STATIC_INLINE void UpSample_8bit(uint32_t* const __restrict target, uint8_t const* const __restrict source,
																 uint32_t const SourceWidth, uint32_t const SourceHeight, callback_done const onDone = nullptr)
{
	xDMA2D::RESIZE_InitTypedef Resize = {

		SourceWidth>>2,     									// source pixel pitch //  ** using LL_DMA2D_INPUT_MODE_ARGB8888 for L8/A8 input, only way this seems to work
		LL_DMA2D_INPUT_MODE_ARGB8888,      		// source color mode //
		0,             												// souce X //  
		0,              											// sourceY //
		SourceWidth>>2,            						// source width // 
		SourceHeight,            							// source height //
		
		SourceWidth<<1,            						// output pixel pitch //  
		0,              											// output X //  
		0,              											// output Y //
		SourceWidth<<1,            						// output width // 
		SourceHeight<<1            						// output height //
	};

	// output bitmap Base Address // source bitmap Base Address // resize desc // "done" callback (optional) //
	xDMA2D::D2D_Resize_Setup((uint32_t)target, (uint32_t)source, &Resize, onDone);
}

STATIC_INLINE void UpSample_32bit(uint32_t* const __restrict target, uint32_t const* const __restrict source,
																	uint32_t const SourceWidth, uint32_t const SourceHeight, callback_done const onDone = nullptr)
{
	xDMA2D::RESIZE_InitTypedef Resize = {

		SourceWidth,     											// source pixel pitch //
		LL_DMA2D_INPUT_MODE_ARGB8888,      		// source color mode //
		0,             												// souce X //  
		0,              											// sourceY //
		SourceWidth,            							// source width // 
		SourceHeight,            							// source height //
		
		SourceWidth<<1,            						// output pixel pitch //  
		0,              											// output X //  
		0,              											// output Y //
		SourceWidth<<1,            						// output width // 
		SourceHeight<<1            						// output height //
	};

	// output bitmap Base Address // source bitmap Base Address // resize desc // "done" callback (optional) //
	xDMA2D::D2D_Resize_Setup((uint32_t)target, (uint32_t)source, &Resize, onDone);
}

} //end namespace DMA2D




#endif


