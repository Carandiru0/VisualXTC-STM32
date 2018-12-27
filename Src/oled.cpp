/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#include "oled.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_utils.h"
#include "stm32f7xx_ll_dma.h"
#include "stm32f7xx_ll_dma2d.h"
#include "dma.h"
#include "spi.h"
#include "Commonmath.h"
#include "rng.h"
#include "Point2D.h"
#include <cctype>
#include "DTCM_Reserve.h"
#include "world.h"

//#include "PixelCowboy.h"
#include "LadyRadical.h"
#include "Gothica.h"

#include "stm32f7xx_ll_rcc.h"
#include "ext_sram.h"
#include "debug.cpp"

#if( 0 != USART_ENABLE )
#include "usart.h"
#endif

#include "L8_CLUT.h" // CLUT IS required to load

extern "C" void asmDitherBlit(void);

using namespace OLED;

// chipselected if low
STATIC_INLINE void CS_L() { LL_GPIO_ResetOutputPin(GPIO_OLED_CS_GPIO_Port, GPIO_OLED_CS_Pin); }
STATIC_INLINE void CS_H() { WaitPendingSPI1ChipDeselect(); LL_GPIO_SetOutputPin(GPIO_OLED_CS_GPIO_Port, GPIO_OLED_CS_Pin); }

//Data if high, command if low
STATIC_INLINE void DC_L() { WaitPendingSPI1ChipDeselect(); LL_GPIO_ResetOutputPin(GPIO_OLED_DC_GPIO_Port, GPIO_OLED_DC_Pin); }
STATIC_INLINE void DC_H() { WaitPendingSPI1ChipDeselect(); LL_GPIO_SetOutputPin(GPIO_OLED_DC_GPIO_Port, GPIO_OLED_DC_Pin); }

STATIC_INLINE void RESET_L() { LL_GPIO_ResetOutputPin(GPIO_OLED_RESET_GPIO_Port, GPIO_OLED_RESET_Pin); }
STATIC_INLINE void RESET_H() { LL_GPIO_SetOutputPin(GPIO_OLED_RESET_GPIO_Port, GPIO_OLED_RESET_Pin); }

namespace DTCM
{
static constexpr uint32_t const HISTO_SIZE = 256,
																HISTO_TotalNumPixels = OLED::SCREEN_WIDTH * OLED::SCREEN_HEIGHT,
																HISTO_TotalNumPixels_SATBITS = 14; 		// 2 pow 14 = 16385 = 256x64
/* ############## */
// 8bpp (L8 src buffer for DMA2D (background),      						// 16KB
uint8_t 	BackBuffer[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH]    // used a lot, eg Lerp Rendering
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".dtcm.BackBuffer")));
// 8bpp*4	(ARGB8888 src buffer for DMA2D (foreground),    			// 64KB
uint32_t	FrontBuffer[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH] 	// used a lot, eg.) compositing alpha+luma buffers
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".dtcm.FrontBuffer")));

// 8bpp	(8bit signed src buffer for zbuffer,    								// 16KB
int8_t	DepthBuffer[OLED::SCREEN_WIDTH][OLED::SCREEN_HEIGHT] 	  // used a lot, eg.) compositing alpha+luma buffers
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".dtcm.DepthBuffer")));	
	
} //end namespace;

// A8R8G8B8 (Target output of DMA2D // 64KB
uint32_t _DMA2DFrameBuffer[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH]
	__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".dma2dscratchbuffer"))); // bss places it a lower priority (higher address) which
																											 // saves DTCM Memory for data that is defined in a section w/o .bss
extern "C" uint32_t globalasm_DMA2DFrameBuffer[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH]
	__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((alias("_DMA2DFrameBuffer"))); // for assembler code

uint8_t * __restrict _DitherTargetFrameBuffer  // 8bpp -> 4bpp, targets either _FrameBuffer 0/1
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".dtcm_atomic.dithertarget")));

extern "C" uint8_t * __restrict globalasm_DitherTargetFrameBuffer  // 8bpp -> 4bpp, targets either _FrameBuffer 0/1
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((alias("_DitherTargetFrameBuffer")));

struct oOLED
{
	// FRAMEBUFFER_WIDTH = 480, START_XOFFSET = 112
	static constexpr uint32_t const FrameBufferWidth = 480>>1, Width = OLED::SCREEN_WIDTH, Height = OLED::SCREEN_HEIGHT, FrameBufferLength = (((FrameBufferWidth) * Height)), 
																	StartXOffset = 112>>1,
																	Width_SATBITS = Constants::SATBIT_256, Height_SATBITS = Constants::SATBIT_64,
																	Sync_Interval = 1200;
		
	// DTCM size is 128KB
	// SRAM1 size is 368KB
	// SRAM2 size is 16KB, total SRAM = 372KB
	// External SRAM size is 512KB
	
	struct DoubleBufferBloomHDR {
		static uint8_t _FrameBuffer0[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH]	// 8bpp HDR Bloom DoubleBuffer 0 ,   16 KB
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".bss.hdrbloomframebuffer0"))),
									 _FrameBuffer1[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH] // 8bpp HDR Bloom DoubleBuffer 1 ,   16 KB
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".bss.hdrbloomframebuffer1")));
	};
	struct DoubleBuffer {
		static uint8_t _FrameBuffer0[FrameBufferLength]	// 4bpp SPI DoubleBuffer 0 ,   15.36 KB
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".framebuffer0"))),
									 _FrameBuffer1[FrameBufferLength] // 4bpp SPI DoubleBuffer 1 ,   15.36 KB
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".framebuffer1")));
		
		static atomic_strict_uint32_t bOLEDBusy_DMA2[2]					// ****Private Acess inside OLED Only, and below is public access for main.cpp interrupt***
		__attribute__((section (".dtcm_atomic")));					// important that this is .dtcm_atomic	
	};													
	
	static FontType const* CurFont;
	static uint32_t LastSync;
};

namespace OLED
{
	namespace Effects
	{
		
	static struct sEffects
	{
		Effects::ScanlinerIntercept* pCurInterceptDesc;
		
		sEffects()
		: pCurInterceptDesc(nullptr)
		{}
			
	} oEffects;	
		
	} // endnamespace
}// endnamespace

// bss places it a lower priority (higher address) which
// saves DTCM Memory for data that is defined in a section w/o .bss
uint8_t oOLED::DoubleBufferBloomHDR::_FrameBuffer0[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH]	// 8bpp HDR Bloom DoubleBuffer 0 ,   16 KB
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
__attribute__((section (".bss.hdrbloomframebuffer0"))),
				oOLED::DoubleBufferBloomHDR::_FrameBuffer1[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH] // 8bpp HDR Bloom DoubleBuffer 1 ,   16 KB
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
__attribute__((section (".bss.hdrbloomframebuffer1")));

uint8_t * __restrict BloomHDRTargetFrameBuffer  										  // 8bpp targets either _FrameBuffer 0/1
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
__attribute__((section (".dtcm_atomic.bloomhdrtarget"))) ((uint8_t * __restrict)oOLED::DoubleBufferBloomHDR::_FrameBuffer1);
static uint8_t const * __restrict BloomHDRLastFrameBuffer  									// 8bpp targets either _FrameBuffer 0/1
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
__attribute__((section (".dtcm_atomic.lastbloomhdr"))) (nullptr);

uint8_t oOLED::DoubleBuffer::_FrameBuffer0[oOLED::FrameBufferLength]
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".framebuffer0")));
uint8_t oOLED::DoubleBuffer::_FrameBuffer1[oOLED::FrameBufferLength]
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".framebuffer1")));

atomic_strict_uint32_t oOLED::DoubleBuffer::bOLEDBusy_DMA2[2]							// ****Private Acess inside OLED Only, and below is public access for main.cpp interrupt***
	__attribute__((section (".dtcm_atomic"))){false, false};				// important that this is .dtcm_atomic
																																	// and (below) is dtcm_const - makes the .dtcm_const section RO(read-Only)

// forward declarations
STATIC_INLINE void ToggleFrameBuffers();
FontType const* oOLED::CurFont(&oFont_Gothica);
uint32_t oOLED::LastSync(0);
	
namespace OLED
{
atomic_strict_uint32_t* const __restrict bbOLEDBusy_DMA2[2] 							
	__attribute__((section (".dtcm_const"))) = { &oOLED::DoubleBuffer::bOLEDBusy_DMA2[0], &oOLED::DoubleBuffer::bOLEDBusy_DMA2[1] };
} // end namespace

namespace OLED
{
	
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint32_t const * const __restrict getScreenFrameBuffer() {
	return( (uint32_t const* const __restrict)_DMA2DFrameBuffer );
}
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint8_t * const __restrict getDeferredFrameBuffer() {
	return( xDMA2D::getEffectBuffer_8bit() );
}

__attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint8_t * const __restrict getEffectFrameBuffer_0() {
	return( xDMA2D::getEffectBuffer_8bit() );
}

void setActiveFont( FontType const* const oFontType )
{
	oOLED::CurFont = oFontType;
}

} // end namespace

STATIC_INLINE void sendC( uint8_t const cmd )
{
	DC_L();
	SPI1_TransmitByte(cmd);
	DC_H();
}
STATIC_INLINE void sendC_D( uint8_t const cmd, uint8_t const data )
{
	DC_L();
	SPI1_TransmitByte(cmd);
	DC_H();
	SPI1_TransmitByte(data);
}
STATIC_INLINE void sendC_D_D( uint8_t const cmd, uint8_t const data0, uint8_t const data1 )
{
	DC_L();
	SPI1_TransmitByte(cmd);
	DC_H();
	SPI1_TransmitHalfWord(data0, data1);
}

static void ClearDisplay();

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Gray Scale Table Setting (Full Screen)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//#define SHOW_OLEDMAXCHARGE
#ifdef SHOW_OLEDMAXCHARGE
static int32_t _MaxChargeLevel(0); // Should be 100, as in 100% lit pixel , greyscale range is packed into 0 => 100 range.
#endif

static void Set_Gray_Scale_Table()
{
	static constexpr float const fNumLevels = 16.0f,
															 fMinCharge = 0.81f,
															 fMaxCharge = 180.0f + fMinCharge, 
														   fInverseMaxCharge = 1.0f / fMaxCharge,
															 fInverseMaxSRGB = 1.0f / 255.0f,
															 fInverseNumLevels = 1.0f / fNumLevels;
	static constexpr float const linearMultiple = (fMaxCharge - fMinCharge) / fNumLevels,
															 linearBlendSRGBFactor = 0.45f;

	sendC(0xB8);			// Set Gray Scale Table
	
	// Rasnge allowable 0 -> 180, grayscale 0 is duplicated on level1
	float curLevel = 0;
	for ( uint32_t iDx = 0 ; iDx < 16 ; ++iDx )
	{
		float const fCurLevelNormalized = curLevel / (linearMultiple*fNumLevels);
		
		float const fCurLevelSRGB = FLOAT_to_SRGB(fCurLevelNormalized);
		
		int32_t iSRGBChargeLevel;
		if ( fCurLevelNormalized < linearBlendSRGBFactor )
		{
				iSRGBChargeLevel = ( lerp(fMinCharge, fMaxCharge, fCurLevelSRGB * fInverseMaxSRGB) + smoothstep(0.0f, 8.0f, ((float)iDx)/16.0f) );
		}
		else
		{
				iSRGBChargeLevel = min( ( mix( lerp(fMinCharge, fMaxCharge, fCurLevelSRGB * fInverseMaxSRGB), curLevel, 
																					  __fmaxf(0.0f, (((float)iDx)*fInverseNumLevels) - linearBlendSRGBFactor)) ), curLevel)
																						+ smoothstep(0.0f, 18.0f, ((float)iDx)/16.0f);
		}

		if ( 0 != iDx )
		{
			iSRGBChargeLevel = __USAT(iSRGBChargeLevel, Constants::SATBIT_256);
#ifdef SHOW_OLEDMAXCHARGE		
			_MaxChargeLevel = max(_MaxChargeLevel, iSRGBChargeLevel);
#endif			
			SPI1_TransmitByte( iSRGBChargeLevel );
		}
		curLevel += linearMultiple;
		
	}
	sendC(0x00);			// Enable Gray Scale Table
}

