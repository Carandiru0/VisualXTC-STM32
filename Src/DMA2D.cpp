/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#include "globals.h"
#include "DMA2D.hpp"
#include "stm32f7xx_ll_bus.h"
#include "oled.h"

extern "C" void asmConv_L8L8L8L8_To_L8(uint8_t* __restrict outputL8, uint32_t const* __restrict inputLumaReplicated, uint32_t length);

using namespace xDMA2D;

sDMA2D_Private::sDMA2D_Private()
: DMA2D_OpIsBusy(0), DMA2D_DualFilterState(0)
{
	
}

sDMA2D_Private DMA2D_Private
	__attribute__((section (".dtcm_atomic")));

namespace xDMA2D
{
static xDMA2DBuffers oDMA2DBuffers
	__attribute__((section (".dtcm_const")));

uint32_t DMA2DBuffers::WorkBufferDMA2D_32bit[(OLED::SCREEN_WIDTH)*(OLED::SCREEN_HEIGHT)]					// 32bpp,     64KB      256x64
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
__attribute__((section (".ext_sram.workbuffer32bit")));

uint8_t DMA2DBuffers::WorkBufferDMA2D_8bit[(OLED::SCREEN_WIDTH)*(OLED::SCREEN_HEIGHT)]						// 8bpp,     	16KB				256x64
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
__attribute__((section (".ext_sram.workbuffer8bit")));

uint32_t DMA2DBuffers::EffectRenderBuffer_32bit[OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT]						// 32bpp,     64KB				256x64
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".bss.effectrenderbuffer32bit")));

uint8_t DMA2DBuffers::EffectRenderBuffer_8bit[OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT]							// 8bpp,     16KB				256x64
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".bss.effectrenderbuffer8bit")));

// Half Size 32bit Effect buffers (Intended for downsampling/upsampling DualFilterBlur) 
uint32_t DMA2DBuffers::DualFilterTarget[(OLED::SCREEN_WIDTH>>1)*(OLED::SCREEN_HEIGHT>>1)]					// 32bpp,     16KB				128x32
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
__attribute__((section (".bss.DualFilterTarget")));

__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint32_t* const __restrict getWorkBuffer_32bit()
{
	return(oDMA2DBuffers.WorkBufferDMA2D_32bit);
}
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint8_t* const __restrict  getWorkBuffer_8bit()
{
	return(oDMA2DBuffers.WorkBufferDMA2D_8bit);
}
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint32_t* const __restrict  getEffectBuffer_32bit()
{
	return(oDMA2DBuffers.EffectRenderBuffer_32bit);
}
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint8_t* const __restrict  getEffectBuffer_8bit()
{
	return(oDMA2DBuffers.EffectRenderBuffer_8bit);
}
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint32_t* const __restrict getDualFilterTarget()
{
	return(oDMA2DBuffers.DualFilterTarget);
}

STATIC_INLINE void EnableIT_TC()	// for only catching the Transfer Complete Interrupt when we want it
{
	LL_DMA2D_EnableIT_TC(DMA2D);
}
STATIC_INLINE void DisableIT_TC()
{
	LL_DMA2D_DisableIT_TC(DMA2D);
}

NOINLINE void Init( initDMA2D_op_staticconfig const opConfig )
{
	DMA2D_Private.DMA2D_OpIsBusy = DMA2D_AVAILABLE;
	
	/* (1) Enable peripheral clock for DMA2D  ***********************************/
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2D);

	NVIC_SetPriority(DMA2D_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_HIGH, 0));
	NVIC_EnableIRQ(DMA2D_IRQn);
	LL_DMA2D_EnableIT_CE(DMA2D);
	LL_DMA2D_EnableIT_TE(DMA2D);
	
	DMA2D_Private.DMA2D_OpIsBusy = DMA2D_BUSY;
	opConfig(); // run the initial DMA2D config op, loads the CLUT's
	DMA2D_Private.DMA2D_OpIsBusy = DMA2D_AVAILABLE;
	
	// Clear Memory Buffers
	ClearBuffer_32bit(oDMA2DBuffers.WorkBufferDMA2D_32bit);
	ClearBuffer_32bit(oDMA2DBuffers.EffectRenderBuffer_32bit);
	ClearBuffer_32bit<false, true, false, (OLED::SCREEN_WIDTH>>1), (OLED::SCREEN_HEIGHT>>1)>(oDMA2DBuffers.DualFilterTarget);
	ClearBuffer_8bit(oDMA2DBuffers.WorkBufferDMA2D_8bit);
	ClearBuffer_8bit(oDMA2DBuffers.EffectRenderBuffer_8bit);
	
	// Disabling to be safe 
	LL_DMA2D_DisableIT_TW(DMA2D);
	LL_DMA2D_DisableIT_CAE(DMA2D);
	LL_DMA2D_DisableIT_CAE(DMA2D);
	LL_DMA2D_DisableIT_CTC(DMA2D);
	DisableIT_TC();
}