static void Configure_DMA2D_MainStatic(void) // mostly for preloading CLUT
{
	LL_DMA2D_InitTypeDef dma2dConfig;
	LL_DMA2D_LayerCfgTypeDef foregroundLayer,
													 backgroundLayer;
	
	LL_DMA2D_StructInit(&dma2dConfig);
	
	/* Configure DMA2D output color mode */
	dma2dConfig.Mode = LL_DMA2D_MODE_M2M_BLEND;
	dma2dConfig.ColorMode = LL_DMA2D_OUTPUT_MODE_ARGB8888;
	dma2dConfig.NbrOfLines = oOLED::Height;
	dma2dConfig.NbrOfPixelsPerLines = oOLED::Width;
	dma2dConfig.OutputMemoryAddress = (uint32_t)_DMA2DFrameBuffer;

	LL_DMA2D_Init(DMA2D, &dma2dConfig);
	//LL_DMA2D_EnableIT_TC(DMA2D);
	
	LL_DMA2D_LayerCfgStructInit(&backgroundLayer);
	LL_DMA2D_LayerCfgStructInit(&foregroundLayer);
	
	// Common Layer Config //
	backgroundLayer.LineOffset = foregroundLayer.LineOffset = 0;
	backgroundLayer.RBSwapMode = foregroundLayer.RBSwapMode = LL_DMA2D_RB_MODE_REGULAR;
	backgroundLayer.CLUTColorMode = foregroundLayer.CLUTColorMode = LL_DMA2D_CLUT_COLOR_MODE_ARGB8888;
	backgroundLayer.CLUTSize = foregroundLayer.CLUTSize = 0xFF;
	backgroundLayer.CLUTMemoryAddress = foregroundLayer.CLUTMemoryAddress = (uint32_t)&L8_CLUT;
	
	// background config //
	backgroundLayer.ColorMode = LL_DMA2D_INPUT_MODE_L8;
	backgroundLayer.AlphaMode = LL_DMA2D_ALPHA_MODE_NO_MODIF;
	backgroundLayer.Red = backgroundLayer.Green = backgroundLayer.Blue = 0x00;
	backgroundLayer.Alpha = 0XFF;
	backgroundLayer.AlphaInversionMode = LL_DMA2D_ALPHA_INVERTED;
	backgroundLayer.MemoryAddress = (uint32_t)DTCM::getBackBuffer();
	
	// foreground config //
	foregroundLayer.ColorMode = LL_DMA2D_INPUT_MODE_ARGB8888;
	foregroundLayer.AlphaMode = LL_DMA2D_ALPHA_MODE_NO_MODIF;
	foregroundLayer.Red = foregroundLayer.Green = foregroundLayer.Blue = 0xff;
	foregroundLayer.Alpha = 0Xff;
	foregroundLayer.AlphaInversionMode = LL_DMA2D_ALPHA_REGULAR;
	foregroundLayer.MemoryAddress = (uint32_t)DTCM::getFrontBuffer();
	
	LL_DMA2D_ConfigLayer(DMA2D, &backgroundLayer, 0);
	LL_DMA2D_ConfigLayer(DMA2D, &foregroundLayer, 1);
	
	LL_DMA2D_BGND_EnableCLUTLoad(DMA2D);
	LL_DMA2D_FGND_EnableCLUTLoad(DMA2D);

	__WFI(); // beware: puts mcu in lowe power-mode, used here to delay a small amount to alow automatic CLUT loading
}
static void Configure_DMA2D_Main(void)
{
	LL_DMA2D_LayerCfgTypeDef foregroundLayer,
													 backgroundLayer;
		
	// Wait foor previous operation to finish
	//Wait_DMA2D();
	
	/* Configure DMA2D output color mode */
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_M2M_BLEND);
	LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_ARGB8888);
		
	//TM_INT_DMA2DGRAPHIC_SetMemory(DIS.PixelSize * (DIS.Width - y - height + DIS.Width * x), DIS.Width - height, width, height);
	LL_DMA2D_ConfigSize(DMA2D, oOLED::Height, oOLED::Width);
	LL_DMA2D_SetLineOffset(DMA2D, 0);
	LL_DMA2D_SetOutputMemAddr(DMA2D, (uint32_t)(_DMA2DFrameBuffer));
	
	LL_DMA2D_LayerCfgStructInit(&backgroundLayer);
	LL_DMA2D_LayerCfgStructInit(&foregroundLayer);
			
	// background config //
	backgroundLayer.ColorMode = LL_DMA2D_INPUT_MODE_L8;
	backgroundLayer.AlphaMode = LL_DMA2D_ALPHA_MODE_NO_MODIF;
	backgroundLayer.Red = backgroundLayer.Green = backgroundLayer.Blue = 0x00;
	backgroundLayer.Alpha = 0XFF;
	backgroundLayer.AlphaInversionMode = LL_DMA2D_ALPHA_INVERTED;
	backgroundLayer.MemoryAddress = (uint32_t)DTCM::getBackBuffer();
		
	// foreground config //
	foregroundLayer.ColorMode = LL_DMA2D_INPUT_MODE_ARGB8888;
	foregroundLayer.AlphaMode = LL_DMA2D_ALPHA_MODE_NO_MODIF;
	foregroundLayer.Red = foregroundLayer.Green = foregroundLayer.Blue = 0xff;
	foregroundLayer.Alpha = 0Xff;
	foregroundLayer.AlphaInversionMode = LL_DMA2D_ALPHA_REGULAR;
	foregroundLayer.MemoryAddress = (uint32_t)DTCM::getFrontBuffer();
	
	LL_DMA2D_ConfigLayer(DMA2D, &backgroundLayer, 0);
	LL_DMA2D_ConfigLayer(DMA2D, &foregroundLayer, 1);
	
	
	// Update background layer "window"
		
	// image[y][x] // assuming this is the original orientation 
	// image[x][original_width - y] // rotated 90 degrees ccw
	// image[original_height - x][y] // 90 degrees cw 
	// image[original_height - y][original_width - x] // 180 degrees
	// p_myImage2[offset].pixel = p_myImage[width * (height - 1 - y) + x].pixel; rotated 90 clockwise
	
	// Select Glyph
	//	DMA2D_SetGlyphAddress((uint32_t const)(oOLED::CurFont->FontMap + ((yGlyph * oOLED::CurFont->FONT_WIDTH) + xGlyph)),
	//												oOLED::CurFont->FONT_WIDTH - oOLED::CurFont->FONT_GLYPHWIDTH);
	
	//  DMA2D_SetMemory_Font((uint32_t const)( _DMA2DFrameBuffer + ((y * oOLED::Width) + x)), oOLED::Width - oOLED::CurFont->FONT_GLYPHWIDTH, 
	//										  	oOLED::CurFont->FONT_GLYPHHEIGHT, oOLED::CurFont->FONT_GLYPHWIDTH);
	
	//xDMA2D::DMA2D_SetMemory_BGND((uint32_t)&DTCM::getBackBuffer()[0][oOLED::Width-1], 0);
	//xDMA2D::DMA2D_SetMemory_BGND((uint32_t)DTCM::getFrontBuffer(),oOLED::Height);
	
	// ****** cache coherency not required for DTCM TCM Memories DTCM::getFrontBuffer(), DTCM::getBackBuffer()
}
static void Configure_DMA2D_Scratch(uint8_t const* __restrict p8bitLayerBack, uint8_t const* __restrict p8bitLayerFront, uint32_t const ForegroundAlpha = Constants::OPACITY_100, uint32_t const ShadeRegister = 0xFF)
{
	LL_DMA2D_LayerCfgTypeDef foregroundLayer,
													 backgroundLayer;
		
	// Wait foor previous operation to finish
	xDMA2D::Wait_DMA2D();
	
	/* Configure DMA2D output color mode */
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_M2M_BLEND);
	LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_ARGB8888);
	
	LL_DMA2D_ConfigSize(DMA2D, oOLED::Height, oOLED::Width);
	LL_DMA2D_SetLineOffset(DMA2D, 0);
	LL_DMA2D_SetOutputMemAddr(DMA2D, (uint32_t)_DMA2DFrameBuffer);
	
	LL_DMA2D_LayerCfgStructInit(&backgroundLayer);
	LL_DMA2D_LayerCfgStructInit(&foregroundLayer);
	
		
	// background config //
	backgroundLayer.ColorMode = LL_DMA2D_INPUT_MODE_L8;
	backgroundLayer.AlphaMode = LL_DMA2D_ALPHA_MODE_NO_MODIF;
	backgroundLayer.Red = backgroundLayer.Green = backgroundLayer.Blue = 0x00;
	backgroundLayer.Alpha = 0XFF;
	backgroundLayer.AlphaInversionMode = LL_DMA2D_ALPHA_INVERTED;
	backgroundLayer.MemoryAddress = (uint32_t)p8bitLayerBack;
	
	// foreground config //
	foregroundLayer.ColorMode = LL_DMA2D_INPUT_MODE_A8;
	foregroundLayer.AlphaMode = LL_DMA2D_ALPHA_MODE_COMBINE;
	foregroundLayer.Red = foregroundLayer.Green = foregroundLayer.Blue = ShadeRegister;
	foregroundLayer.Alpha = ForegroundAlpha;
	foregroundLayer.AlphaInversionMode = LL_DMA2D_ALPHA_REGULAR;
	foregroundLayer.MemoryAddress = (uint32_t)p8bitLayerFront;

	LL_DMA2D_ConfigLayer(DMA2D, &backgroundLayer, 0);
	LL_DMA2D_ConfigLayer(DMA2D, &foregroundLayer, 1);
	
	// Required b4 dma, ensure data is commited to memory from cache
	SCB_CleanDCache_by_Addr((uint32_t*)p8bitLayerFront, oOLED::Width*oOLED::Height);
	SCB_CleanDCache_by_Addr((uint32_t*)p8bitLayerBack, oOLED::Width*oOLED::Height);

}

namespace OLED
{
	
void FillBackBuffer(uint8_t const Luma)
{
	xDMA2D::FillBuffer_8bit<true>((uint8_t* const __restrict)DTCM::getBackBuffer(), Luma);
}
void FillFrontBuffer(uint8_t const Alpha, uint8_t const Luma)
{
	xDMA2D::FillBuffer_32bit<true>((uint32_t* const __restrict)DTCM::getFrontBuffer(), xDMA2D::AlphaLuma(Alpha, Luma));
}

} // END NAMESPACE

void StartUp_Output_Sys()
{
	static constexpr char const* const ON = "On",
														 * const OFF = "Off";
	
	uint32_t NbBytes(0);
	float tWrite(0), tRead(0);
	
	LL_RCC_ClocksTypeDef sysClocks;
												 
	uint32_t const quadspi_cr = QUADSPI->CR;
	
	LL_RCC_GetSystemClocksFreq(&sysClocks);

#ifdef SHOW_OLEDMAXCHARGE	
	DebugMessage("OLED MaxCharge: %d\n", 
								_MaxChargeLevel );

#else
	DebugMessage("Speed: %luMHx  QSPI: %3luMHx\n", 
								sysClocks.SYSCLK_Frequency/1000000,
								(sysClocks.SYSCLK_Frequency/1000000)/(((quadspi_cr & 0xFF000000)>>24)+1) );
#endif
	uint32_t const flash_acr = FLASH->ACR;
  uint32_t const scb_ccr = SCB->CCR;
	
	DebugMessage("ART %s , ART-PF %s , D-cache %s , I-cache %s\n", 
								((flash_acr & 0x200)? ON:OFF),
							  ((flash_acr & 0x100)? ON:OFF),
								((scb_ccr & 0x10000)? ON:OFF),
	        			((scb_ccr & 0x20000)? ON:OFF) );
								
#ifdef DO_RAMTEST_AT_STARTUP
{														 
	
	uint32_t const uiSRAMTestResult = ExtSRAM::RamTest(NbBytes, tWrite, tRead);
		
	if ( uiSRAMTestResult == NbBytes ) {
		//DebugMessage("SRAM OK\n");
		DebugMessage("SRAM W: %d MB/s  R: %d MB/s\n", uint32::__roundf(((float)NbBytes / tWrite) / 1e6f), // dividing by 1e6f
																									uint32::__roundf(((float)NbBytes / tRead) / 1e6f)); // to get MB/s
	}																																																		// instead of bytes/s
	else {
		DebugMessage("SRAM fail: %d / %d, diff:%d\n", uiSRAMTestResult, NbBytes, NbBytes - uiSRAMTestResult);
	}
	
}
#endif
}

//--------------------------------------------------------------------------
STATIC_INLINE void Set_Column_Address(uint8_t const a, uint8_t const b)
{
  sendC_D_D(0x15,a,b); // Set Column Address
	
	// a);    //   Default => 0x00
  // b);    //   Default => 0x77
}

//--------------------------------------------------------------------------
STATIC_INLINE void Set_Row_Address(uint8_t const a, uint8_t const b)
{
  sendC_D_D(0x75,a,b); // Set Row Address
	
	// a);    //   Default => 0x00
  // b);    //   Default => 0x7F
}
STATIC_INLINE void WriteEnable()
{
  /// Enable MCU to Write into RAM
	sendC(0x5c);
}
static void SyncAddress(uint32_t const tNow) // Required, or "creeping x offset occurs"
{
	Set_Column_Address(0x00,0x77);
	Set_Row_Address(0x00,0x7f);
	
  WriteEnable();
	oOLED::LastSync = tNow;
}
namespace OLED
{
void DisplayOn()  // Set Sleep mode OFF (Display ON)
{
	CS_L();
	sendC(0xaf);		                	/* display on */
	CS_H();
}
void DisplayOff() // // Set Sleep mode On (Display OFF)
{
	CS_L();
	sendC(0xae);		                	/* display off */
	CS_H();
}
NOINLINE void Init()
{		
	// DCLK = FOsc / D
	// FFrm = FOsc / (D * K * #ofMux)
	// D is clock divide ratio (1 => 1024)
	// K is # of display clocks per row.
	//	K = Phase 1 period + Phase 2 Period + X
	//	X = DCLKs in current drive period.  
	//			Default X = constant + GS15 = 10 + 112
	//			Default K = 9 + 7 + 122
	// #ofMux is set as multiplex ratio (is 64MUX for this display)
	// FOsc is set by command 0xB3, higher the registere setiing results in higher frequency
	
	// OLED Setup:
	// X = 10 + 100 = 110
	// K = 9 + 15 + 110 = 134
	// D * K * 64 = 8576
	// FOsc = 1.94Mhz
	// FFrm = 1.94Mhz / 8576
	// ~226 Hz nominal, FOsc can vary to a minimum of 1.75Mhz, giving a minimum FFrm of 204Hz
	// want OLED Frame frequency to be at least 2 * desired frame rate (60Hz)
	
	CS_L();
	RESET_L();
	
	LL_mDelay(1);
	
	RESET_H();
	CS_H();
	
	LL_mDelay(300);
	
	CS_L();
	
	// Lots of reset/tweaking commands follow
	
  // Set Command Lock (MCU protection status)
  // = Reset
  sendC_D(0xfd, 0x12);
	
	sendC(0xA4); // Set Display Mode = OFF
	
  // Set Front Clock Divider / Oscillator Frequency
  // = reset / 1100b 
	sendC_D(0xb3, 0xb0);

  // Set MUX Ratio
  // = 63d = 64MUX
	sendC_D(0xca, 0x3f);

  // Set Display Offset (y scan offset)
  // = RESET
	sendC_D(0xa2, 0x00);
	
  // Set Display Start Line (y scan offset)
  // = register 00h
	sendC_D(0xa1, 0x00);
	
  // Set Re-map and Dual COM Line mode
  // = Reset except Enable Nibble Re-map, Scan from COM[N-1] to COM0, where N is the Multiplex ratio
  // = Reset except Enable Dual COM mode (MUX = 63)
	sendC_D_D(0xa0, 0x04, 0x11),			// Set Re-Map / Dual COM Line Mode
	
  // Set GPIO
  // = GPIO0, GPIO1 = HiZ, Input Disabled
	sendC_D(0xb5, 0x00);
	
  // Function Selection
  // = reset = Enable internal VDD regulator
	sendC_D(0xab, 0x01);
	
  // Display Enhancement A
  // = Enable external VSL
  // = Normal (reset) b5 or fd
	sendC_D_D(0xb4, 0xa2, 0xfd);
	
  // Set Contrast Current
  // = reset
	sendC_D(0xc1, 0xff);
	
  // Master Contrast Current Control
  // = no change
	sendC_D(0xc7, 0x0f);
	
  // Select Default Linear Gray Scale table
	//sendC(0xb9);
	Set_Gray_Scale_Table();
	
  // Set Phase Length
  // = Phase 1 period (reset phase length) = 9 DCLKs, Phase 2 period (first pre-charge phase length) = 15 DCLKs
	sendC_D(0xb1, 0xf4);

  // Display Enhancement B
  // = Normal (reset)
  // Reserved Reccomended
	//sendC_D_D(0xd1, 0x82, 0x20);  // CAUSES WEIRD BANDING AND NOISE COMPARED TO WITHOUT, DARKER
	
  // Set Pre-charge voltage
  // = 0.60 x VCC
	sendC_D(0xbb, 0x1f);
	
  // Set Second Precharge Period
  // = 8 dclks [reset]
	sendC_D(0xb6, 0x08);
	
  // Set VCOMH Deselect Level
  // = 0.80 x VCC (Default)
	sendC_D(0xbe, 0x04);
	
  // Set Display Mode = Normal Display
	//sendC(0xa6);
	sendC(0xa7);	//inverted
  // Exit Partial Display
	sendC(0xa9);
	
	/*
	sendC_D(0xfd, 0x12),            	// unlock 
  sendC(0xae),		                	// display off
  sendC_D(0xb3, 0x91),							// set display clock divide ratio/oscillator frequency (set clock as 80 frames/sec) 
  sendC_D(0xca, 0x3f),							// multiplex ratio 1/64 Duty (0x0F~0x3F) 
  sendC_D(0xa2, 0x00),							// display offset, shift mapping ram counter 
  sendC_D(0xa1, 0x00),							// display start line
  //U8X8_CAA(0xa0, 0x14, 0x11),			// Set Re-Map / Dual COM Line Mode
  sendC_D_D(0xa0, 0x06, 0x011),			// Set Re-Map / Dual COM Line Mode
  sendC_D(0xab, 0x01),							// Enable Internal VDD Regulator
  sendC_D_D(0xb4, 0xa0, 0x005|0x0fd),	// Display Enhancement A
  sendC_D(0xc1, 0x9f),								// contrast
  sendC_D(0xc7, 0x0f),								// Set Scale Factor of Segment Output Current Control
  sendC(0xb9),		                		// linear grayscale
  sendC_D(0xb1, 0xe2),								// Phase 1 (Reset) & Phase 2 (Pre-Charge) Period Adjustment
  sendC_D_D(0xd1, 0x082|0x020, 0x020),	// Display Enhancement B
  sendC_D(0xbb, 0x1f),									// precharge  voltage
  sendC_D(0xb6, 0x08),									// precharge  period
  sendC_D(0xbe, 0x07),									// vcomh
  sendC(0xa6),		                				// normal display
	//sendC(0xa5),		                				// white display
	//sendC(0xa4),		                				// black display
  sendC(0xa9);		                				// exit partial display
	*/
	CS_H();
	
	LL_mDelay(1);
	CS_L();
	WriteEnable();
	ClearDisplay();
	CS_H();
	
	DisplayOn();

	LL_mDelay(300);
		
	WaitForSPI1BufferEmpty();
	
	/* Enable the Double buffer mode */
	LL_DMA_EnableDoubleBufferMode(DMA2, LL_DMA_STREAM_3);
	LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_3, oOLED::FrameBufferLength);
	LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_3, (uint32_t)oOLED::oOLED::DoubleBuffer::_FrameBuffer0);
	LL_DMA_SetMemory1Address(DMA2, LL_DMA_STREAM_3, (uint32_t)oOLED::oOLED::DoubleBuffer::_FrameBuffer1);
	LL_DMA_SetCurrentTargetMem(DMA2, LL_DMA_STREAM_3, LL_DMA_CURRENTTARGETMEM0);
	
	CS_L();
	SyncAddress(millis());		// important - fixes ofset creeping during runtime realtime render loop with autoaddress increment!
	CS_H();
	WaitPendingSPI1ChipDeselect();
	
	// Keep CS Low //
	CS_L();
	WaitPendingSPI1ChipDeselect();
	ToggleFrameBuffers();

	// Initialize DMA2D
	xDMA2D::Init(&Configure_DMA2D_MainStatic);
	ClearBuffer_8bit(oOLED::DoubleBuffer::_FrameBuffer0);
	ClearBuffer_8bit(oOLED::DoubleBuffer::_FrameBuffer1);
	ClearBuffer_8bit((uint8_t* const __restrict)oOLED::DoubleBufferBloomHDR::_FrameBuffer0);
	ClearBuffer_8bit((uint8_t* const __restrict)oOLED::DoubleBufferBloomHDR::_FrameBuffer1);
	