/* Setup FG/BG address and FGalpha for linear blend, start DMA2D */

/* resize loop parameter */
typedef struct
{
  uint32_t Counter;           /* Loop Counter */
  uint32_t BaseAddress;       /* Loop Base Address */
  uint32_t BlendIndex;        /* Loop Blend index (Q21) */
  uint32_t BlendCoeff;        /* Loop Blend coefficient (Q21) */
  uint32_t SourcePitchBytes;  /* Loop Source pitch bytes */
  uint32_t OutputPitchBytes;  /* Loop Output pitch bytes */
}D2D_Loop_Typedef;  

/*Current resize stage*/
static atomic_strict_uint32_t  D2D_Loop_Stage __attribute__((section (".dtcm_atomic")))(D2D_STAGE_IDLE);

/*First loop parameter*/
static D2D_Loop_Typedef   D2D_First_Loop __attribute__((section (".dtcm")));
/*2nd loop parameter*/
static D2D_Loop_Typedef   D2D_2nd_Loop __attribute__((section (".dtcm")));

/*current parameter pointer*/
static D2D_Loop_Typedef* __restrict D2D_Loop __attribute__((section (".dtcm")))(&D2D_First_Loop);


/* storage of misc. parameter for 2nd loop DMA2D register setup */
static struct
{
	uint32_t SourceBaseAddress; 							/* original source bitmap Base Address */
  uint32_t OutputBaseAddress; 							/* output bitmap Base Address */
  uint32_t OutputWidth, OutputHeight;       /* output height */
  uint32_t OutputPitch;       							/* output pixel pitch */
  uint32_t SourceWidth;       							/* source width */ 
	
	callback_done 		UserCallback_Finished;
	
} D2D_Misc_Param __attribute__((section (".dtcm")));