#ifndef PROGRAM_SDF_TO_FRAM
	StartUp_Output_Sys();
#endif
}

} //end namespace

STATIC_INLINE void ToggleFrameBuffers()
{
#if defined(_DEBUG_OUT_OLED) && defined(MEASURE_FRAMEBUFFER_UTILIZATION)
	static uint32_t countUsingTarget_0(0),
									countUsingTarget_1(0);
#endif	
	switch( LL_DMA_GetCurrentTargetMem(DMA2, LL_DMA_STREAM_3) )
	{
		case LL_DMA_CURRENTTARGETMEM1:	// Switching from Target 1 to Target 0

			while ( 0 != oOLED::DoubleBuffer::bOLEDBusy_DMA2[CURRENTTARGETMEM0] ) {	// Wsit until Target 0 is complete if busy sending to OLED
				
				__WFI(); // beware: puts mcu in lowe power-mode, this the only place this is acceptable (ToggleBuffers())
				
				if ( 0 == oOLED::DoubleBuffer::bOLEDBusy_DMA2[CURRENTTARGETMEM0] )
					break;
				else if ( 0 == oOLED::DoubleBuffer::bOLEDBusy_DMA2[CURRENTTARGETMEM1] )	// If Target 0 is busy, and Target 1 is not, branch to reuse current target
					goto TargetMem_0;																// bugfix: prevents strange lockup
				
			}
TargetMem_1:
			oOLED::DoubleBuffer::bOLEDBusy_DMA2[CURRENTTARGETMEM0] = 1;	// Using Target 0 for sending to OLED
	
		// Move current UserFrameBuffer(0) to active, flush cache of pending active framebuffer(0) prior to DMA transfer (ensure comitted to memory)
			// required as memory is in SRAM, which is WBWA
			//SCB_CleanDCache_by_Addr((uint32_t*)oOLED::DoubleBuffer::_FrameBuffer0, oOLED::FrameBufferLength); // ### working without ? is it config'd WT?
		
		// Set new target 0 and send to OLED
		LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_3);
			LL_DMA_SetCurrentTargetMem(DMA2, LL_DMA_STREAM_3, LL_DMA_CURRENTTARGETMEM0);
		
			// Using new free framebuffer, as framebuffer 0 is currently sending to OLED
			_DitherTargetFrameBuffer = oOLED::DoubleBuffer::_FrameBuffer1;
#if defined(_DEBUG_OUT_OLED) && defined(MEASURE_FRAMEBUFFER_UTILIZATION)
			++countUsingTarget_1;
#endif
			break;
		case LL_DMA_CURRENTTARGETMEM0:	// Switching from Target 0 to Target 1
		default:

			while ( 0 != oOLED::DoubleBuffer::bOLEDBusy_DMA2[CURRENTTARGETMEM1] ) {
				
				__WFI(); // beware: puts mcu in lowe power-mode, this the only place this is acceptable (ToggleBuffers())
				
				if ( 0 == oOLED::DoubleBuffer::bOLEDBusy_DMA2[CURRENTTARGETMEM1] )
					break;
				else if ( 0 == oOLED::DoubleBuffer::bOLEDBusy_DMA2[CURRENTTARGETMEM0] )
					goto TargetMem_1;																// bugfix: prevents strange lockup

			}
TargetMem_0:
			oOLED::DoubleBuffer::bOLEDBusy_DMA2[CURRENTTARGETMEM1] = 1;
		
		// Move current UserFrameBuffer(0) to active, flush cache of pending active framebuffer(0) prior to DMA transfer (ensure commited to memory)
			// required as memory is in SRAM, which is WBWA
			//SCB_CleanDCache_by_Addr((uint32_t*)oOLED::DoubleBuffer::_FrameBuffer1, oOLED::FrameBufferLength);  // ### working without ? is it config'd WT?
		
		// Set new target 1 and send to OLED
		LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_3);
			LL_DMA_SetCurrentTargetMem(DMA2, LL_DMA_STREAM_3, LL_DMA_CURRENTTARGETMEM1);
			

		// Last active framebuffer(1) now free to use as UserFrameBuffer->DitherFrameBuffer
			_DitherTargetFrameBuffer = oOLED::DoubleBuffer::_FrameBuffer0;
#if defined(_DEBUG_OUT_OLED) && defined(MEASURE_FRAMEBUFFER_UTILIZATION)
			++countUsingTarget_0;
#endif
			break;
	}
	
#if defined(_DEBUG_OUT_OLED) && defined(MEASURE_FRAMEBUFFER_UTILIZATION)
	SerDebugOut_Float(0, (float)(countUsingTarget_0 * 100) / (float)(countUsingTarget_0 + countUsingTarget_1));
	SerDebugOut_Float(1, (float)(countUsingTarget_1 * 100) / (float)(countUsingTarget_0 + countUsingTarget_1));
#endif
}

namespace OLED // public access
{
void ClearFrameBuffers(uint32_t const tNow) // called from main() renderscene
{	
	// waiting not required, rendering only proceeds on atomic state == DMA2D_COMPLETED
	// xDMA2D::Wait_DMA2D<true>(); // b4 going forward, the main DMA2D operation must be complete
															   // otherwise they could be overwritten (backbuffer/frontbuffer) before blending is complete
	
	// Exploiting parellism of dma2d, both the lp timer interrupt and the first calls to modify
	// the back or front buffers must wait until the main dma2d blending operation is complete
	// from the *previous* frame before starting a new frame. Any other dma2d usage before completion is also prohibited.
	
	// Clear FrameBuffers //
	xDMA2D::ClearBuffer_8bit<true>((uint8_t* const __restrict)DTCM::getBackBuffer());
	xDMA2D::ClearBuffer_32bit<true, false>((uint32_t* const __restrict)DTCM::getFrontBuffer());
	// while dma2d is clearing frontbuffer, use cpu to clear depth buffer must be cleared to INT8_MIN
	memset(DTCM::getDepthBuffer(), INT8_MIN, oOLED::Width*oOLED::Height*sizeof(int8_t));
	
	xDMA2D::Wait_DMA2D<true>();	
	
	// Start the dualfilter (Bloom HDR post-process) // 
	if (nullptr != BloomHDRLastFrameBuffer) {
		xDMA2D::DualFilterBlur_BeginAsync(BloomHDRLastFrameBuffer);
	}
}
} // end namespace

/*
// obsolete, replaced by asmDitherBlit, 7X improvement in speed, for debugging/reference implementation only 
NOINLINE static void DitherBlit()
{
	static constexpr uint8_t const n255by15 = (255/15),
													        n0F = 0x0F,
												          nF0 = 0xF0;
	
	uint32_t const * __restrict pi
					__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))((uint32_t const* __restrict)_DMA2DFrameBuffer);
	uint8_t const * __restrict pd
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)));
	uint8_t * __restrict po0
	  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)));

	
	//void __pldx(unsigned int #access_kind#, 0 = read, 1 = write
   //           unsigned int #cache_level#, 0 = L1, 1 = L2, 2 = L3 Cache
     //         unsigned int #retention_policy#, 0 = Normal (temporal), 1 = Streaming (memory only used once?)
       //       void const volatile *addr);
					
	// profiles better perfrormance 776us vs 804us and ramfunc also increases performance from 811us with pld enabled!					
	__pldx(1, 0, 0, _DitherTargetFrameBuffer); // in sram1 or sram2 memory
	
	// 256 to 16-color dither (ordered)
  // 2 px/byte
	// 4 pixels at a time using simd intrinsics
		
	int32_t yy(oOLED::Height - 1);

	do {
	//for (uint32_t y=Height-1;0!=y;--y) { 
		
		pd=_dithertable+(((yy&7)<<3)+7);
		
			// output pixel setup line (this is where the 90 cw rotation is undone)
			// image[y][x] // assuming this is the original orientation 
			// image[x][original_width - y] // rotated 90 degrees ccw
			// image[original_height - x][y] // 90 degrees cw 
			// image[original_height - y][original_width - x] // 180 degrees
			// p_myImage2[offset].pixel = p_myImage[width * (height - 1 - y) + x].pixel; rotated 90 clockwise
			po0 = _DitherTargetFrameBuffer + (yy * oOLED::FrameBufferWidth + oOLED::StartXOffset);
			//po0 = _DitherTargetFrameBuffer + (oOLED::FrameBufferWidth * (oOLED::Height - 1 - yy) + oOLED::StartXOffset);
		
			int32_t xxx(oOLED::Width - 1);
			
			do {
			//for (uint32_t x=Width;0!=x;x-=8) {
				pd -= 8;
								
				#pragma unroll
				for(int32_t unroll=1; unroll >= 0; --unroll)
				{
					PixelsQuad o;
					
					{ // scope temporaries for possible compiler optimizations
						uint8_t const p0 = *pi++;
						uint8_t const p1 = *pi++;
						uint8_t const p2 = *pi++;
						uint8_t const p3 = *pi++;
						
					// Use Dither lut table, pack both nyblles into simd structure for parallel unsigned saturation (nneeded to prevent overflow
					PixelsQuad const a( (p0&nF0), (p1&nF0), (p2&nF0), (p3&nF0) );
					
					o.Set( ((*pd++)   - (p0&n0F)*n255by15)>>4&16,
								 ((*pd++) - (p1&n0F)*n255by15)>>4&16,
								 ((*pd++) - (p2&n0F)*n255by15)>>4&16,
								 ((*pd++) - (p3&n0F)*n255by15)>>4&16 );
					

					// saturating add
					o.v = __UQADD8(a.v, o.v);
					}
					
					// output the 2 pixels
					*po0++ = (o.pixels.p0 & nF0) | ((o.pixels.p1 >> 4) & n0F);
					// output 2 more pixels
					*po0++ = (o.pixels.p2 & nF0) | ((o.pixels.p3 >> 4) & n0F);
					
					// next output pixels
					// iterate to next pixel, just processed four
				}
				
				xxx -= 8;
			} while(xxx >= 0);
			
		} while (--yy >= 0);
}
*/
NOINLINE static void DrawTextLayer(uint32_t const& tNow);
static void DrawScreenSavingScanline(uint32_t const& tNow);