/* Setup FG/BG address and FGalpha for linear blend, start DMA2D */
STATIC_INLINE void D2D_Blend_Line(void)
{
	/* Integer part of BlendIndex (Q21) is the first line number */
	/* 8 MSB of fractional part as FG alpha (Blend factor) */
  uint32_t const FirstLine(D2D_Loop->BlendIndex>>21), FGalpha(D2D_Loop->BlendIndex>>13);

  /* calculate and setup address for first and 2nd lines */ 
	DMA2D_Private.DMA2D_OpIsBusy = DMA2D_RESIZE_BUSY;
	LL_DMA2D_BGND_SetMemAddr(DMA2D, D2D_Loop->BaseAddress + FirstLine * D2D_Loop->SourcePitchBytes);
	LL_DMA2D_FGND_SetMemAddr(DMA2D, LL_DMA2D_BGND_GetMemAddr(DMA2D) + D2D_Loop->SourcePitchBytes);
 
	LL_DMA2D_FGND_SetAlpha(DMA2D, FGalpha);
  
	/* restart DMA2D transfer*/
 
	/* Start transfer */
	LL_DMA2D_Start(DMA2D);		// special case start of DMA2D
}
/* 

D2D_Resize_Setup() Setup and start the resize process.
parameter:         RESIZE_InitTypedef structure  
return value:      D2D_STAGE_SETUP_DONE if process start or D2D_STAGE_SETUP_BUSY 
                   when a resize process already in progress

*/
uint32_t const D2D_Resize_Setup(uint32_t const OutputBaseAddress, uint32_t const SourceBaseAddress,
																RESIZE_InitTypedef const* const __restrict R, callback_done const	UserCallback_Finished)
{
  uint32_t PixelBytes, PitchBytes;
  uint32_t BaseAddress;

  /* Test for loop already in progress */
  if(D2D_Loop_Stage != D2D_STAGE_IDLE)
    return (D2D_STAGE_SETUP_BUSY);
	
	DMA2D_Private.DMA2D_OpIsBusy = DMA2D_RESIZE_BUSY;
	
  /* DMA2D operation mode */ 
  LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_M2M_BLEND);	// includes PFC if input source format is different from target output
	LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_ARGB8888);
	
  /* DMA2D enable ISR */
	EnableIT_TC();
    
  /* first loop parameter init */
	PixelBytes 											= 4;
  PitchBytes                      = R->SourcePitch * PixelBytes;
  BaseAddress                     = SourceBaseAddress + 
                                    R->SourceY * PitchBytes + R->SourceX * PixelBytes;
	
	SCB_CleanDCache_by_Addr((uint32_t*)SourceBaseAddress, R->SourceWidth*R->SourceHeight*PixelBytes); //important
	
  D2D_First_Loop.Counter          = R->OutputHeight;
  D2D_First_Loop.SourcePitchBytes = PitchBytes;
  D2D_First_Loop.OutputPitchBytes = R->SourceWidth<<2;  
  D2D_First_Loop.BaseAddress      = BaseAddress;
  D2D_First_Loop.BlendCoeff       = ((R->SourceHeight-1)<<21) / R->OutputHeight;
  D2D_First_Loop.BlendIndex       = D2D_First_Loop.BlendCoeff>>1; 

	xDMA2D::ConfigureLayer_ForBlendMode<xDMA2D::FGND>(DMA2D, R->SourceColorMode, LL_DMA2D_ALPHA_MODE_REPLACE, 0, 0);
	xDMA2D::ConfigureLayer_ForDefaultBlendMode<xDMA2D::BGND>(DMA2D);
	
	LL_DMA2D_SetNbrOfLines(DMA2D, 1);
	LL_DMA2D_SetNbrOfPixelsPerLines(DMA2D, R->SourceWidth);
	//DMA2D->NLR                      = (1 | (R->SourceWidth<<16));
	LL_DMA2D_SetLineOffset(DMA2D, 0);
	LL_DMA2D_FGND_SetLineOffset(DMA2D, 0);
	LL_DMA2D_BGND_SetLineOffset(DMA2D, 0);
	LL_DMA2D_SetOutputMemAddr(DMA2D, (uint32_t)oDMA2DBuffers.WorkBufferDMA2D_32bit);

  /* 2nd loop parameter init */
  PixelBytes                      = 4;	// only supporting ARGB8888 for output
  PitchBytes                      = R->OutputPitch * PixelBytes;
  BaseAddress                     = OutputBaseAddress +
                                      R->OutputY * PitchBytes + R->OutputX * PixelBytes;
    
  D2D_2nd_Loop.Counter            = R->OutputWidth;
  D2D_2nd_Loop.SourcePitchBytes   = 4;
  D2D_2nd_Loop.OutputPitchBytes   = PixelBytes;
  D2D_2nd_Loop.BaseAddress        = (uint32_t)oDMA2DBuffers.WorkBufferDMA2D_32bit;
  D2D_2nd_Loop.BlendCoeff         = ((R->SourceWidth-1)<<21) / R->OutputWidth;
  D2D_2nd_Loop.BlendIndex         = D2D_2nd_Loop.BlendCoeff>>1; 
  
	D2D_Misc_Param.SourceBaseAddress= SourceBaseAddress;
  D2D_Misc_Param.OutputBaseAddress= OutputBaseAddress;
	D2D_Misc_Param.OutputWidth			= R->OutputWidth;
  D2D_Misc_Param.OutputHeight     = R->OutputHeight;
  D2D_Misc_Param.OutputPitch      = R->OutputPitch;
  D2D_Misc_Param.SourceWidth      = R->SourceWidth;
	D2D_Misc_Param.UserCallback_Finished = UserCallback_Finished;
	
  /* start first loop stage */
  D2D_Loop = &D2D_First_Loop;
  D2D_Loop_Stage = D2D_STAGE_FIRST_LOOP;
  
  D2D_Blend_Line();
  return(D2D_STAGE_SETUP_DONE);
}

void Interrupt_ISR()
{
	/* Test for loop in progress */
	if (D2D_STAGE_IDLE != D2D_Loop_Stage)
	{
		/* decrement loop counter and if != 0 process loop row*/ 
		if(--D2D_Loop->Counter)
		{ 
			/* Update output memory address */
			DMA2D->OMAR += D2D_Loop->OutputPitchBytes;
			/* Add BlenCoeff to BlendIndex */
			D2D_Loop->BlendIndex+=D2D_Loop->BlendCoeff;
			/* Setup FG/BG address and FGalpha for linear blend, start DMA2D */
			D2D_Blend_Line();
		}
		else
		{
			/* else test for current D2D Loop stage */
			if(D2D_STAGE_FIRST_LOOP == D2D_Loop_Stage)
			{
				// workbuffer uses sourcewidth*outputheight*sizeof(uint32_t) total bytes!!!!
				// workbuffer is isolated to dma2d //SCB_InvalidateDCache_by_Addr((uint32_t* const __restrict)xDMA2D::getWorkBuffer_32bit(), D2D_Misc_Param.SourceWidth*D2D_Misc_Param.OutputHeight*sizeof(uint32_t)); 
				
				/* setup DMA2D register */
				// input color mode changes, "workbuffer" is now source on stage two

				xDMA2D::ConfigureLayer_ForBlendMode<xDMA2D::FGND>(DMA2D, LL_DMA2D_INPUT_MODE_ARGB8888, LL_DMA2D_ALPHA_MODE_REPLACE, 0, 0);
				xDMA2D::ConfigureLayer_ForDefaultBlendMode<xDMA2D::BGND>(DMA2D);

				LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_ARGB8888);

				LL_DMA2D_SetNbrOfLines(DMA2D, D2D_Misc_Param.OutputHeight);
				LL_DMA2D_SetNbrOfPixelsPerLines(DMA2D, 1);
				//DMA2D->NLR     = (D2D_Misc_Param.OutputHeight | (1<<16));
				
				LL_DMA2D_SetLineOffset(DMA2D, D2D_Misc_Param.OutputPitch-1);

				uint32_t const SourceWidthOffset(D2D_Misc_Param.SourceWidth-1);
				LL_DMA2D_FGND_SetLineOffset(DMA2D, SourceWidthOffset);
				LL_DMA2D_BGND_SetLineOffset(DMA2D, SourceWidthOffset);
				
				LL_DMA2D_SetOutputMemAddr(DMA2D, (uint32_t)D2D_Misc_Param.OutputBaseAddress);
				
				/* start 2nd loop stage */
				D2D_Loop = &D2D_2nd_Loop;
				D2D_Loop_Stage = D2D_STAGE_2ND_LOOP;
				
				// workbuffer uses sourcewidth*outputheight*sizeof(uint32_t) total bytes!!!!
				// workbuffer is isolated to dma2d //SCB_CleanDCache_by_Addr((uint32_t* const __restrict)xDMA2D::getWorkBuffer_32bit(), D2D_Misc_Param.SourceWidth*D2D_Misc_Param.OutputHeight*sizeof(uint32_t));
					
				/* Setup FG/BG address and FGalpha for linear blend, start DMA2D */
				D2D_Blend_Line();
			}
			else
			{
				// important
				SCB_InvalidateDCache_by_Addr((uint32_t* const __restrict)D2D_Misc_Param.OutputBaseAddress, D2D_Misc_Param.OutputWidth*D2D_Misc_Param.OutputHeight*sizeof(uint32_t)); 
				
				/* reset to idle stage */
				D2D_Loop_Stage = D2D_STAGE_IDLE;
								
				/* else resize complete */
				if ( nullptr != D2D_Misc_Param.UserCallback_Finished ) {
					
					do_chain_op const opChained = D2D_Misc_Param.UserCallback_Finished( D2D_Misc_Param.SourceBaseAddress, D2D_Misc_Param.OutputBaseAddress, D2D_Misc_Param.OutputWidth, D2D_Misc_Param.OutputHeight );
					if ( nullptr != opChained ) {
						
						// Proper order
						opChained( D2D_Misc_Param.SourceBaseAddress, D2D_Misc_Param.OutputBaseAddress, D2D_Misc_Param.OutputWidth, D2D_Misc_Param.OutputHeight );
						// a "chained" resize op is now running, leave interrupts alone and effectively keep the DMA2D_RESIZE_BUSY atomic state
						// resulting in hopefully less overhead and exploiting Cortex M7 interrupt tail-chaining
					}
					else {
						// no chained operations pending, safe to clear atomic state and disable the tracking / TC interrupt
						// disable interrupts on completion //
						DisableIT_TC();
						DMA2D_Private.DMA2D_OpIsBusy = DMA2D_AVAILABLE;
					}
				}
				
			}
		}
	}
	/*else
	{
		// add here other DMA2d TC Irq stuff
		// This is an unknown state / not used
		//LED_SignalError();
		//DisableIT_TC();
		//ignore
	}*/
} 