namespace OLED
{

uint32_t const getActiveStreamDoubleBufferIndex()
{
	return( (LL_DMA_CURRENTTARGETMEM0 == LL_DMA_GetCurrentTargetMem(DMA2, LL_DMA_STREAM_3) ? CURRENTTARGETMEM0 : CURRENTTARGETMEM1) );
}

static void SwapBloomHDRBuffers()
{
		// Swap the HDRBloom Buffers
	BloomHDRLastFrameBuffer = BloomHDRTargetFrameBuffer;
	if ( (uint8_t* const __restrict)oOLED::DoubleBufferBloomHDR::_FrameBuffer0 == BloomHDRTargetFrameBuffer )
		BloomHDRTargetFrameBuffer = (uint8_t* const __restrict)oOLED::DoubleBufferBloomHDR::_FrameBuffer1;
	
	else
		BloomHDRTargetFrameBuffer = (uint8_t* const __restrict)oOLED::DoubleBufferBloomHDR::_FrameBuffer0;
	
	// Clear / Prepare the new target
	memset(BloomHDRTargetFrameBuffer, 0, oOLED::Width*oOLED::Height*sizeof(uint8_t));	// not using DMA2D clear due to ongoing Main DMA2D Blend op
																																										// taking advatage of parallism here
}
void Render(uint32_t const tNow)
{
	// need to allow any previous dma2d operation to finish before entry into oled finalization Render func
	/* Wait until transfer is over */
  xDMA2D::Wait_DMA2D<true>();

	xDMA2D::DualFilterBlur_EndAsync();  // wait for dual filter blur to finish, blend result into frontbuffer

	DrawTextLayer(tNow);
	
	DrawScreenSavingScanline(tNow);
	
	Configure_DMA2D_Main();			// all rendering to backbuffer and frontbuffer is complete at this state
	
	/* Start transfer */
  xDMA2D::Start_DMA2D<false>();	// async operation for foreground + background blend
	
	if ( tNow - oOLED::LastSync > oOLED::Sync_Interval ) // after a really long time screen starts to drift horizontally (bugfix)
		SyncAddress(tNow);
	
	bbRenderSync->tLastRenderCompleted = millis(); // update timestamp 1st b4 change in state - required for proper synchronization
	bbRenderSync->State = OLED::RenderSync::LOADED;	// flag/signal to pendiing LP Timer 60 Hz Render interrupt that it has work todo
	
	IsoDepth::UpdateDynamicRange();	// taking advantage of async parallism of current dma2d operation ongoing

	SwapBloomHDRBuffers();

}

// Can take advantage of async dma2d operation between RenderOLED and SendFrameBuffer, ie update quadspi, update various rendering states depth
// anything not requiring dma2d
void SendFrameBuffer(uint32_t const tNow) // called from LP Timer 60hz interrupt
{	
	// Exploiting parellism of dma2d, both the lp timer interrupt and the first calls to modify
	// the back or front buffers must wait until the main dma2d blending operation is complete
	// from the *previous* frame before starting a new frame. Any other dma2d usage before completion is also prohibited.
	
	// Wait until transfer is over
  // xDMA2D::Wait_DMA2D<true>(); // this test has been moved outside of function with increased conccurrency and efficiency do NOT wait inside interrupt
	SCB_InvalidateDCache_by_Addr((uint32_t* const __restrict)_DMA2DFrameBuffer, oOLED::Width*oOLED::Height*sizeof(uint32_t)); 
	// after the main dma op, if cpu is going to read (DitherBlit) - must invalidate cache
	// invalidate = cached data may be invalid at this point, flag as need to reload cache with new fresh data from memory
	
	bbRenderSync->State = OLED::RenderSync::MAIN_DMA2D_COMPLETED;	// signal main() realtime loop that it can continue to render

	// set a breakpoint here and then capture of either 4bit raw oled display buffer or 8bit oled display buffer can be accurately captured during debug
	
	asmDitherBlit();    /// awesome - greater than 7X improvement in speed over c version of ditherblit, may be more as it seems 100us is a possib le limit on resolution of micros()
											// does require that premption of by systick does not happen during SendFrameBuffer (done by root interrupt function)
#if( 0 != USART_ENABLE )
	USART::PushFrameBuffer( _DitherTargetFrameBuffer );
#endif
	ToggleFrameBuffers();
}
}//end namespace

STATIC_INLINE void DMA2D_SetMemory_Font(uint32_t const MemoryAddress,
																	      uint32_t const Offset, uint32_t const NumberOfLine, uint32_t const PixelPerLine) 
{	
	// Set memory settings
	
	// regular framebuffer orientation:
	// TM_INT_DMA2DGRAPHIC_SetMemory(DIS.PixelSize * (y * DIS.Width + x), DIS.Width - width, height, width);
	// Working_address = Target_Address + (X +  Target_Width * Y) * BPP
	// LineOffset = Target_Width - Source_Width
	
	xDMA2D::DMA2D_SetMemory_TargetIsBGND(MemoryAddress, Offset, NumberOfLine, PixelPerLine);
}

STATIC_INLINE void DMA2D_SetGlyphAddress(uint32_t const MemoryAddress,
																				 uint32_t const Offset)
{
	// Foreground source memory address which in this case is blended with the background source buffer
	xDMA2D::DMA2D_SetMemory_FGND(MemoryAddress, Offset);
}

/*
STATIC_INLINE void DMA2D_SetMemory_Font(uint32_t const MemoryAddress,
																	      uint32_t const Offset, uint32_t const NumberOfLine, uint32_t const PixelPerLine) 
{	
	// Set memory settings 
	
	// TM_INT_DMA2DGRAPHIC_SetMemory(DIS.PixelSize * (y * DIS.Width + x), DIS.Width - width, height, width);
	// Working_address = Target_Address + (X +  Target_Width * Y) * BPP
	// LineOffset = Target_Width - Source_Width
	
	LL_DMA2D_ConfigSize(DMA2D, NumberOfLine, PixelPerLine);
	
	LL_DMA2D_SetOutputMemAddr(DMA2D, MemoryAddress );
	LL_DMA2D_SetLineOffset(DMA2D, Offset);
	
	// background source memory
	LL_DMA2D_BGND_SetMemAddr(DMA2D, MemoryAddress);
	LL_DMA2D_BGND_SetLineOffset(DMA2D, Offset);
}

STATIC_INLINE void DMA2D_SetGlyphAddress(uint32_t const MemoryAddress,
																				 uint32_t const Offset)
{
	// Foreground source memory address which in this case is blended with the background source buffer
	LL_DMA2D_FGND_SetMemAddr(DMA2D, MemoryAddress);
	LL_DMA2D_FGND_SetLineOffset(DMA2D, Offset);
}
*/

static constexpr uint32_t const SPACE_CHAR = 32;

static void Begin_DMA2D_FontOp()
{
	LL_DMA2D_LayerCfgTypeDef foregroundLayer, backgroundLayer;
		
	LL_DMA2D_LayerCfgStructInit(&foregroundLayer);
	LL_DMA2D_LayerCfgStructInit(&backgroundLayer);
	
	// background config //
	backgroundLayer.ColorMode = LL_DMA2D_INPUT_MODE_ARGB8888;
	backgroundLayer.AlphaMode = LL_DMA2D_ALPHA_MODE_NO_MODIF;
	backgroundLayer.Red = backgroundLayer.Green = backgroundLayer.Blue = 0x00;
	backgroundLayer.Alpha = 0XFF;
	backgroundLayer.AlphaInversionMode = LL_DMA2D_ALPHA_REGULAR;

	// Changing foreground layer

	// foreground config //
	foregroundLayer.ColorMode = LL_DMA2D_INPUT_MODE_A8;
	foregroundLayer.AlphaMode = LL_DMA2D_ALPHA_MODE_COMBINE; // LL_DMA2D_ALPHA_MODE_NO_MODIF
	foregroundLayer.Red = foregroundLayer.Green = foregroundLayer.Blue = 0xFF;
	foregroundLayer.AlphaInversionMode = LL_DMA2D_ALPHA_REGULAR;
	
	LL_DMA2D_ConfigLayer(DMA2D, &backgroundLayer, 0);
	LL_DMA2D_ConfigLayer(DMA2D, &foregroundLayer, 1);
	
	/* Configure DMA2D output color mode */
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_M2M_BLEND);
}
static uint32_t const Set_DMA2D_FontGlyphOp(uint32_t const x, uint32_t const y, char cCharacter, uint8_t const Luma = 0xFF)
{
	cCharacter = oOLED::CurFont->FONT_ASCII_END < 'z' ? std::toupper(cCharacter) : cCharacter;
	if ( SPACE_CHAR == cCharacter )
	{
		return(oOLED::CurFont->FONT_SPACEWIDTH);
	}
	else if ( likely( cCharacter >= oOLED::CurFont->FONT_ASCII_START && cCharacter <= oOLED::CurFont->FONT_ASCII_END
						        && (y + oOLED::CurFont->FONT_GLYPHHEIGHT) < oOLED::Height && (x + oOLED::CurFont->FONT_GLYPHWIDTH) < oOLED::Width ))
	{
		uint32_t const resolvedCharacter = cCharacter - oOLED::CurFont->FONT_ASCII_START;
		
		uint32_t const yGlyph = oOLED::CurFont->FONT_GLYPHHEIGHT * (resolvedCharacter / oOLED::CurFont->FONT_GLYPHSWIDE);
		uint32_t const xGlyph = oOLED::CurFont->FONT_GLYPHWIDTH * (resolvedCharacter % oOLED::CurFont->FONT_GLYPHSWIDE);
		
		// Wait foor previous operation to finish
		xDMA2D::Wait_DMA2D<false>();	// WFI addds addional overhead not suitable for each chasracter drawn in Stringv
		
		// Update Alpha Level
		LL_DMA2D_FGND_SetAlpha(DMA2D, Luma);
		
		// Select Glyph
		DMA2D_SetGlyphAddress((uint32_t const)(oOLED::CurFont->FontMap + ((yGlyph * oOLED::CurFont->FONT_WIDTH) + xGlyph)),
													oOLED::CurFont->FONT_WIDTH - oOLED::CurFont->FONT_GLYPHWIDTH);

		// Update background layer "window"
		DMA2D_SetMemory_Font((uint32_t const)( &DTCM::getFrontBuffer()[y][x] ), (oOLED::Width - oOLED::CurFont->FONT_GLYPHWIDTH), 
													oOLED::CurFont->FONT_GLYPHHEIGHT, oOLED::CurFont->FONT_GLYPHWIDTH);
		
		/* Start transfer */
		xDMA2D::Start_DMA2D<false>();
		 
		return(oOLED::CurFont->CHARACTER_WIDTH[resolvedCharacter] /* oOLED::CurFont->FONT_InvMaxCharacterWidth) 
						* oOLED::CurFont->FONT_GLYPHWIDTH*/);
	}
	return(0);
}

static uint32_t const DrawFloat(uint32_t x, uint32_t const y, float fValue, uint8_t const Luma = 0xFF, char const* const szPrecisionMask = "%.03f")
{
	static constexpr uint32_t const MAX_COUNT = 12 + 1 + 1;
	char str[MAX_COUNT]; // 10 chars max for (without sign), plus sign, period, zero, null terminator

	uint32_t const uiXStart(x);
	
	// Convert floating-point number to characters
	snprintf(str, MAX_COUNT, szPrecisionMask, fValue);

	// Draw a number
	Begin_DMA2D_FontOp();
	
	uint32_t uiCharacterWidth(0);
	char const* pStr = str;
	while (*pStr){ // until null terminator
		
		if (0 == (uiCharacterWidth = Set_DMA2D_FontGlyphOp(x, y, *pStr, Luma)))
			break; // Stop rendering string, either invalid character or reached outside of screen bounds
		
		x += uiCharacterWidth + oOLED::CurFont->FONT_SPACING;
		++pStr;
	}
	
	return(x - uiXStart);
}
static uint32_t const DrawInteger(uint32_t x, uint32_t const y, int iValue, uint8_t const Luma = 0xFF)
{
	char str[12]; // 10 chars max for INT32_MIN..INT32_MAX (without sign)
	char *pStr = str;
	uint32_t const uiXStart(x);
	bool neg = false;

	// String termination character
	*pStr++ = '\0';

	// Convert number to characters
	if (iValue < 0) {
		neg = true;
		iValue = absolute(iValue);
	}
	do { *pStr++ = (iValue % 10) + '0'; } while (iValue /= 10);
	if (neg) 
		*pStr++ = '-';

	// Draw a number
	Begin_DMA2D_FontOp();
	
	uint32_t uiCharacterWidth(0);
	while (*--pStr){ 
		
		if (0 == (uiCharacterWidth = Set_DMA2D_FontGlyphOp(x, y, *pStr, Luma)))
			break; // Stop rendering string, either invalid character or reached outside of screen bounds
		
		x += uiCharacterWidth + oOLED::CurFont->FONT_SPACING;
	}
	
	return(x - uiXStart);
}
static uint32_t const DrawString(uint32_t x, uint32_t const y, char const* const szString, uint8_t const Luma = 0xFF)
{
	char CurCharacter(0);
	char const* pWalkString = szString;
	
	uint32_t uiCharacterWidth(0);
	uint32_t const uiXStart(x);
	
	Begin_DMA2D_FontOp();
	
	while(0x00 != (CurCharacter = *pWalkString++))
	{
		if (0 == (uiCharacterWidth = Set_DMA2D_FontGlyphOp(x, y, CurCharacter, Luma)))
			break; // Stop rendering string, either invalid character or reached outside of screen bounds
		
		x += uiCharacterWidth + oOLED::CurFont->FONT_SPACING;
	}
	
	return(x - uiXStart);
}

namespace OLED
{
uint32_t const getStringPixelLength(char const* const szString)
{
	char CurCharacter(0);
	char const* pWalkString = szString;
	
	uint32_t uiStringLength(0), uiCharacterWidth(0);

	while(0x00 != (CurCharacter = *pWalkString++))
	{
		CurCharacter = oOLED::CurFont->FONT_ASCII_END < 'z' ? std::toupper(CurCharacter) : CurCharacter;
		if (SPACE_CHAR == CurCharacter)
		{
			uiStringLength += oOLED::CurFont->FONT_SPACEWIDTH;
		}
		else if ( likely(CurCharacter >= oOLED::CurFont->FONT_ASCII_START && CurCharacter <= oOLED::CurFont->FONT_ASCII_END) )
		{
			uiCharacterWidth = oOLED::CurFont->CHARACTER_WIDTH[CurCharacter - oOLED::CurFont->FONT_ASCII_START] /* oOLED::CurFont->FONT_InvMaxCharacterWidth
													* oOLED::CurFont->FONT_GLYPHWIDTH*/;
			
			uiStringLength += uiCharacterWidth;
			uiStringLength += oOLED::CurFont->FONT_SPACING;
		}
		else
				break;
	}
	
	return(uiStringLength);
}
} //end namespace

#ifdef _DEBUG_OUT_OLED
sDEBUG::sDebugMessageQueue sDEBUG::oDebugMessageQueue[QUEUE_SIZE]
	__attribute__((section (".bss.debugmessagequeue")));// = { {0}, {0}, {0} };
uint32_t sDEBUG::sDebugMessageQueue::Position(0);
DEBUG_ONLY oDEBUG
__attribute__((section (".bss.debugobject")));
#endif