uint32_t const D2D_Resize_Stage(void)
{
  return (D2D_Loop_Stage);
}

static xDMA2D::do_chain_op const DF_UpSample_128x32_32bit(uint32_t const SourceAddress, uint32_t const OutputAddress, uint32_t const OutputWidth, uint32_t const OutputHeight)
{
	// 128x32 to 256x64 Finished, resulting is sitting in oDMA2DBuffers.EffectRenderBuffer_32bit
	DMA2D_Private.DMA2D_DualFilterState = DMA2D_DUALFILTERBLUR_DMA2D_FINISHED;
	return(nullptr); // all chain operations finished
}
static void DF_Chain_UpSample_128x32_32bit(uint32_t const SourceAddress, uint32_t const OutputAddress, uint32_t const OutputWidth, uint32_t const OutputHeight)
{
	// 128x32 => 256x64 begins output uses the source, source uses last output
	// this sample output is: xDMA2D::getEffectBuffer_32bit()
	// this samole source is: xDMA2D::getDualFilterTarget()
	xDMA2D::UpSample_32bit( (uint32_t* const __restrict)SourceAddress, (uint32_t const* const __restrict)OutputAddress, OutputWidth, OutputHeight, &DF_UpSample_128x32_32bit );
}
/* ########### */
static xDMA2D::do_chain_op const DF_UpSample_64x16_32bit(uint32_t const SourceAddress, uint32_t const OutputAddress, uint32_t const OutputWidth, uint32_t const OutputHeight)
{
	// 64x16 to 128x32 Finished, chain next upsample
	return(&DF_Chain_UpSample_128x32_32bit); // chain operation
}
static void DF_Chain_UpSample_64x16_32bit(uint32_t const SourceAddress, uint32_t const OutputAddress, uint32_t const OutputWidth, uint32_t const OutputHeight)
{
	// 64x16 => 128x32 begins output uses the source, source uses last output
	// this sample output is: xDMA2D::getDualFilterTarget()
	// this samole source is: xDMA2D::getEffectBuffer_32bit()
	xDMA2D::UpSample_32bit( (uint32_t* const __restrict)SourceAddress, (uint32_t const* const __restrict)OutputAddress, OutputWidth, OutputHeight, &DF_UpSample_64x16_32bit );
}
/* ########### */
static xDMA2D::do_chain_op const DF_DownSample_128x32_32bit(uint32_t const SourceAddress, uint32_t const OutputAddress, uint32_t const OutputWidth, uint32_t const OutputHeight)
{
	// 128x32 to 64x16 Finished, chain next upsample
	return(&DF_Chain_UpSample_64x16_32bit); // chain operation
}
static void DF_Chain_DownSample_128x32_32bit(uint32_t const SourceAddress, uint32_t const OutputAddress, uint32_t const OutputWidth, uint32_t const OutputHeight)
{
	// 128x32 => 64x16 begins - 32bit pingpong starts between effectbuffer (256x64 - 32bit) and dualfilter buffer (128x32 - 32bit)
	// this sample output is: xDMA2D::getEffectBuffer_32bit()
	// this samole source is: xDMA2D::getDualFilterTarget()
	xDMA2D::DownSample_32bit( oDMA2DBuffers.EffectRenderBuffer_32bit, (uint32_t const* const __restrict)OutputAddress, OutputWidth, OutputHeight, &DF_DownSample_128x32_32bit );
}
/* ########### */
static xDMA2D::do_chain_op const DF_DownSample_256x64_8bit(uint32_t const SourceAddress, uint32_t const OutputAddress, uint32_t const OutputWidth, uint32_t const OutputHeight)
{
	// 256x64 to 128x32 Finished, chain next downsample
	return(&DF_Chain_DownSample_128x32_32bit); // chain operation
}
bool const DualFilterBlur_BeginAsync(uint8_t const* const __restrict blurSource)
{
	if ( DMA2D_DUALFILTERBLUR_DMA2D_AVAILABLE == DMA2D_Private.DMA2D_DualFilterState ) {
		// 256x64 => 128x32 begins
		// this sample output is: xDMA2D::getDualFilterTarget()
		// this samole source is: blurSource
		DMA2D_Private.DMA2D_DualFilterState = DMA2D_DUALFILTERBLUR_DMA2D_STARTED;
		xDMA2D::DownSample_8bit( oDMA2DBuffers.DualFilterTarget, blurSource, OLED::SCREEN_WIDTH, OLED::SCREEN_HEIGHT, &DF_DownSample_256x64_8bit );
			
		return(true);
	}
	
	return(false);
}
/* ########### */
void DualFilterBlur_EndAsync()
{
	if ( DMA2D_DUALFILTERBLUR_DMA2D_STARTED == DMA2D_Private.DMA2D_DualFilterState ) {
		// if started (currently busy) wait until finished
		while ( DMA2D_Private.DMA2D_DualFilterState > 0 ) // -1 == DMA2D_DUALFILTERBLUR_DMA2D_FINISHED
		{}
	}
	
	// only if finished (if entered function as available, ignore EndAsync request)
	if ( DMA2D_Private.DMA2D_DualFilterState < 0 ) {
		
		//Convert from ARGB8888 to A8 for blending op
		asmConv_L8L8L8L8_To_L8(oDMA2DBuffers.WorkBufferDMA2D_8bit, oDMA2DBuffers.EffectRenderBuffer_32bit, OLED::SCREEN_HEIGHT*OLED::SCREEN_WIDTH);
		
		// Blend result of dual filter blur with OLED 32bit frontbuffer
		LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_M2M_BLEND);
		
		ConfigureLayer_ForDefaultBlendMode<BGND>(DMA2D);
		ConfigureLayer_ForBlendMode<FGND>(DMA2D, LL_DMA2D_INPUT_MODE_A8, LL_DMA2D_ALPHA_MODE_COMBINE, 0xFF, 0xCF);
		//ConfigureLayer_ForBlendMode<FGND>(DMA2D, LL_DMA2D_INPUT_MODE_ARGB8888, LL_DMA2D_ALPHA_MODE_NO_MODIF, 0xFF, 0x9F);
		DMA2D_SetMemory_FGND((uint32_t)oDMA2DBuffers.WorkBufferDMA2D_8bit, 0);
		//DMA2D_SetMemory_FGND((uint32_t)oDMA2DBuffers.EffectRenderBuffer_32bit, 0);
		
		DMA2D_SetMemory_TargetIsBGND((uint32_t)DTCM::getFrontBuffer(), 0, OLED::SCREEN_HEIGHT, OLED::SCREEN_WIDTH);
		
		xDMA2D::Start_DMA2D();
		
		// reset state of dual filter blur
		DMA2D_Private.DMA2D_DualFilterState = DMA2D_DUALFILTERBLUR_DMA2D_AVAILABLE;
	}
	
	// state here is always DMA2D_DUALFILTERBLUR_DMA2D_AVAILABLE
}




} // end namespace