#ifdef _DEBUG_OUT_OLED
__attribute__ ( (noinline) ) static void DebugLog(uint32_t const& tNow)
{
	static constexpr uint32_t tIntervalGrayDelta = 33,
														TTL = 16000, // ms
														OFFSET = 1,
														NUM_LINES = DEBUG_ONLY::QUEUE_SIZE + 1;
	
	static struct sLogTicker
	{
		char szLine[NUM_LINES][256];
		int32_t tLife[NUM_LINES];
		uint8_t Luma[NUM_LINES];
		uint32_t uiLinesActive;
		
		sLogTicker()
		: uiLinesActive(0)
		{
			for ( uint32_t iDx = 0 ; iDx < NUM_LINES ; ++iDx ) {
				strncpy(szLine[iDx], "", DEBUG_ONLY::MESSAGE_BYTES);
				tLife[iDx] = 0;
				Luma[iDx] = 0;
			}
		}
	} oLogTicker;
	
	static uint32_t tLastFade;
	
	uint32_t const tDelta = min(tNow - tLastFade, tIntervalGrayDelta << 2);
	if ( !IsDebugQueueEmpty() )
	{
		//uint32_t uiEnd(NUM_LINES-1);
		//while ( uiEnd != 0 )
		{
			//strcpy(oLogTicker.szLine[uiEnd], oLogTicker.szLine[uiEnd+1]);
			//oLogTicker.tLife[uiEnd] = oLogTicker.tLife[uiEnd+1];
			
			strncpy(oLogTicker.szLine[3], oLogTicker.szLine[2], DEBUG_ONLY::MESSAGE_BYTES);
			strncpy(oLogTicker.szLine[2], oLogTicker.szLine[1], DEBUG_ONLY::MESSAGE_BYTES);
			strncpy(oLogTicker.szLine[1], oLogTicker.szLine[0], DEBUG_ONLY::MESSAGE_BYTES);
			oLogTicker.tLife[3] = (oLogTicker.tLife[2] * 0.98f);
			oLogTicker.tLife[2] = (oLogTicker.tLife[1] * 0.89f);
			oLogTicker.tLife[1] = (oLogTicker.tLife[0] * 0.81f);
			
			//oLogTicker.Luma[iEnd] = oLogTicker.Luma[iEnd];
			//--uiEnd;
		}
		//strcpy(oLogTicker.szLine[0], GetCurDebugMessage(0));
		//oLogTicker.tLife[0] = TTL;
		
		int32_t iPos = DebugQueuePosition;
		while (iPos >= 0)
		{
			if ( 0 != GetCurDebugMessage(iPos)[0] ) {
				
				strncpy(oLogTicker.szLine[iPos], GetCurDebugMessage(iPos), DEBUG_ONLY::MESSAGE_BYTES);
				oLogTicker.tLife[iPos] = TTL;
				
				if ( oLogTicker.uiLinesActive < NUM_LINES - 1 )
					++oLogTicker.uiLinesActive;
			}
			--iPos;
		}
		
		ResetDebugMessageQueue();
	}
	
	if ( tDelta > tIntervalGrayDelta )
	{
		int32_t i = oLogTicker.uiLinesActive - 1;
		
		while ( i >= 0 ) 
		{
			if (oLogTicker.tLife[i] > 0)
			{
				if ( (oLogTicker.tLife[i] -= tDelta) > 0 )
				{ 
					oLogTicker.Luma[i] = __USAT( (int32_t const) lerp(0.0f, 255.0f, ((float32_t const)oLogTicker.tLife[i] / (float32_t const)(TTL))), Constants::SATBIT_256 );
				}
				else if (oLogTicker.uiLinesActive > 0) {
					oLogTicker.tLife[i] = 0;	// clamp to 0
					--oLogTicker.uiLinesActive;
				}
				else {
					oLogTicker.tLife[i] = 0;	// clamp to 0
				}
			}
			else
				oLogTicker.tLife[i] = 0;	// clamp to 0
			--i;
		}
		
		tLastFade = tNow;
	}
	
	static constexpr int32_t const GRAYLEVEL_DELTA = 35,		// this is subtracted from actual value
																 GRAYLEVEL_MAX = 170,			// so max being our greatest subtractor, min the smallest
																 GRAYLEVEL_MIN = 0,				// constexpr must be signed int!!!! 
																 GRAYLEVEL_RATE = 18;
	
	static bool bDirection(true);
	
	static int32_t iLuma(GRAYLEVEL_MIN), tLastMove;
	
	if (tNow - tLastMove > GRAYLEVEL_DELTA)
	{
		if ( bDirection )
		{
			if( (iLuma += GRAYLEVEL_RATE) >= GRAYLEVEL_MAX) {
				iLuma = GRAYLEVEL_MAX;
				bDirection = false;
			}
		}
		else {
			
			if( (iLuma -= GRAYLEVEL_RATE) <= GRAYLEVEL_MIN ) {
				iLuma = GRAYLEVEL_MIN;
				bDirection = true;
			}
		}
			
		tLastMove = tNow;
	}
	
	uint32_t uiY = oOLED::CurFont->FONT_GLYPHHEIGHT + OFFSET;
	int32_t i(0);
		// fixed is now correct order with newest on bottom always, fading to oldest to top of screen
	while ( i < oLogTicker.uiLinesActive ) 
	{
		if ( oLogTicker.tLife[i] ) {
			uint32_t const LogLineLuma = __USAT( (int32_t)oLogTicker.Luma[i] - (iLuma >> (i << 1) )
				                                        + 32, Constants::SATBIT_256 );
			
			if (LogLineLuma != 0 ) {
				DrawString(OFFSET, oOLED::Height - uiY, oLogTicker.szLine[i], 
									 LogLineLuma);
			}
		}
		++i;
		uiY += oOLED::CurFont->FONT_GLYPHHEIGHT + 1;
	}
}
#endif
__attribute__ ( (noinline) ) static void DebugFrameTiming(uint32_t const& tNow)
{
	static constexpr uint32_t const NUM_SAMPLES_MS = 4,
																	SPIKE_MS = 4,
																	GRAYLEVEL_DELTA = 30,
																	GRAYLEVEL_MIN = 129,
																	GRAYLEVEL_RATE = 4;
	
	static uint32_t uiTotalLength(0);
	static uint32_t tFrameLast, tLastUpdateAvg;
	static uint32_t tSamples[NUM_SAMPLES_MS] = {0};
	
	static bool bDirection(true), bInitValues(true);
	
	static int32_t iLuma(GRAYLEVEL_MIN), tLastMove;
	
	if ( unlikely(bInitValues) ) {
		tFrameLast = tNow;
		tLastUpdateAvg = tNow;
		bInitValues = false;
	}
	
	if (tNow - tLastMove > GRAYLEVEL_DELTA)
	{
		if ( bDirection )
		{
			if( (iLuma += GRAYLEVEL_RATE) >= Constants::n255) {
				iLuma = Constants::n255;
				bDirection = false;
			}
		}
		else {
			
			if( (iLuma -= GRAYLEVEL_RATE) <= GRAYLEVEL_MIN ) {
				iLuma = GRAYLEVEL_MIN;
				bDirection = true;
			}
		}
			
		tLastMove = tNow;
	}
	
	// update tme samplin of ms/frame
	uint32_t const tDelta = tNow - tFrameLast;
	
	// highlight spikes
	if ( tDelta > (tSamples[0] + SPIKE_MS) ) {
		tSamples[3] = tDelta;
		tSamples[2] = tDelta;
		tSamples[1] = tDelta;
		tSamples[0] = tDelta;
		
	} // Smooth display of time
	else if ( (tNow - tLastUpdateAvg) > ((tSamples[0] + tSamples[3]) << 1)) {
		tSamples[3] = tSamples[2];
		tSamples[2] = tSamples[1];
		tSamples[1] = tSamples[0];
		tSamples[0] = tDelta;
		
		tLastUpdateAvg = tNow;
	}
	
	uint32_t const tFrameAvg = (tSamples[3] + tSamples[2] + tSamples[1] + tSamples[0]) >> 2;
	
	uint32_t uiStringLength(0);
	
	if ( unlikely(0 == uiTotalLength) ) {
		uiStringLength += DrawInteger(0, 0, tFrameAvg, iLuma);
		uiStringLength += oOLED::CurFont->FONT_SPACEWIDTH << 1;
		
		uiTotalLength = uiStringLength;
	}
	else {
		uiStringLength += DrawInteger((oOLED::Width - uiTotalLength), (oOLED::Height >> 1) - (oOLED::CurFont->FONT_GLYPHHEIGHT >> 1), tFrameAvg, iLuma);
		uiStringLength += oOLED::CurFont->FONT_SPACEWIDTH;
		
		//uiTotalLength = max(uiTotalLength, uiStringLength);
	}
	
	tFrameLast = tNow;
}

NOINLINE static void DrawTextLayer(uint32_t const& tNow)
{
#ifdef _DEBUG_OUT_OLED
	static constexpr uint32_t const NUM_DEBUG_SEPARATE_LINES = 2;
	static constexpr uint32_t const GRAYLEVEL_DELTA = 30,
																	GRAYLEVEL_MIN = 0,
																	GRAYLEVEL_RATE = 2;
	
	static uint32_t uiTotalLength[NUM_DEBUG_SEPARATE_LINES] = {0};
	static bool bDirection(true);
	
	static uint32_t iLuma(GRAYLEVEL_MIN), tLastMove;
	
	DebugLog(tNow);
	if (tNow - tLastMove > GRAYLEVEL_DELTA)
	{
		if ( bDirection )
		{
			if( (iLuma += GRAYLEVEL_RATE) >= Constants::n255) {
				iLuma = 255;
				bDirection = false;
			}
		}
		else {
			
			if( (iLuma -= GRAYLEVEL_RATE) <= GRAYLEVEL_MIN ) {
				iLuma = GRAYLEVEL_MIN;
				bDirection = true;
			}
		}
			
		tLastMove = tNow;
	}
	
	uint32_t const FIRST_DEBUG_LINE_Y = 0,
								 SECOND_DEBUG_LINE_Y = oOLED::CurFont->FONT_GLYPHHEIGHT*2+3*3;
			
	uint32_t uiStringLength[NUM_DEBUG_SEPARATE_LINES] = {0};
	if ( 0 == uiTotalLength[0] ) {

		uiStringLength[0] += DrawInteger(0, FIRST_DEBUG_LINE_Y, oDEBUG.i[3], iLuma);
		uiStringLength[0] += DrawString(uiStringLength[0], FIRST_DEBUG_LINE_Y, "%", iLuma);
		
#if defined(_DEBUG_OUT_OLED) && defined(MEASURE_FRAMEBUFFER_UTILIZATION)
		uiStringLength[1] += DrawFloat(0, SECOND_DEBUG_LINE_Y, oDEBUG.f[0], iLuma);
		uiStringLength[1] += DrawString(uiStringLength[1], SECOND_DEBUG_LINE_Y, "/", iLuma);
		uiStringLength[1] += DrawFloat(uiStringLength[1], SECOND_DEBUG_LINE_Y, oDEBUG.f[1], iLuma);
		uiStringLength[1] += DrawString(uiStringLength[1], SECOND_DEBUG_LINE_Y, " ( ", iLuma);
		uiStringLength[1] += DrawInteger(uiStringLength[1], SECOND_DEBUG_LINE_Y, oDEBUG.i[0], iLuma);
		uiStringLength[1] += DrawString(uiStringLength[1], SECOND_DEBUG_LINE_Y, ") ", iLuma);
#endif

		for ( uint32_t iDx = 0 ; iDx < NUM_DEBUG_SEPARATE_LINES ; ++iDx )
			uiTotalLength[iDx] = uiStringLength[iDx];
	}
	else {
		if ( DEBUG_OUT_RESET != oDEBUG.i[3] ) {
			uiStringLength[0] += DrawInteger((oOLED::Width - uiTotalLength[0]), FIRST_DEBUG_LINE_Y, oDEBUG.i[3], iLuma);
			uiStringLength[0] += DrawString((oOLED::Width - uiTotalLength[0]) + uiStringLength[0], FIRST_DEBUG_LINE_Y, "%", iLuma);
		}
#if defined(_DEBUG_OUT_OLED) && defined(MEASURE_FRAMEBUFFER_UTILIZATION)
			uiStringLength[1] += DrawFloat((oOLED::Width - uiTotalLength[1]), SECOND_DEBUG_LINE_Y, oDEBUG.f[0], iLuma);
			uiStringLength[1] += DrawString((oOLED::Width - uiTotalLength[1]) + uiStringLength[1], SECOND_DEBUG_LINE_Y, "/", iLuma);
			uiStringLength[1] += DrawFloat((oOLED::Width - uiTotalLength[1]) + uiStringLength[1], SECOND_DEBUG_LINE_Y, oDEBUG.f[1], iLuma);
			uiStringLength[1] += DrawString((oOLED::Width - uiTotalLength[1]) + uiStringLength[1], SECOND_DEBUG_LINE_Y, " ( ", iLuma);
			uiStringLength[1] += DrawInteger((oOLED::Width - uiTotalLength[1]) + uiStringLength[1], SECOND_DEBUG_LINE_Y, oDEBUG.i[0], iLuma);
			uiStringLength[1] += DrawString((oOLED::Width - uiTotalLength[1]) + uiStringLength[1], SECOND_DEBUG_LINE_Y, ") ", iLuma);
#endif
		
		for ( uint32_t iDx = 0 ; iDx < NUM_DEBUG_SEPARATE_LINES ; ++iDx )
			uiTotalLength[iDx] = max(uiTotalLength[iDx], uiStringLength[iDx]);
	}
#endif // DEBUG_OUT
	
#ifdef SHOW_FRAMETIMING
	DebugFrameTiming(tNow);
#endif
}

static void ClearDisplay()
{
	// blank display while clearing (also hides noise at powerup)
  sendC(0xA4); // Set Display Mode = black

	Set_Column_Address(0x00,0x77);
	Set_Row_Address(0x00,0x7f);
  WriteEnable();
	
	for(uint32_t i=0;i<oOLED::Height;i++)
  {
    for(uint32_t j=0;j<(oOLED::FrameBufferWidth>>1);j++)
    {
      SPI1_TransmitByte(0);
		}
	}
	
	sendC(0xA6); // Set Display Mode = normal
	//sendC(0xa7);	//inverted
}



namespace OLED
{
	namespace Effects
	{
		
	void AttachScanlineIntercept(Effects::ScanlinerIntercept* const pInterceptDesc)
	{
		oEffects.pCurInterceptDesc = pInterceptDesc;
	}
	void DetachScanlineIntercept()
	{
		oEffects.pCurInterceptDesc = nullptr;
	}
	
	}//endnamespace
}//endnamespace

static void DrawScreenSavingScanline(uint32_t const& tNow)	// nice has curvature illusion, don't change
{
	static constexpr uint32_t const SCANLINE_SAVER_SPEED_MS = 16,
																	SCANLINE_INSIDE_LINES = 3;
	static constexpr uint32_t const TOTAL_TRANSPARENCY = 0x3d;
	
	static uint32_t ScanlineYPos;
	static uint32_t tLastMove;
	
	bool bDoIntersectionTest(false);
	uint32_t yIntercept(0);
	
	if ( tNow - tLastMove > SCANLINE_SAVER_SPEED_MS )
	{
		ScanlineYPos = (ScanlineYPos + 1) & (oOLED::Height - 1);
		tLastMove = tNow;
				
		// Only on changes in Y position of scanline 
		bDoIntersectionTest = (nullptr != OLED::Effects::oEffects.pCurInterceptDesc);
		if (bDoIntersectionTest) {
			yIntercept = OLED::Effects::oEffects.pCurInterceptDesc->uiYIntercept; // Leverage stack rather than pointer indirection to memory
		}
	
	}
	
	// 2, 			4, 				8, 				16				32				64				128
	// r << 1		r << 2, 	r << 3, 	r << 4		r << 5,		r << 6, 	r << 7
		
	for (int32_t iDx = SCANLINE_INSIDE_LINES - 1; iDx >= 0 ; --iDx) //leverage overflow Y wraparound
	{
		uint32_t WrappedScanLine = ScanlineYPos + iDx;
	
		if (WrappedScanLine >= oOLED::Height) {
			WrappedScanLine = (oOLED::Height + WrappedScanLine) & (oOLED::Height - 1);
		}
		
		switch(iDx)
		{
			case SCANLINE_INSIDE_LINES - 1: // outside lines above and below center line
			case 0:
				if ( CheckHLine(0, WrappedScanLine, oOLED::Width) )
					DrawHLineBlendFast(0, WrappedScanLine, oOLED::Width, xDMA2D::AlphaLuma(TOTAL_TRANSPARENCY>>1 , g9), 127 );
				break;
			default: // "inside" center line
				if ( CheckHLine(0, WrappedScanLine, oOLED::Width) )
					DrawHLineBlendFast(0, WrappedScanLine, oOLED::Width, xDMA2D::AlphaLuma(TOTAL_TRANSPARENCY, g2), 180);
				break;
		}
						
		if (bDoIntersectionTest) {
			if ( yIntercept == WrappedScanLine) {
				OLED::Effects::oEffects.pCurInterceptDesc->bIntercepted = true;
				bDoIntersectionTest = false; // no further test neccessary
			}
		}
	}
}

static void TestGradientBounce(uint32_t const tNow)
{
	static bool bIncreasing(true);
	static uint32_t tLastUpdate(0);
	static int32_t iXMiddle(0);
		
	/*if (tNow - tLastUpdate > 16)
	{
		if (bIncreasing)
		{
			++iXMiddle;
		}
		else
		{
			--iXMiddle;
		}
	}*/
	if (tNow - tLastUpdate > 14)
	{
		if (bIncreasing)
		{
			++iXMiddle;
			
			if ( iXMiddle >= oOLED::Width){
				bIncreasing = false;
				--iXMiddle;
			}
		}
		else
		{
			--iXMiddle;
			
			if ( iXMiddle <= 0 ){
				bIncreasing = true;
				++iXMiddle;
			}
		}
		
		tLastUpdate = tNow;
	}
	
	for ( int32_t iDy = 0; iDy < oOLED::Height; ++iDy)
	{
			for ( int32_t iDx = 0; iDx < oOLED::Width; ++iDx)
			{				
				DrawPixel<OLED::BACK_BUFFER, SHADE_ENABLE>( iDx, iDy, __USAT( absolute(iXMiddle - iDx), Constants::SATBIT_256 ) );
			}
	}
}

namespace OLED
{
void TestPixels(uint32_t const& tNow)
{
	//TestGradientBounce(tNow);
}
void RenderBloomHDRBuffer()
{
	if ( nullptr != BloomHDRLastFrameBuffer )
	{
		xDMA2D::Wait_DMA2D<false>();
		xDMA2D::ClearBuffer_8bit((uint8_t* const __restrict)DTCM::getBackBuffer());
		xDMA2D::ClearBuffer_32bit((uint32_t* const __restrict)DTCM::getFrontBuffer());
		
		Render8bitScreenLayer_Back(BloomHDRLastFrameBuffer);
	}
}
void RenderDepthBuffer()
{
	xDMA2D::Wait_DMA2D<false>();
	xDMA2D::ClearBuffer_8bit((uint8_t* const __restrict)DTCM::getBackBuffer());
	xDMA2D::ClearBuffer_32bit((uint32_t* const __restrict)DTCM::getFrontBuffer());
	
	// Now draw depth buffer representation
	uint32_t const clampWidth = OLED::SCREEN_WIDTH;
	uint32_t clampHeight = OLED::SCREEN_HEIGHT;
		
	uint32_t yPixel = 0;
	uint8_t * __restrict
				__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) UserFrameBuffer;
	
	while( 0 != clampHeight )
  {
		uint32_t wlen = clampWidth;
		uint32_t xPixel = 0;
		
		// note the 1D acess to 2D access has swapped x and y resulting in a 90 degree rotation
		// ie.) no rotation: DTCM::getBackBuffer() + (yPixel << oOLED::Width_SATBITS) = DTCM::getDepthBuffer()[yPixel][xPixel]),
			//                  where DepthBuffer[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH] - x Changes fastest
		// 90 cw rotation: DTCM::getBackBuffer() + (yPixel << oOLED::Width_SATBITS) = DTCM::getDepthBuffer()[xPixel][yPixel]),
			//									where DepthBuffer[OLED::SCREEN_WIDTH][OLED::SCREEN_HEIGHT]  - y Changes fastest
		//UserFrameBuffer = DTCM::getBackBuffer() + (yPixel << oOLED::Width_SATBITS);
		UserFrameBuffer = &DTCM::getBackBuffer()[xPixel][yPixel];
		
		while( 0 != wlen ) 
		{
			// Calculate scaled depth value 
			*UserFrameBuffer = __USAT( uint32::__roundf( lerp(0.0f, 255.0f, IsoDepth::getDistance(DTCM::getDepthBuffer()[xPixel][yPixel]))), Constants::SATBIT_256 );

			++UserFrameBuffer;
			++xPixel;
			--wlen;
			
		}
		++yPixel;
		--clampHeight;
	}
}

void RenderCopy32bitScreenLayerAsync(uint32_t* const __restrict p32bitLayerDst, uint32_t const* const __restrict p32bitLayerSrc)
{
	LL_DMA2D_LayerCfgTypeDef foregroundLayer;
		
	// Wait foor previous operation to finish
	//Wait_DMA2D();
	
	// Configure DMA2D output color mode 
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_M2M);													
	LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_ARGB8888);	
																																			
	LL_DMA2D_ConfigSize(DMA2D, oOLED::Height>>1, oOLED::Width>>1);
	LL_DMA2D_SetLineOffset(DMA2D, 0);
	LL_DMA2D_SetOutputMemAddr(DMA2D, (uint32_t)p32bitLayerDst);
	
	LL_DMA2D_LayerCfgStructInit(&foregroundLayer);
	
	// foreground config //
	foregroundLayer.ColorMode = LL_DMA2D_INPUT_MODE_ARGB8888;
	foregroundLayer.AlphaMode = LL_DMA2D_ALPHA_MODE_NO_MODIF;
	foregroundLayer.Red = foregroundLayer.Green = foregroundLayer.Blue = 0x00;
	foregroundLayer.Alpha = 0xff;
	foregroundLayer.MemoryAddress = (uint32_t)p32bitLayerSrc;

	LL_DMA2D_ConfigLayer(DMA2D, &foregroundLayer, 1);
	
	// Required b4 dma, ensure data is commited to memory from cache
	SCB_CleanDCache_by_Addr((uint32_t*)p32bitLayerSrc, oOLED::Width*oOLED::Height*sizeof(uint32_t));
	
	xDMA2D::Start_DMA2D<false, false>();
}

void RenderCopy8bitScreenLayerAsync(uint8_t* const __restrict p8bitLayerDst, uint8_t const* const __restrict p8bitLayerSrc)
{
	LL_DMA2D_LayerCfgTypeDef foregroundLayer;
		
	// Wait foor previous operation to finish
	//Wait_DMA2D();
	
	// Configure DMA2D output color mode 
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_M2M);													// wont work for blending
	LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_ARGB8888);	// Lying to DMA2D, actual output is L8 
																																			// width/4 to result L8L8L8L8
	LL_DMA2D_ConfigSize(DMA2D, oOLED::Height, oOLED::Width>>2);// pixelsperline must be even and div by 4
	LL_DMA2D_SetLineOffset(DMA2D, 0);
	LL_DMA2D_SetOutputMemAddr(DMA2D, (uint32_t)p8bitLayerDst);
	
	LL_DMA2D_LayerCfgStructInit(&foregroundLayer);
	
	// foreground config //
	foregroundLayer.ColorMode = LL_DMA2D_INPUT_MODE_ARGB8888;		// actually L8
	foregroundLayer.AlphaMode = LL_DMA2D_ALPHA_MODE_NO_MODIF;
	foregroundLayer.Red = foregroundLayer.Green = foregroundLayer.Blue = 0x00;
	foregroundLayer.Alpha = 0xff;
	foregroundLayer.MemoryAddress = (uint32_t)p8bitLayerSrc;

	LL_DMA2D_ConfigLayer(DMA2D, &foregroundLayer, 1);
	
	// Required b4 dma, ensure data is commited to memory from cache
	SCB_CleanDCache_by_Addr((uint32_t*)p8bitLayerSrc, oOLED::Width*oOLED::Height);
	
	xDMA2D::Start_DMA2D<false, false>();
}
void RenderCopy32bitScreenLayer(uint32_t* const __restrict p32bitLayerDst, uint32_t const* const __restrict p32bitLayerSrc)
{
	RenderCopy32bitScreenLayerAsync(p32bitLayerDst, p32bitLayerSrc);
	
	xDMA2D::Wait_DMA2D<false>();
	SCB_InvalidateDCache_by_Addr( (uint32_t*)p32bitLayerDst, oOLED::Width*oOLED::Height*sizeof(uint32_t));
}
void RenderCopy8bitScreenLayer(uint8_t* const __restrict p8bitLayerDst, uint8_t const* const __restrict p8bitLayerSrc)
{
	RenderCopy8bitScreenLayerAsync(p8bitLayerDst, p8bitLayerSrc);
	
	xDMA2D::Wait_DMA2D<false>();
	SCB_InvalidateDCache_by_Addr( (uint32_t*)p8bitLayerDst, oOLED::Width*oOLED::Height);
}
void Render8bitScreenLayer_Back(uint8_t const* const __restrict p8bitLayer0)
{
	RenderCopy8bitScreenLayer((uint8_t* const __restrict)DTCM::getBackBuffer(), p8bitLayer0);
}

void Render32bitScreenLayer_Front(uint32_t const* const __restrict p32bitLayer0)
{
	RenderCopy32bitScreenLayer((uint32_t* const __restrict)DTCM::getFrontBuffer(), p32bitLayer0);
}
void Render8bitScreenLayer_Front(uint8_t const* const __restrict p8bitLayer0, uint32_t const Opacity)
{
	LL_DMA2D_LayerCfgTypeDef foregroundLayer;
		
	// Wait foor previous operation to finish
	//Wait_DMA2D();
	
	// Configure DMA2D output color mode
	LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_M2M_PFC);													
	LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_ARGB8888);	
																																			
	LL_DMA2D_ConfigSize(DMA2D, oOLED::Height, oOLED::Width);
	LL_DMA2D_SetLineOffset(DMA2D, 0);
	LL_DMA2D_SetOutputMemAddr(DMA2D, (uint32_t)DTCM::FrontBuffer);
	
	LL_DMA2D_LayerCfgStructInit(&foregroundLayer);
	
	// foreground config //
	foregroundLayer.ColorMode = LL_DMA2D_INPUT_MODE_L8;
	foregroundLayer.AlphaMode = LL_DMA2D_ALPHA_MODE_REPLACE;
	foregroundLayer.Red = foregroundLayer.Green = foregroundLayer.Blue = 0xff;
	foregroundLayer.Alpha = Opacity;
	foregroundLayer.MemoryAddress = (uint32_t)p8bitLayer0;

	LL_DMA2D_ConfigLayer(DMA2D, &foregroundLayer, 1);
	
	// Required b4 dma, ensure data is commited to memory from cache
	SCB_CleanDCache_by_Addr((uint32_t*)p8bitLayer0, oOLED::Width*oOLED::Height);
	
	xDMA2D::Start_DMA2D<true, false>();
}

void Render8bitScreenLayer_Lerp_Back(uint8_t const* __restrict p8bitLayerA, uint8_t const* __restrict p8bitLayerB, float const tDeltaNorm)
{
	// Color interpolation between two rendered screen size layers. Gives effect of smoothly interpolating moving pixels, 
	// however we are not interpolating position, just the pixels color from one buffer to the next.
	
	uint32_t const clampWidth = OLED::SCREEN_WIDTH;
	uint32_t clampHeight = OLED::SCREEN_HEIGHT;
		
	uint32_t xPixel = 0, yPixel = 0;
	
	while( 0 != clampHeight )
  {
		uint32_t wlen = clampWidth;
		
		xPixel = 0;

		while( 0 != wlen ) 
		{
			// tDeltaNorm is already premultiplied by the inverse of the total transition time (normalized)
			PixelsQuad lerpPixels;
			lerpPixels.SetAndSaturate( int32::__roundf(lerp( *p8bitLayerA++, *p8bitLayerB++, tDeltaNorm )), int32::__roundf(lerp( *p8bitLayerA++, *p8bitLayerB++, tDeltaNorm )),
																 int32::__roundf(lerp( *p8bitLayerA++, *p8bitLayerB++, tDeltaNorm )), int32::__roundf(lerp( *p8bitLayerA++, *p8bitLayerB++, tDeltaNorm )) );
			
			DrawPixel<OLED::BACK_BUFFER, (OLED::SHADE_ENABLE|OLED::BLOOM_DISABLE)>(xPixel, yPixel, lerpPixels.pixels.p0);
			DrawPixel<OLED::BACK_BUFFER, (OLED::SHADE_ENABLE|OLED::BLOOM_DISABLE)>(xPixel + 1, yPixel, lerpPixels.pixels.p1);
			DrawPixel<OLED::BACK_BUFFER, (OLED::SHADE_ENABLE|OLED::BLOOM_DISABLE)>(xPixel + 2, yPixel, lerpPixels.pixels.p2);
			DrawPixel<OLED::BACK_BUFFER, (OLED::SHADE_ENABLE|OLED::BLOOM_DISABLE)>(xPixel + 3, yPixel, lerpPixels.pixels.p3);
			
			wlen -= 4;
			xPixel += 4;
		}
		++yPixel;
		--clampHeight;
	}
}
/*
void Render8bitScreenLayer_Lerp_Front(uint8_t const* __restrict p8bitLayerA, uint8_t const* __restrict p8bitLayerB, float const tDeltaNorm, uint32_t const Opacity )
{
	// Color interpolation between two rendered screen size layers. Gives effect of smoothly interpolating moving pixels, 
	// however we are not interpolating position, just the pixels color from one buffer to the next.

	uint32_t const clampWidth = OLED::SCREEN_WIDTH;
	uint32_t clampHeight = OLED::SCREEN_HEIGHT;
		
	uint32_t yPixel = 0;
	uint32_t * __restrict
				__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) UserFrameBuffer;
	
	while( 0 != clampHeight )
  {
		uint32_t wlen = clampWidth;

		UserFrameBuffer = (uint32_t* const __restrict)DTCM::getFrontBuffer() + (yPixel << oOLED::Width_SATBITS);
		
		while( 0 != wlen ) 
		{
			PixelsQuad const oA( *p8bitLayerA++, *p8bitLayerA++, *p8bitLayerA++, *p8bitLayerA++ );
			PixelsQuad const oB( *p8bitLayerB++, *p8bitLayerB++, *p8bitLayerB++, *p8bitLayerB++ );
			
			// tDeltaNorm is already premultiplied by the inverse of the total transition time (normalized)
			
			PixelsQuad lerpPixels;
			lerpPixels.SetAndSaturate( lerp( oA.pixels.p0, oB.pixels.p0, tDeltaNorm ), lerp( oA.pixels.p1, oB.pixels.p1, tDeltaNorm ),
																 lerp( oA.pixels.p2, oB.pixels.p2, tDeltaNorm ), lerp( oA.pixels.p3, oB.pixels.p3, tDeltaNorm ) );
			
			*UserFrameBuffer++ = xDMA2D::AlphaLuma(Opacity, lerpPixels.pixels.p0).v;
			*UserFrameBuffer++ = xDMA2D::AlphaLuma(Opacity, lerpPixels.pixels.p1).v;
			*UserFrameBuffer++ = xDMA2D::AlphaLuma(Opacity, lerpPixels.pixels.p2).v;
			*UserFrameBuffer++ = xDMA2D::AlphaLuma(Opacity, lerpPixels.pixels.p3).v;
			
			wlen -= 4;	
		}
		++yPixel;
		--clampHeight;
	}
}
*/
 void Render8bitScreenLayer_Blended(uint8_t const* const __restrict p8bitLayerA, uint8_t* const __restrict p8bitOffScreenTarget, uint32_t const Opacity, 
																																			 uint32_t const ShadeRegister)
{
	//_DMA2DScratchBuffer
	Configure_DMA2D_Scratch(p8bitOffScreenTarget, p8bitLayerA, Opacity, ShadeRegister);
	
	xDMA2D::Start_DMA2D(/*true*/);
	
	// Copy result of the 32bit blend op back to the current target
	uint32_t const clampWidth = OLED::SCREEN_WIDTH;
	uint32_t clampHeight = OLED::SCREEN_HEIGHT;
		
	uint32_t yPixel = 0;
	uint32_t * __restrict
				__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) ScratchBufferSrc((uint32_t* __restrict)_DMA2DFrameBuffer);
	uint8_t * __restrict
				__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) TargetFrameBuffer;
	
	while( 0 != clampHeight )
	{
		uint32_t wlen = clampWidth;

		TargetFrameBuffer = p8bitOffScreenTarget + (yPixel << oOLED::Width_SATBITS);
		
		while( 0 != wlen ) 
		{
			PixelsQuad const o0( *ScratchBufferSrc++, *ScratchBufferSrc++, *ScratchBufferSrc++, *ScratchBufferSrc++ );
			
			*(reinterpret_cast<uint32_t* const __restrict>(TargetFrameBuffer)) = o0.v;
			
			TargetFrameBuffer += 4;
			wlen -= 4;
			
		}
		++yPixel;
		--clampHeight;
	}
}
 void Render8bitScreenLayer_AlphaMask_Front(uint8_t const* __restrict p8bitLayerA, uint8_t const* __restrict p8bitAlphaMask)
{
	uint32_t const clampWidth = OLED::SCREEN_WIDTH;
	uint32_t clampHeight = OLED::SCREEN_HEIGHT;
		
	uint32_t yPixel = 0;
	uint32_t * __restrict
				__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) UserFrameBuffer;
	
	while( 0 != clampHeight )
  {
		uint32_t wlen = clampWidth;

		UserFrameBuffer = (uint32_t* const __restrict)DTCM::getFrontBuffer() + (yPixel << oOLED::Width_SATBITS);
		
		while( 0 != wlen ) 
		{ 							
			*UserFrameBuffer |= xDMA2D::AlphaLuma(*p8bitAlphaMask++, *p8bitLayerA++).v;
			
			++UserFrameBuffer;
			--wlen;	
		}
		++yPixel;
		--clampHeight;
	}
}
void Render1bitScreenLayer_Front(uint8_t const* const __restrict p1bitLayer, uint8_t const* const __restrict p1bitAlphaLayer)
{
	uint32_t const clampWidth = oOLED::Width;
	uint32_t clampHeight = oOLED::Height;
		
	uint32_t yPixel = 0;
	
	uint32_t rowOffsetBytes = 0;
	while( 0 != clampHeight )
  {
		uint32_t wlen = clampWidth;
		uint32_t xPixel = 0;
		
		uint8_t bitMask = 1;
		uint32_t columnOffsetBytes = 0;

		while( 0 != wlen ) 
		{
				uint8_t const curByte = *(p1bitLayer+(rowOffsetBytes+columnOffsetBytes));
			
			if ( 0 != (*(p1bitAlphaLayer+(rowOffsetBytes+columnOffsetBytes)) & bitMask) )
				DrawPixel<OLED::FRONT_BUFFER, SHADE_ENABLE>( xPixel, yPixel, 0 == (curByte & bitMask) ? g0 : g15 );
			

			bitMask <<= 1;		//Next bit of current byte

			// Overflow ? We are done current byte
			if ( 0 == bitMask )
			{
				bitMask =  1; // Reset mask for start of new byte

				++columnOffsetBytes; // Next byte
			}
			++xPixel;
			--wlen;
		}
		rowOffsetBytes += columnOffsetBytes;//((clampWidth+7) >> 3);
		
		++yPixel;
		--clampHeight;
	}
}

namespace Effects
{	
	static constexpr uint8_t const MARK_CHANGE_TO_BLACK = 254;	// 254 should work for 99.9% SDF shades
		
	uint8_t const findAverage( uint8_t const* const __restrict pBufferL8 )
	{
		uint32_t const clampWidth = oOLED::Width;
		uint32_t clampHeight = oOLED::Height;
		
		uint8_t const* __restrict
					__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) UserFrameBuffer;
		
		uint32_t yPixel = 0;
		
		uint32_t sumValue(0);
		
		while( 0 != clampHeight )
		{
			uint32_t wlen = clampWidth;
			
			UserFrameBuffer = pBufferL8 + (yPixel << oOLED::Width_SATBITS); //read-only
			
			while( 0 != wlen ) 
			{ 	
				sumValue += *UserFrameBuffer;
				
				++UserFrameBuffer;
				--wlen;
			} //end while
			
			++yPixel;
			--clampHeight;
		} // end while
		
		return( sumValue / (oOLED::Width*oOLED::Height) );
	}
	uint8_t const findMin( uint8_t const* const __restrict pBufferL8 )
	{
		uint32_t const clampWidth = oOLED::Width;
		uint32_t clampHeight = oOLED::Height;
		
		uint8_t const* __restrict
					__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) UserFrameBuffer;
		
		uint32_t yPixel = 0;
		
		uint8_t minValue(Constants::n255);
		
		while( 0 != clampHeight )
		{
			uint32_t wlen = clampWidth;
			
			UserFrameBuffer = pBufferL8 + (yPixel << oOLED::Width_SATBITS); //read-only
			
			while( 0 != wlen ) 
			{ 	
				minValue = min( minValue, *UserFrameBuffer);
				
				if (0 == minValue)
					return(minValue);
				
				++UserFrameBuffer;
				--wlen;
			} //end while
			
			++yPixel;
			--clampHeight;
		} // end while
		
		return(minValue);
	}
	uint8_t const findMax( uint8_t const* const __restrict pBufferL8 )
	{
		uint32_t const clampWidth = oOLED::Width;
		uint32_t clampHeight = oOLED::Height;
		
		uint8_t const* __restrict
					__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) UserFrameBuffer;
		
		uint32_t yPixel = 0;
		
		uint8_t maxValue(0);
		
		while( 0 != clampHeight )
		{
			uint32_t wlen = clampWidth;
			
			UserFrameBuffer = pBufferL8 + (yPixel << oOLED::Width_SATBITS); //read-only
			
			while( 0 != wlen ) 
			{ 	
				maxValue = max( maxValue, *UserFrameBuffer);
				
				if (Constants::n255 == maxValue)
					return(maxValue);
				
				++UserFrameBuffer;
				--wlen;
			} //end while
			
			++yPixel;
			--clampHeight;
		} // end while
		
		return(maxValue);
	}
	static uint32_t const finalize_erode( uint8_t* const __restrict pBufferL8 )
	{ // Second Loop thru
			
		uint32_t uiLitPixels(0);
		uint32_t const clampWidth = oOLED::Width;
		uint32_t clampHeight = oOLED::Height;
		
		uint8_t* __restrict
					__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) UserFrameBuffer;
			
		uint32_t yPixel = 0;
		
		while( 0 != clampHeight )
		{
			uint32_t wlen = clampWidth;
			
			UserFrameBuffer = pBufferL8 + (yPixel << oOLED::Width_SATBITS); //read-write
			
			while( 0 != wlen ) 
			{ 	
				if ( 0 != *UserFrameBuffer ) {
					// Only if pixel is flagged
					if ( MARK_CHANGE_TO_BLACK == *UserFrameBuffer ) { // make "new" totally black
						*UserFrameBuffer = 0;
					}
					else
						++uiLitPixels;
				}
				
				++UserFrameBuffer;
				--wlen;
			} //end while
			
			++yPixel;
			--clampHeight;
		} // end while
		
		return( uiLitPixels );
	}
	static void PokeHole( uint8_t* const __restrict pBufferL8, uint32_t uiRarity = 10 )
	{
		uint32_t const clampWidth = oOLED::Width;
		
		int32_t const iLeftToRight = ( PsuedoRandom5050() ? 0 : oOLED::Width - 1 );
		int32_t const iTopToBottom = ( PsuedoRandom5050() ? 0 : oOLED::Height - 1 );
	
		uint32_t pokePixel = findAverage(pBufferL8);
		
		do
		{
			int32_t yPixel = 0;
			uint32_t clampHeight = oOLED::Height;
			
			while( 0 != clampHeight )
			{
				uint32_t wlen = clampWidth;
				int32_t xPixel = 0;
				
				// Update hole coordinates based on previous selection
				int32_t const yPixelHole = absolute(iTopToBottom - yPixel);
				
				while( 0 != wlen ) 
				{ 	
					// Update hole coordinates based on previous selection
					int32_t const xPixelHole = absolute(iLeftToRight - xPixel);
					
					uint8_t const* const __restrict
					__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) UserFrameBuffer = pBufferL8 + (yPixelHole << oOLED::Width_SATBITS) + xPixelHole; //read-only
				
					if ( 0 != *UserFrameBuffer && *UserFrameBuffer <= pokePixel ) {
						if ( RandomNumber(0, 100) < uiRarity ) {
							
							// Itself 
							*(pBufferL8 + (((yPixelHole) << oOLED::Width_SATBITS) + (xPixelHole))) = 0;
							// + neighbours
							// Down
							if (yPixelHole > 0 && 0 != *(pBufferL8 + ((yPixelHole-1) << oOLED::Width_SATBITS))  )
								*(pBufferL8 + (((yPixelHole-1) << oOLED::Width_SATBITS))) = 0;
							
							// Left
							if (xPixelHole > 0 && 0 != *(pBufferL8 + (((yPixelHole) << oOLED::Width_SATBITS) + (xPixelHole-1)))  ) 
								*(pBufferL8 + (((yPixelHole) << oOLED::Width_SATBITS) + (xPixelHole-1))) = 0;
							
							// Up
							if (yPixelHole+1 < oOLED::Height && 0 != *(pBufferL8 + ((yPixelHole+1) << oOLED::Width_SATBITS))  )
								*(pBufferL8 + (((yPixelHole+1) << oOLED::Width_SATBITS))) = 0;
							
							// Right
							if (xPixelHole+1 < clampWidth && 0 != *(pBufferL8 + (((yPixelHole) << oOLED::Width_SATBITS) + (xPixelHole+1)))  ) 
								*(pBufferL8 + (((yPixelHole) << oOLED::Width_SATBITS) + (xPixelHole+1))) = 0;
							
							return; // Done poking hole
						}
						else
							break; // Skip Row on random fail
					}
					
					++xPixel;
					--wlen;
				} //end while
				
				++yPixel;
				--clampHeight;
			} // end while
			
			// switch to max
			pokePixel = findMax(pBufferL8);
			
		} while( ++uiRarity ); // no poke hole! do again - very rare
	}
	
	uint32_t const Erode_8bit( uint8_t* const __restrict pBufferL8 )
	{
		uint32_t const clampWidth = oOLED::Width;
		uint32_t clampHeight = oOLED::Height;
		
		// Poke a random hole
		PokeHole( pBufferL8 );
		
		{ // First Loop thru		
			uint32_t yPixel = 0;
			
			uint8_t const * __restrict
					__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) UserFrameBuffer;
			
			
			while( 0 != clampHeight )
			{
				uint32_t wlen = clampWidth;
				uint32_t xPixel = 0;
				
				UserFrameBuffer = pBufferL8 + (yPixel << oOLED::Width_SATBITS); //read-only
				
				while( 0 != wlen ) 
				{ 	
					if ( 0 == *UserFrameBuffer ) {
						// Bounds Check Neighbouring pixels (cross) and set "flag" to black if they are above the threshold for black
						
						// Down
						if (yPixel > 0 && 0 != *(pBufferL8 + ((yPixel-1) << oOLED::Width_SATBITS))  )
							*(pBufferL8 + (((yPixel-1) << oOLED::Width_SATBITS))) = MARK_CHANGE_TO_BLACK;
						
						// Left
						if (xPixel > 0 && 0 != *(pBufferL8 + (((yPixel) << oOLED::Width_SATBITS) + (xPixel-1)))  ) 
							*(pBufferL8 + (((yPixel) << oOLED::Width_SATBITS) + (xPixel-1))) = MARK_CHANGE_TO_BLACK;
						
						// Up
						if (yPixel+1 < oOLED::Height && 0 != *(pBufferL8 + ((yPixel+1) << oOLED::Width_SATBITS))  )
							*(pBufferL8 + (((yPixel+1) << oOLED::Width_SATBITS))) = MARK_CHANGE_TO_BLACK;
						
						// Right
						if (xPixel+1 < clampWidth && 0 != *(pBufferL8 + (((yPixel) << oOLED::Width_SATBITS) + (xPixel+1)))  ) 
							*(pBufferL8 + (((yPixel) << oOLED::Width_SATBITS) + (xPixel+1))) = MARK_CHANGE_TO_BLACK;
						
					}
					
					++UserFrameBuffer;
					++xPixel;
					--wlen;
				} // end while
				
				++yPixel;
				--clampHeight;
			} // end while
		} // end first loop thru
		
		// Loop thru again, seetting all new "flagged" pixels to black. This has to be done
		// in 2 separate passes. If one loop was used, we would successvely overwrite black to n neighbouing pixels
		
		return( finalize_erode( pBufferL8 ) );
	}
	
	static constexpr uint32_t const NUM_BOXES = 3;
	template<uint32_t const sigma>
	STATIC_INLINE void boxesForGauss(int32_t (& __restrict Sizes)[NUM_BOXES], float (& __restrict InverseSizes)[NUM_BOXES])
	{
		constexpr int32_t const wIdeal = ct_sqrt((12 * sigma * sigma / NUM_BOXES) + 1);
		
		constexpr int32_t const wl = (0 == (wIdeal % 2) ? wIdeal - 1 : wIdeal);
		
		constexpr int32_t const wu = wl + 2;

		constexpr int32_t const mIdeal = (12 * sigma * sigma - NUM_BOXES * wl * wl - 4 * NUM_BOXES * wl - 3 * NUM_BOXES) / (-4 * wl - 4);
		constexpr int32_t const m(mIdeal);

		// float const iar( 1.0f / ( (float)(r + r + 1) ) );
		for (int i = NUM_BOXES - 1; i >= 0; --i) {
			int32_t const box = ((i < m ? wl : wu) - 1) >> 1;
			Sizes[i] = box;
			InverseSizes[i] =  1.0f / ( (float)(box + box + 1) );
		}
	}
	
	static constexpr uint32_t ALPHAMASK = 0xFF000000;
	static constexpr uint32_t BLUEMASK  = 0x000000FF;

	#define ALPHA_DOWN(x) ((ALPHAMASK & (x)) >> 24)
	#define ALPHA_UP(x) (((x) << 24) & ALPHAMASK)
	#define BLUE_ONLY(x) (BLUEMASK & (x))
	
	template<typename T>
	STATIC_INLINE void boxBlurH_AL(T const* const __restrict source, T* const __restrict dest, int const r, float const iar)
	{
			#define w (oOLED::Width>>1)
			#define h (oOLED::Height>>1)
			
			for (int i = h - 1 ; i >= 0 ; --i)
			{
					int ti = i * w;
					int li = ti;
					int ri = ti + r;
					int fvA = ALPHA_DOWN(*(source + ti));
					int lvA = ALPHA_DOWN(*(source + (ti + w - 1)));
					int fvB = BLUE_ONLY(*(source + ti));
					int lvB = BLUE_ONLY(*(source + (ti + w - 1)));
					int valA = (r + 1) * fvA;
					int valB = (r + 1) * fvB;
					for (int j = r - 1; j >= 0; --j) {
						valA += ALPHA_DOWN(*(source + ti + j));
						valB += BLUE_ONLY(*(source + ti + j));
					}
					for (int j = 0; j <= r; ++j)
					{
						valA += ALPHA_DOWN(*(source + ri)) - fvA; 
						valB += BLUE_ONLY(*(source + ri)) - fvB; 
						++ri;
						uint32_t const blueReg = BLUE_ONLY(uint32::__roundf((float)valB * iar));
							*(dest + ti) = ALPHA_UP(uint32::__roundf((float)valA * iar)) | (blueReg << 16) | (blueReg << 8) | (blueReg);
						++ti;
					}
					for (int j = r + 1; j < w - r; ++j)
					{
							valA += ALPHA_DOWN(*(source + ri)) - ALPHA_DOWN(*(dest + li)); 
							valB += BLUE_ONLY(*(source + ri)) - BLUE_ONLY(*(dest + li)); 
							++ri; 
							++li;
							uint32_t const blueReg = BLUE_ONLY(uint32::__roundf((float)valB * iar));
							*(dest + ti) = ALPHA_UP(uint32::__roundf((float)valA * iar)) | (blueReg << 16) | (blueReg << 8) | (blueReg);
							++ti;
					}
					for (int j = w - r; j < w; ++j)
					{
							valA += lvA - ALPHA_DOWN(*(source + li)); 
							valB += lvB - BLUE_ONLY(*(source + li)); 
							++li;
							uint32_t const blueReg = BLUE_ONLY(uint32::__roundf((float)valB * iar));
							*(dest + ti) = ALPHA_UP(uint32::__roundf((float)valA * iar)) | (blueReg << 16) | (blueReg << 8) | (blueReg);
							++ti;
					}
			}
			#undef h 
			#undef w
	}

	template<typename T>
	STATIC_INLINE void boxBlurT_AL(T const* const __restrict source, T* const __restrict dest, int const r, float const iar)
	{
			#define w (oOLED::Width>>1)
			#define h (oOLED::Height>>1)
			
			for (int i = w - 1 ; i >= 0 ; --i)
			{
					int ti = i;
					int li = ti;
					int ri = ti + r * w;
					int fvA = ALPHA_DOWN(*(source + ti));
					int lvA = ALPHA_DOWN(*(source + (ti + w * (h - 1))));
					int fvB = BLUE_ONLY(*(source + ti));
					int lvB = BLUE_ONLY(*(source + (ti + w * (h - 1))));
					int valA = (r + 1) * fvA;
					int valB = (r + 1) * fvB;
					for (int j = r - 1; j >= 0; --j) {
						valA += ALPHA_DOWN(*(source + (ti + j * w)));
						valB += BLUE_ONLY(*(source + (ti + j * w)));
					}
					for (int j = 0; j <= r; ++j)
					{
							valA += ALPHA_DOWN(*(source + ri)) - fvA;
							valB += ALPHA_DOWN(*(source + ri)) - fvB;
							uint32_t const blueReg = BLUE_ONLY(uint32::__roundf((float)valB * iar));
							*(dest + ti) = ALPHA_UP(uint32::__roundf((float)valA * iar)) | (blueReg << 16) | (blueReg << 8) | (blueReg);
							ri += w;
							ti += w;
					}
					for (int j = r + 1; j < h - r; ++j)
					{
							valA += ALPHA_DOWN(*(source + ri)) - ALPHA_DOWN(*(source + li));
							valB += BLUE_ONLY(*(source + ri)) - BLUE_ONLY(*(source + li));
							uint32_t const blueReg = BLUE_ONLY(uint32::__roundf((float)valB * iar));
							*(dest + ti) = ALPHA_UP(uint32::__roundf((float)valA * iar)) | (blueReg << 16) | (blueReg << 8) | (blueReg);
							li += w;
							ri += w;
							ti += w;
					}
					for (int j = h - r; j < h; ++j)
					{
							valA += lvA - ALPHA_DOWN(*(source + li));
							valB += lvB - BLUE_ONLY(*(source + li));
							uint32_t const blueReg = BLUE_ONLY(uint32::__roundf((float)valB * iar));
							*(dest + ti) = ALPHA_UP(uint32::__roundf((float)valA * iar)) | (blueReg << 16) | (blueReg << 8) | (blueReg);
							li += w;
							ti += w;
					}
			}
			#undef h 
			#undef w
		}
	
	template<typename T>
	STATIC_INLINE void boxBlurH(T const* const __restrict source, T* const __restrict dest, int const r, float const iar)
	{
			#define w (oOLED::Width>>1)
			#define h (oOLED::Height>>1)
			
			for (int i = h - 1 ; i >= 0 ; --i)
			{
					int ti = i * w;
					int li = ti;
					int ri = ti + r;
					int fv = *(source + ti);
					int lv = *(source + (ti + w - 1));
					int val = (r + 1) * fv;
					for (int j = r - 1; j >= 0; --j) val += *(source + ti + j);
					for (int j = 0; j <= r; ++j)
					{
						val += *(source + ri) - fv; ++ri;
						*(dest + ti) = uint32::__roundf((float)val * iar);
						++ti;
					}
					for (int j = r + 1; j < w - r; ++j)
					{
							val += *(source + ri) - *(dest + li); ++ri; ++li;
							*(dest + ti) = uint32::__roundf((float)val * iar);
							++ti;
					}
					for (int j = w - r; j < w; ++j)
					{
							val += lv - *(source + li); ++li;
							*(dest + ti) = uint32::__roundf((float)val * iar);
							++ti;
					}
			}
			#undef h 
			#undef w
	}

	template<typename T>
	STATIC_INLINE void boxBlurT(T const* const __restrict source, T* const __restrict dest, int const r, float const iar)
	{
			#define w (oOLED::Width>>1)
			#define h (oOLED::Height>>1)
			
			for (int i = w - 1 ; i >= 0 ; --i)
			{
					int ti = i;
					int li = ti;
					int ri = ti + r * w;
					int fv = *(source + ti);
					int lv = *(source + (ti + w * (h - 1)));
					int val = (r + 1) * fv;
					for (int j = r - 1; j >= 0; --j) val += *(source + (ti + j * w));
					for (int j = 0; j <= r; ++j)
					{
							val += *(source + ri) - fv;
							*(dest + ti) = uint32::__roundf((float)val * iar);
							ri += w;
							ti += w;
					}
					for (int j = r + 1; j < h - r; ++j)
					{
							val += *(source + ri) - *(source + li);
							*(dest + ti) = uint32::__roundf((float)val * iar);
							li += w;
							ri += w;
							ti += w;
					}
					for (int j = h - r; j < h; ++j)
					{
							val += lv - *(source + li);
							*(dest + ti) = uint32::__roundf((float)val * iar);
							li += w;
							ti += w;
					}
			}
			#undef h 
			#undef w
		}

	template<class T>
	struct gaussBlurBits				/// #### So this is HOW todo function dispatch based on type at compile time! ##### ////
	{	
		// if "condition (true/false)" of equal type, enable tempolate instantion of function
		template< bool cond, typename U >
    using resolvedType  = typename std::enable_if< cond, U >::type; 
		
		template< typename U = T > 
		STATIC_INLINE void boxBlur_32bit(resolvedType< std::is_same<U, uint32_t>::value, U >* const __restrict source, 
																		 resolvedType< std::is_same<U, uint32_t>::value, U >* const __restrict dest, int const r, float const iar) {
																			 
			boxBlurH_AL<T>(dest, source, r, iar);
			boxBlurT_AL<T>(source, dest, r, iar);
		}
		template< typename U = T > 
		STATIC_INLINE void boxBlur_8bit(resolvedType< std::is_same<U, uint8_t>::value, U >* const __restrict source, 
																		resolvedType< std::is_same<U, uint8_t>::value, U >* const __restrict dest, int const r, float const iar) {
							
			boxBlurH<T>(dest, source, r, iar);
			boxBlurT<T>(source, dest, r, iar);
		}
	};
	template<uint32_t const radius>
	STATIC_INLINE void gaussBlur32bit(uint32_t* const __restrict dest, uint32_t* const __restrict source) // ONLY FUNC THAT IS DEST,SOURCE (ROOT FUNCTION)
	{
		RenderCopy32bitScreenLayerAsync(dest, source); // source must equal destination before first pass
		
		{
			float boxInverseRadii[NUM_BOXES];
			int32_t boxRadii[NUM_BOXES];
			
			boxesForGauss<radius>(boxRadii, boxInverseRadii);
		
			xDMA2D::Wait_DMA2D<false>();
			SCB_InvalidateDCache_by_Addr( (uint32_t*)dest, (oOLED::Width>>1)*(oOLED::Height>>1)*sizeof(uint32_t));
			
			gaussBlurBits<uint32_t>::boxBlur_32bit(source, dest, boxRadii[0], boxInverseRadii[0]);
			gaussBlurBits<uint32_t>::boxBlur_32bit(dest, source, boxRadii[1], boxInverseRadii[1]);
			gaussBlurBits<uint32_t>::boxBlur_32bit(source, dest, boxRadii[2], boxInverseRadii[2]);
		}
	}
	template<uint32_t const radius>
	STATIC_INLINE void gaussBlur8bit(uint8_t* const __restrict dest, uint8_t* const __restrict source) // ONLY FUNC THAT IS DEST,SOURCE (ROOT FUNCTION)
	{
		RenderCopy8bitScreenLayerAsync(dest, source); // source must equal destination before first pass
		
		{
			float boxInverseRadii[NUM_BOXES];
			int32_t boxRadii[NUM_BOXES];
			
			boxesForGauss<radius>(boxRadii, boxInverseRadii);
		
			xDMA2D::Wait_DMA2D<false>();
			SCB_InvalidateDCache_by_Addr( (uint32_t*)dest, oOLED::Width*oOLED::Height);
			
			gaussBlurBits<uint8_t>::boxBlur_8bit(source, dest, boxRadii[0], boxInverseRadii[0]);
			gaussBlurBits<uint8_t>::boxBlur_8bit(dest, source, boxRadii[1], boxInverseRadii[1]);
			gaussBlurBits<uint8_t>::boxBlur_8bit(source, dest, boxRadii[2], boxInverseRadii[2]);
		}
	}

	void GaussianBlur_32bit( uint32_t* const __restrict pDestBufferARGB, uint32_t* const __restrict pSourceBufferARGB, uint32_t const BlurRadius )
	{
		switch(BlurRadius)
		{
			default:
			case 1:
				gaussBlur32bit<1>(pDestBufferARGB, pSourceBufferARGB);
			break;
			case 2:
				gaussBlur32bit<2>(pDestBufferARGB, pSourceBufferARGB);
			break;
			case 3:
				gaussBlur32bit<3>(pDestBufferARGB, pSourceBufferARGB);
			break;
			case 4:
				gaussBlur32bit<4>(pDestBufferARGB, pSourceBufferARGB);
			break;
			case 5:
				gaussBlur32bit<5>(pDestBufferARGB, pSourceBufferARGB);
			break;
			case 6:
				gaussBlur32bit<6>(pDestBufferARGB, pSourceBufferARGB);
			break;
		}
	}
	void GaussianBlur_8bit( uint8_t* const __restrict pDestBufferL8, uint8_t* const __restrict pSourceBufferL8, uint32_t const BlurRadius )
	{
		switch(BlurRadius)
		{
			default:
			case 1:
				gaussBlur8bit<1>(pDestBufferL8, pSourceBufferL8);
			break;
			case 2:
				gaussBlur8bit<2>(pDestBufferL8, pSourceBufferL8);
			break;
			case 3:
				gaussBlur8bit<3>(pDestBufferL8, pSourceBufferL8);
			break;
			case 4:
				gaussBlur8bit<4>(pDestBufferL8, pSourceBufferL8);
			break;
			case 5:
				gaussBlur8bit<5>(pDestBufferL8, pSourceBufferL8);
			break;
			case 6:
				gaussBlur8bit<6>(pDestBufferL8, pSourceBufferL8);
			break;
		}
	}
	
}//end namespace

}//end namespace

