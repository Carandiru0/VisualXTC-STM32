/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef OLED_H
#define OLED_H
#include "PreprocessorCore.h"
#include "globals.h"
#include "DMA2D.hpp"
#include "stm32f7xx_ll_gpio.h"
#include "spi.h"
#include "font.h"
#include "Commonmath.h"
#include "math_3d.h"
#include "DTCM_Reserve.h"
#include "isodepth.h"
#include "shade_ops.h"


namespace DTCM
{
	extern uint8_t 	BackBuffer[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH]
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".dtcm.BackBuffer")));

	extern uint32_t	FrontBuffer[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH]
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".dtcm.FrontBuffer")));

	extern int8_t	DepthBuffer[OLED::SCREEN_WIDTH][OLED::SCREEN_HEIGHT] 	// 90 dsgree rotation in memory
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".dtcm.DepthBuffer")));

#define getBackBuffer() BackBuffer
#define getFrontBuffer() FrontBuffer
#define getDepthBuffer() DepthBuffer
}

extern uint32_t _DMA2DFrameBuffer[OLED::SCREEN_HEIGHT][OLED::SCREEN_WIDTH]
					__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
					__attribute__((section (".dma2dscratchbuffer")));

extern uint8_t * __restrict BloomHDRTargetFrameBuffer
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
__attribute__((section (".dtcm_atomic.bloomhdrtarget")));

namespace OLED
{
	static constexpr uint32_t NUMLEVELS4BPP = 16, NUMLEVELS8PP = 256;
	
	static constexpr uint32_t const
	
		g0 = 0,
		g1 = 0x11,
		g2 = 0x22,
		g3 = 0x33,
		g4 = 0x44,
		g5 = 0x55,
		g6 = 0x66,
		g7 = 0x77,
		g8 = 0x88,
		g9 = 0x99,
		g10 = 0xaa,
		g11 = 0xbb,
		g12 = 0xcc,
		g13 = 0xdd,
		g14 = 0xee,
		g15 = 0xff;
	
	static constexpr uint32_t const IndexLuma[NUMLEVELS4BPP] =
	{
		g0, g1, g2, g3, g4, g5, g6, g7, g8, g9, g10, g11, g12, g13, g14, g15
	};

	static constexpr uint32_t const BLOOMHDR_SATBITS = 7,
																	BLOOMHDR_MIN = Constants::n255 - (1 << BLOOMHDR_SATBITS), // must be a power of 2
																	BLOOMHDR_MIN_SHADE = 16; // optional, but allows for fine adjustment
																	
	/**SPI1 GPIO Configuration  
  PA5   ------> SPI1_SCK	--------- Purplish Red
  PA7   ------> SPI1_MOSI 	------- Gray
	PE14  ------> SPI1_OLED_RESET --- Yellow
	PE12  ------> SPI1_OLED_DC  ----- Blue
	PE15  ------> SPI1_OLED_CS ------ Green
  */
	static constexpr uint32_t const GPIO_OLED_RESET_Pin = LL_GPIO_PIN_14;
	static constexpr GPIO_TypeDef * const __restrict GPIO_OLED_RESET_GPIO_Port = __builtin_constant_p(GPIOE) ? GPIOE : GPIOE;
	static constexpr uint32_t const GPIO_OLED_DC_Pin = LL_GPIO_PIN_12;
	static constexpr GPIO_TypeDef * const __restrict  GPIO_OLED_DC_GPIO_Port = __builtin_constant_p(GPIOE) ? GPIOE : GPIOE;
	static constexpr uint32_t const GPIO_OLED_CS_Pin = LL_GPIO_PIN_15;
	static constexpr GPIO_TypeDef * const __restrict  GPIO_OLED_CS_GPIO_Port = __builtin_constant_p(GPIOE) ? GPIOE : GPIOE;

	static constexpr uint32_t const CURRENTTARGETMEM0 = 0,
																	CURRENTTARGETMEM1 = 1;
	
	static constexpr uint32_t const FRONT_BUFFER = 1,
																	BACK_BUFFER = 0;
	
	static constexpr uint32_t const SHADE_ENABLE = (1 << 0), // intended for complex shading enabling, or simple flag when not using zbuffer only
																	Z_ENABLE = (1 << 1),
																	ZWRITE_ENABLE = (1 << 2),
																	FOG_ENABLE = (1 << 3),
																	BLOOM_DISABLE = (1 << 4);
	
	typedef struct sRenderSync
	{
		static constexpr int32_t const UNLOADED = ::UNLOADED,
																	 PENDING = ::PENDING,
																	 LOADED = ::LOADED,
																	 MAIN_DMA2D_COMPLETED = -1;
		
		uint32_t 					tLastRenderCompleted,				// these need not be volatile as long as they are updated b4
											tLastSendCompleted;					// a change in state, state controls the flow of execution between the
																									// render interrupt and the main render loop
		
		atomic_strict_int32_t State;  // new.) I think freezing bug is fixed.
		//volatile int32_t State;  // atomic_strict_int32_t may be causing the freeze here only, volatile int32_t works fine
		
		sRenderSync()
		: tLastRenderCompleted(0), tLastSendCompleted(1), State(UNLOADED)
		{}
			
	} RenderSync;
	
	extern RenderSync* const __restrict bbRenderSync 
		__attribute__((section (".dtcm_const")));
	extern atomic_strict_uint32_t* const __restrict bbOLEDBusy_DMA2[2]
		__attribute__((section (".dtcm_const"))); // don't touch
	
NOINLINE void Init();
void DisplayOn();
void DisplayOff();

STATIC_INLINE __attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) uint32_t * const __restrict getScratchBuffer() { return((uint32_t * const __restrict)_DMA2DFrameBuffer); }
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) extern uint8_t * const __restrict getDeferredFrameBuffer();
__attribute__((pure)) __attribute__((assume_aligned(ARMV7M_DCACHE_LINESIZE))) extern uint8_t * const __restrict getEffectFrameBuffer_0();
	
 uint32_t const getActiveStreamDoubleBufferIndex();
uint32_t const getStringPixelLength(char const* const szString);

STATIC_INLINE void setDepthLevel( int32_t const NewLevel );

void setActiveFont( FontType const* const oFontType );
	
 void ClearFrameBuffers( uint32_t const tNow );
STATIC_INLINE void ClearBuffer_8bit(uint8_t* const __restrict pDstBufferL8) { xDMA2D::ClearBuffer_8bit(pDstBufferL8); }
STATIC_INLINE void FillBuffer_8bit(uint8_t* const __restrict pDstBufferL8, uint8_t const Luma) { xDMA2D::FillBuffer_8bit(pDstBufferL8, Luma); }
void FillBackBuffer(uint8_t const Luma);
void FillFrontBuffer(uint8_t const Alpha, uint8_t const Luma);

void Render(uint32_t const tNow);
void SendFrameBuffer(uint32_t const tNow);

void TestPixels(uint32_t const& tNow);
void RenderBloomHDRBuffer();
void RenderDepthBuffer();

void RenderCopy32bitScreenLayerAsync(uint32_t* const __restrict p32bitLayerDst, uint32_t const* const __restrict p32bitLayerSrc);
void RenderCopy8bitScreenLayerAsync(uint8_t* const __restrict p8bitLayerDst, uint8_t const* const __restrict p8bitLayerSrc);
void RenderCopy32bitScreenLayer(uint32_t* const __restrict p32bitLayerDst, uint32_t const* const __restrict p32bitLayerSrc);
void RenderCopy8bitScreenLayer(uint8_t* const __restrict p8bitLayerDst, uint8_t const* const __restrict p8bitLayerSrc);
void Render8bitScreenLayer_Back(uint8_t const* const __restrict p8bitLayerA);		// 8bit to 8bit
void Render32bitScreenLayer_Front(uint32_t const* const __restrict p32bitLayer0);  // 32bit to 32bit
void Render8bitScreenLayer_Front(uint8_t const* __restrict p8bitLayerA, uint32_t const Opacity = Constants::OPACITY_100); // 8bit to 32bit PFC

 void Render8bitScreenLayer_AlphaMask_Front(uint8_t const*  __restrict p8bitLayerA,uint8_t const*  __restrict p8bitAlphaMask);

 void Render8bitScreenLayer_Blended(uint8_t const* const __restrict p8bitLayerA, uint8_t* const __restrict p8bitOffScreenTarget, uint32_t const Opacity = Constants::OPACITY_100, 
																																			 uint32_t const ShadeRegister = 0xFF);

 void Render8bitScreenLayer_Lerp_Back(uint8_t const* __restrict p8bitLayerA, uint8_t const* __restrict p8bitLayerB, float const tDeltaNorm);
 void Render8bitScreenLayer_Lerp_Front(uint8_t const* __restrict p8bitLayerA, uint8_t const* __restrict p8bitLayerB, float const tDeltaNorm, uint32_t const Opacity = Constants::OPACITY_100);

 void Render1bitScreenLayer_Front(uint8_t const* const __restrict p1bitLayer, uint8_t const* const __restrict p1bitAlphaLayer);

	namespace Effects
	{
		typedef struct sScanlinerIntercept
		{
			bool bIntercepted;
			uint32_t uiYIntercept;
			
			void Reset(uint32_t const& yIntercept) {
				bIntercepted = false;
				uiYIntercept = yIntercept;
			}
			sScanlinerIntercept() 
			: bIntercepted(false), uiYIntercept(0)
			{}
			
		} ScanlinerIntercept;
		
		void AttachScanlineIntercept(Effects::ScanlinerIntercept* const pInterceptDesc);
		void DetachScanlineIntercept();
		
		uint8_t const findAverage( uint8_t const* const __restrict pBufferL8 );
		uint8_t const findMin( uint8_t const* const __restrict pBufferL8 );
		uint8_t const findMax( uint8_t const* const __restrict pBufferL8 );
		uint32_t const Erode_8bit( uint8_t* const __restrict pBufferL8 );
		
		void GaussianBlur_8bit( uint8_t* const __restrict pDestBufferL8, uint8_t* const __restrict pSourceBufferL8, uint32_t const BlurRadius );
		void GaussianBlur_32bit( uint32_t* const __restrict pDestBufferARGB, uint32_t* const __restrict pSourceBufferARGB, uint32_t const BlurRadius );
		
	}//endnamespace
	
	namespace Texture {

		enum eTextureMode {
			WRAP = 0,
			CLAMP
		};
		
		template<uint32_t const SATBIT>
		STATIC_INLINE constexpr float const textureSample(uint32_t const * const __restrict texture, int16_t const x, int16_t const y) { return(((float const)*(texture + (y << SATBIT) + x))); }
		template<uint32_t const SATBIT>
		STATIC_INLINE constexpr float const textureSample(uint8_t const * const __restrict texture, int16_t const x, int16_t const y) { return(((float const)*(texture + (y << SATBIT) + x))); }

		// Bilinear Sampling, where texture width = height (units must be power of 2)
		template<typename T, uint32_t const SATBIT, eTextureMode const CLAMP_OR_WRAP = CLAMP, typename U = T>
		STATIC_INLINE T const textureSampleBilinear_1D(U const * const __restrict texture, vec2_t uv)
		{
			constexpr float dimension = (1 << SATBIT);
			
			if constexpr(!CLAMP_OR_WRAP) { // wrapping 
				uv = v2_fract(uv);
			}
			uv.x = __fma(uv.x, dimension, Constants::nfNegativePoint5);
			uv.y = __fma(uv.y, dimension, Constants::nfNegativePoint5);

			Pixels lb(uv.x, uv.y);

			Pixels rt(1, 1);

			rt.v = __QADD16(lb.v, rt.v);

			lb.Saturate<SATBIT>(); rt.Saturate<SATBIT>();

			Vector4 const vSamples( textureSample<SATBIT>(texture, lb.pixels.p0, lb.pixels.p1), textureSample<SATBIT>(texture, rt.pixels.p0, lb.pixels.p1),
											        textureSample<SATBIT>(texture, lb.pixels.p0, rt.pixels.p1), textureSample<SATBIT>(texture, rt.pixels.p0, rt.pixels.p1)
											      );

			float const lr = uv.x - lb.pixels.p0;

			if constexpr(std::is_same<T, float>::value) {
				return( mix(mix(vSamples.pt.x, vSamples.pt.y, lr),
										mix(vSamples.pt.z, vSamples.pt.w, lr), uv.y - lb.pixels.p1) * Constants::inverseUINT8);
			}
			else {
				return( __USAT( int32::__roundf( mix(mix(vSamples.pt.x, vSamples.pt.y, lr),
					                               mix(vSamples.pt.z, vSamples.pt.w, lr), uv.y - lb.pixels.p1)), Constants::SATBIT_256) );
			}
		}
		
		// Bilinear Sampling, where texture width != height (still units must be power of 2) but can be different
		template<typename T, uint32_t const WIDTH_SATBIT, uint32_t const HEIGHT_SATBIT, eTextureMode const CLAMP_OR_WRAP = CLAMP, typename U = T>
		STATIC_INLINE T const textureSampleBilinear_2D(U const * const __restrict texture, vec2_t uv)
		{
			constexpr float dimensionX = (1 << WIDTH_SATBIT),
										  dimensionY = (1 << HEIGHT_SATBIT);
			
			if constexpr(!CLAMP_OR_WRAP) { // wrapping 
				uv = v2_fract(uv);
			}
			uv.x = __fma(uv.x, dimensionX, Constants::nfNegativePoint5);
			uv.y = __fma(uv.y, dimensionY, Constants::nfNegativePoint5);

			Pixels lb(uv.x, uv.y);

			Pixels rt(1, 1);

			rt.v = __QADD16(lb.v, rt.v);

			lb.Saturate<WIDTH_SATBIT>(); rt.Saturate<WIDTH_SATBIT>(); // only need to saturate width

			Vector4 const vSamples ( textureSample<WIDTH_SATBIT>(texture, lb.pixels.p0, lb.pixels.p1), textureSample<WIDTH_SATBIT>(texture, rt.pixels.p0, lb.pixels.p1),
											         textureSample<WIDTH_SATBIT>(texture, lb.pixels.p0, rt.pixels.p1), textureSample<WIDTH_SATBIT>(texture, rt.pixels.p0, rt.pixels.p1)
											       );

			float const lr = uv.x - lb.pixels.p0;

			if constexpr(std::is_same<T, float>::value) {
				return( mix(mix(vSamples.pt.x, vSamples.pt.y, lr),
										mix(vSamples.pt.z, vSamples.pt.w, lr), uv.y - lb.pixels.p1) * Constants::inverseUINT8);
			}
			else {
				return( __USAT( int32::__roundf( mix(mix(vSamples.pt.x, vSamples.pt.y, lr),
					                                   mix(vSamples.pt.z, vSamples.pt.w, lr), uv.y - lb.pixels.p1)), Constants::SATBIT_256) );
			}
		}
	} // endnamespace
	
	/* ############### Inline Function ####################### */
	template<typename T, uint32_t const WIDTH_SATBIT, uint32_t const HEIGHT_SATBIT> 
	// source layer satbits (defines dimension, dimensions must be power of 2) but can be different
	STATIC_INLINE void RenderScreenLayer_Upscale(T* const __restrict pTbitLayerDst, T const* const __restrict pTbitLayerSrc)
	{
		int32_t yPixel(OLED::SCREEN_HEIGHT-1); // inverted y-axis

		vec2_t uv;
		while (yPixel >= 0)
		{
			int32_t xPixel(OLED::SCREEN_WIDTH-1);

			T * __restrict UserFrameBuffer = pTbitLayerDst + (yPixel << OLED::Width_SATBITS);

			uv.y = lerp(0.0f, 1.0f, static_cast<float>(yPixel) * OLED::INV_SCREEN_HEIGHT_MINUS1);

			while (xPixel >= 0)
			{
				uv.x = lerp(0.0f, 1.0f, static_cast<float>(xPixel) * OLED::INV_SCREEN_WIDTH_MINUS1);

				*UserFrameBuffer = Texture::textureSampleBilinear_2D<T, WIDTH_SATBIT, HEIGHT_SATBIT, Texture::WRAP>(pTbitLayerSrc, uv);
				++UserFrameBuffer;
				
				--xPixel;
			}
			
			--yPixel;
		}
	}		
		
	STATIC_INLINE void setDepthLevel( int32_t const NewLevel )
	{		
		IsoDepth::NewDepthLevelSet(NewLevel);
	}
	STATIC_INLINE void setDepthLevel_ToFarthest( int32_t const DepthModifier = 0 ) // takes a signed number to increase / decrease depth to be set
	{		
		IsoDepth::NewDepthLevelSet(IsoDepth::getDynamicRangeMin() + DepthModifier ); // positive being closer, negative farther
	}
	STATIC_INLINE void setDepthLevel_ToClosest( int32_t const DepthModifier = 0 ) // takes a signed number to increase / decrease depth to be set
	{		
		IsoDepth::NewDepthLevelSet(IsoDepth::getDynamicRangeMax() + DepthModifier ); // positive being closer, negative farther
	}
	
	STATIC_INLINE_PURE bool const TestStrict_Not_OnScreen(point2D_t pixel) 
	{
		// zero based tests (performance)
		
		// Test case, outside bounds left | top

		// If either component is negative, the point was greater than screen bounds
		if ( (pixel.pt.x | pixel.pt.y) >= 0 ) { //test the sign bit if it exists
			
			// Continue and verify not greater than screen bounds
			
			// Test case, outside bounds right | bottom
			pixel = p2D_sub( point2D_t(OLED::SCREEN_WIDTH, OLED::SCREEN_HEIGHT), pixel );
			
			// If either component is negative, the point was greater than screen bounds
			//test the sign bit if it exists
			return((pixel.pt.x | pixel.pt.y) < 0); // returning zero for successfully tested, point is onscreen
		}
			
		return(true); 
	}
	
	STATIC_INLINE_PURE bool const TestStrict_Not_OnScreen(point2D_t const pixel, int32_t const radiusSquared) 
	{
		point2D_t closestAxis;
		
		//Find closest x offset
    if( pixel.pt.x < 0 )		// point left of screen?
    {
        closestAxis.pt.x = 0;
    }
    else if( pixel.pt.x > OLED::SCREEN_WIDTH )		// point right of screen
    {
         closestAxis.pt.x = OLED::SCREEN_WIDTH;
    }
    else // point inside screen
    {
         closestAxis.pt.x = pixel.pt.x;
    }
		
		//Find closest y offset
    if( pixel.pt.y < 0 )		// point above screen?
    {
         closestAxis.pt.y = 0;
    }
    else if( pixel.pt.y > OLED::SCREEN_HEIGHT )		// point below screen
    {
         closestAxis.pt.y = OLED::SCREEN_HEIGHT;
    }
    else // point inside screen
    {
         closestAxis.pt.y = pixel.pt.y;
    }
		
		// Test closest axis to point, if distance is less than the radius squared there is visibility
		if ( p2D_distanceSquared(pixel,  closestAxis) < radiusSquared )
			return(false);  // visibile
		
		return(true);  // not visibile
	}
	
	// image[y][x] // assuming this is the original orientation 
	// image[x][original_width - y] // rotated 90 degrees ccw
	// image[original_height - x][y] // 90 degrees cw 
	// image[original_height - y][original_width - x] // 180 degrees
	// p_myImage2[offset].pixel = p_myImage[width * (height - 1 - y) + x].pixel; rotated 90 clockwise
	/*
	static constexpr uint32_t const FRONT_BUFFER = 1,
																	BACK_BUFFER = 0;
	
	static constexpr uint32_t const SHADE_ENABLE = (1 << 0),
																	Z_ENABLE = (1 << 1),
																	ZWRITE_ENABLE = (1 << 2);
	*/
	// main rendered buffer
	// Previous Frames Pixel Value
	template<uint32_t const TargetBuffer = (OLED::BACK_BUFFER)>	/// defaults to composite front+back (last frame)
	__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE uint32_t const GetPixel( uint32_t const x, uint32_t const y )
	{
		if constexpr ( OLED::BACK_BUFFER == TargetBuffer ) // static compile time eval
			return( ((uint8_t)(DTCM::getBackBuffer()[y][x])) );		// must be casted to uint8_t otherwise is accessed in "word" sized chunks
		else if constexpr ( OLED::FRONT_BUFFER == TargetBuffer )
			return( DTCM::getFrontBuffer()[y][x] );
	}
	/* main rendered buffer
	template<uint32_t const TargetBuffer = OLED::FRONT_BUFFER>	/// defaults to composite front+back (last frame)
	__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE float32_t const GetPixelAsFloat( uint32_t const x, uint32_t const y )
	{
		if ( OLED::BACK_BUFFER == TargetBuffer ) // static compile time eval
			return( (float32_t const)(*(DTCM::getBackBuffer() + (y << OLED::Width_SATBITS) + x )) * Constants::inverseUINT8 );
		else
			return( (float32_t const)(0xFF & *(oOLED_Public.DMA2DFrameBuffer + ((y << OLED::Width_SATBITS) + x))) * Constants::inverseUINT8 );
	}
	// arbritray buffers
	__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE float32_t const GetPixelAsFloat( uint32_t const x, uint32_t const y, uint8_t const* const __restrict pFrameBuffer )
	{
		return( (float32_t const)(0xFF & *(pFrameBuffer + ((y << OLED::Width_SATBITS) + x))) * Constants::inverseUINT8 );
	}*/
	template<uint32_t const TargetBuffer = OLED::BACK_BUFFER>
	__attribute__((always_inline)) STATIC_INLINE void LL_DrawBloomHDRPixel( uint32_t const x, uint32_t const y, xDMA2D::AlphaLuma const AlphaLuma )
	{
		int32_t HDRLuma( AlphaLuma.pixel.Luma - BLOOMHDR_MIN );
		//        (b-a)(x - min)
		// f(x) = --------------  + a
    //          max - min
		//          (255-BLOOMHDR_MIN_SHADE)(x - BLOOMHDR_MIN)
		// f(x) =       --------------                          + BLOOMHDR_MIN_SHADE
    //          UINT8_MAX - BLOOMHDR_MIN
		// see BLOOMHDR_SATBITS and resulting BLOOMHDR_MIN to avoid divide
		// limits ranges to 8, 16, 32 ... 128 etc however this is acceptable for choosing the minimum
		// and the width of the range
		
		if ( HDRLuma > 0 ) {

			HDRLuma = (((UINT8_MAX-BLOOMHDR_MIN_SHADE) * HDRLuma) >> BLOOMHDR_SATBITS) + BLOOMHDR_MIN_SHADE;
			
			if constexpr ( OLED::BACK_BUFFER == TargetBuffer ) { 
				
					*(BloomHDRTargetFrameBuffer + ((y << OLED::Width_SATBITS) + x)) = HDRLuma;
			}
			else {

					*(BloomHDRTargetFrameBuffer + ((y << OLED::Width_SATBITS) + x)) = ((AlphaLuma.pixel.Alpha * HDRLuma) >> 8);
			}
		}
	}
	 
	// back or front buffers
	// image[y][x] // assuming this is the original orientation (y * DIS.Width + x)
	// image[x][original_width - y] // rotated 90 degrees ccw
	// image[original_height - x][y] // 90 degrees cw **** linear memory incremental addressing, more cache "hits" for vertical lines
	// image[original_height - y][original_width - x] // 180 degrees
	#define DrawPixel_DepthTestResult LL_DrawPixel
	#define DrawPixel (void)LL_DrawPixel
	template<uint32_t const TargetBuffer = OLED::BACK_BUFFER, uint32_t const RenderingFlags = (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE)>
	 __attribute__((always_inline)) STATIC_INLINE bool const LL_DrawPixel( uint32_t const x, uint32_t const y, xDMA2D::AlphaLuma const AlphaLuma )
	{
		if constexpr (Z_ENABLE == (Z_ENABLE & RenderingFlags)) // statically evaluated addition of code @ compile time
		{
			int8_t* const __restrict pDepthBuffer
				__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))(&DTCM::getDepthBuffer()[x][y]);
			
			int32_t const CurDepth(*pDepthBuffer),
										CurSelectedDepth(IsoDepth::getCurSelectedDepth());
			
			// Test currently set depth level against coord into depth buffer
			if ( CurSelectedDepth < CurDepth ) {			// if depth is less than. pixel is occluded, if depth is greater than pixel is infront (reversed logic)
				// reject pixel
				return(IsoDepth::DEPTH_TEST_FAIL);
			}
			else if constexpr ( ZWRITE_ENABLE == (ZWRITE_ENABLE & RenderingFlags) ) {
				// update stored depth value to "greater" depth currently selected
				*pDepthBuffer = CurSelectedDepth;
				// fallthrough to below and draw pixel to appropriate buffer
			}
		}
		
		if constexpr ( SHADE_ENABLE == (SHADE_ENABLE & RenderingFlags) )
		{
			// Original non - antialiased //
			if constexpr ( OLED::BACK_BUFFER == TargetBuffer )  // static compile time eval
				DTCM::getBackBuffer()[y][x] = AlphaLuma.pixel.Luma;
			else
				DTCM::getFrontBuffer()[y][x] = AlphaLuma.v;

			if constexpr ( BLOOM_DISABLE != (BLOOM_DISABLE & RenderingFlags) )
				LL_DrawBloomHDRPixel<TargetBuffer>(x, y, AlphaLuma);
		}
		
		if constexpr ( FOG_ENABLE == (FOG_ENABLE & RenderingFlags) )
		{
			uint32_t const FogShade = IsoDepth::getActiveFogShade();
			
			if ( 0 != FogShade ) {
				DTCM::getFrontBuffer()[y][x] = FogShade;		// Fog will overwrite frontbuffer, FOG_ENABLE Rendering flag
			}
		}
		return(IsoDepth::DEPTH_TEST_PASS);
	}
	
	template<uint32_t const RenderingFlags = (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE)>
	__attribute__((always_inline)) STATIC_INLINE void DrawPixelAA_BackBuffer( uint32_t const x, uint32_t const y, xDMA2D::AlphaLuma const Luma )
	{
		// early reject / exit if current pixel is same shade -- speedup
		if (GetPixel<OLED::BACK_BUFFER>( x, y ) == Luma.pixel.Luma)
			return;
		
		// early reject all antialiasing if current main pixel does not pass
		if constexpr ( Z_ENABLE == (Z_ENABLE & RenderingFlags) ) // statically evaluated 
		{
			if ( IsoDepth::DEPTH_TEST_FAIL == DrawPixel_DepthTestResult<OLED::BACK_BUFFER, SHADE_ENABLE | RenderingFlags>( x, y, Luma ))
				return;
		}
		else
		{
			// just draw
			DrawPixel<OLED::BACK_BUFFER, SHADE_ENABLE | RenderingFlags>( x, y, Luma );
		}
		
		PixelsQuad const pixLevel( Luma.pixel.Luma );
		
		Pixels oLeftRight( ((int32_t)x) - 1, x + 1 ),
					 oTopBottom( ((int32_t)y) - 1, y + 1 );
			oLeftRight.Saturate<Constants::SATBIT_256>();
			oTopBottom.Saturate<Constants::SATBIT_64>();
			
		{
		PixelsQuad pixLRTB( GetPixel<OLED::BACK_BUFFER>( oLeftRight.pixels.p0, y ), GetPixel<OLED::BACK_BUFFER>( oLeftRight.pixels.p1, y ),
												GetPixel<OLED::BACK_BUFFER>( x, oTopBottom.pixels.p0 ), GetPixel<OLED::BACK_BUFFER>( x, oTopBottom.pixels.p1 ) );
		// average result
		pixLRTB.v = __UHADD8(pixLRTB.v, pixLevel.v);
		
		// must always enable Z for "additional" pixels drawn for AA
		DrawPixel<OLED::BACK_BUFFER,RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)>( oLeftRight.pixels.p0, y, pixLRTB.pixels.p0 );
		DrawPixel<OLED::BACK_BUFFER,RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)>( oLeftRight.pixels.p1, y, pixLRTB.pixels.p1 );
		DrawPixel<OLED::BACK_BUFFER,RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)>( x, oTopBottom.pixels.p0, pixLRTB.pixels.p2 );
		DrawPixel<OLED::BACK_BUFFER,RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)>( x, oTopBottom.pixels.p1, pixLRTB.pixels.p3 );
		}
		
		{
		PixelsQuad pixTLTRBLBR( GetPixel<OLED::BACK_BUFFER>( oLeftRight.pixels.p0, oTopBottom.pixels.p0 ), GetPixel<OLED::BACK_BUFFER>( oLeftRight.pixels.p1, oTopBottom.pixels.p0 ),
												    GetPixel<OLED::BACK_BUFFER>( oLeftRight.pixels.p0, oTopBottom.pixels.p1 ), GetPixel<OLED::BACK_BUFFER>( oLeftRight.pixels.p1, oTopBottom.pixels.p1 ) );
		
		pixTLTRBLBR.v = __UHADD8(pixTLTRBLBR.v, pixLevel.v);
			
		// must always enable Z for "additional" pixels drawn for AA
		DrawPixel<OLED::BACK_BUFFER,RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)>( oLeftRight.pixels.p0, oTopBottom.pixels.p0, pixTLTRBLBR.pixels.p0 );
		DrawPixel<OLED::BACK_BUFFER,RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)>( oLeftRight.pixels.p1, oTopBottom.pixels.p0, pixTLTRBLBR.pixels.p1 );
		DrawPixel<OLED::BACK_BUFFER,RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)>( oLeftRight.pixels.p0, oTopBottom.pixels.p1, pixTLTRBLBR.pixels.p2 );
		DrawPixel<OLED::BACK_BUFFER,RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)>( oLeftRight.pixels.p1, oTopBottom.pixels.p1, pixTLTRBLBR.pixels.p3 );
		}
	}
	
	template<uint32_t const RenderingFlags = (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE)>
	__attribute__((always_inline)) STATIC_INLINE void DrawPixelAA_FrontBuffer( uint32_t const x, uint32_t const y, xDMA2D::AlphaLuma const AlphaLuma )
	{
		// early reject / exit if current pixel is same shade -- speedup
		if (GetPixel<OLED::FRONT_BUFFER>( x, y ) == AlphaLuma.v)
			return;
		
		// early reject all antialiasing if current main pixel does not pass
		if constexpr ( Z_ENABLE == (Z_ENABLE & RenderingFlags) ) // statically evaluated 
		{
			if ( IsoDepth::DEPTH_TEST_FAIL == DrawPixel_DepthTestResult<OLED::FRONT_BUFFER, ((SHADE_ENABLE | RenderingFlags) & ~FOG_ENABLE)>( x, y, AlphaLuma ))
				return;
		}
		else
		{
			// just draw
			DrawPixel<OLED::FRONT_BUFFER, ((SHADE_ENABLE | RenderingFlags) & ~FOG_ENABLE)>( x, y, AlphaLuma );
		}
		PixelsQuad const pixLevelAlpha(AlphaLuma.pixel.Alpha), // isolate alpha and replicate 4 times
										 pixLevelLuma(AlphaLuma.pixel.Luma);   //    ""   luma   ""   ""      ""  ""
		
		Pixels oLeftRight( ((int32_t)x) - 1, x + 1 ),
					 oTopBottom( ((int32_t)y) - 1, y + 1 );
			oLeftRight.Saturate<Constants::SATBIT_256>();
			oTopBottom.Saturate<Constants::SATBIT_64>();
			
		{
			PixelsQuad pixLRTB_Alpha, pixLRTB_Luma;
			
			{
				xDMA2D::AlphaLuma const Left(GetPixel<OLED::FRONT_BUFFER>( oLeftRight.pixels.p0, y )),
																Right(GetPixel<OLED::FRONT_BUFFER>( oLeftRight.pixels.p1, y )),
																Top(GetPixel<OLED::FRONT_BUFFER>( x, oTopBottom.pixels.p0 )),
																Bottom(GetPixel<OLED::FRONT_BUFFER>( x, oTopBottom.pixels.p1 ) );
			
				pixLRTB_Alpha.Set( Left.pixel.Alpha, Right.pixel.Alpha,
													 Top.pixel.Alpha, Bottom.pixel.Alpha );
				// average result
				pixLRTB_Alpha.v = __UHADD8(pixLRTB_Alpha.v, pixLevelAlpha.v);
				
				pixLRTB_Luma.Set( Left.pixel.Luma, Right.pixel.Luma,
													Top.pixel.Luma, Bottom.pixel.Luma );
				// average result
				pixLRTB_Luma.v = __UHADD8(pixLRTB_Luma.v, pixLevelLuma.v);
			}
			// must always enable Z for "additional" pixels drawn for AA
			DrawPixel<OLED::FRONT_BUFFER,((RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)) & ~FOG_ENABLE)>( oLeftRight.pixels.p0, y, xDMA2D::AlphaLuma(pixLRTB_Alpha.pixels.p0, pixLRTB_Luma.pixels.p0).v );
			DrawPixel<OLED::FRONT_BUFFER,((RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)) & ~FOG_ENABLE)>( oLeftRight.pixels.p1, y, xDMA2D::AlphaLuma(pixLRTB_Alpha.pixels.p1, pixLRTB_Luma.pixels.p1).v );
			DrawPixel<OLED::FRONT_BUFFER,((RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)) & ~FOG_ENABLE)>( x, oTopBottom.pixels.p0, xDMA2D::AlphaLuma(pixLRTB_Alpha.pixels.p2, pixLRTB_Luma.pixels.p2).v );
			DrawPixel<OLED::FRONT_BUFFER,((RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)) & ~FOG_ENABLE)>( x, oTopBottom.pixels.p1, xDMA2D::AlphaLuma(pixLRTB_Alpha.pixels.p3, pixLRTB_Luma.pixels.p3).v );
		}	
		
		{
			PixelsQuad pixTLTRBLBR_Alpha, pixTLTRBLBR_Luma;
			
			{
				xDMA2D::AlphaLuma const LeftTop(GetPixel<OLED::FRONT_BUFFER>( oLeftRight.pixels.p0, oTopBottom.pixels.p0 )),
																RightTop(GetPixel<OLED::FRONT_BUFFER>( oLeftRight.pixels.p1, oTopBottom.pixels.p0 )),
																LeftBottom(GetPixel<OLED::FRONT_BUFFER>( oLeftRight.pixels.p0, oTopBottom.pixels.p1 )),
																RightBottom(GetPixel<OLED::FRONT_BUFFER>( oLeftRight.pixels.p1, oTopBottom.pixels.p1 ));
			
				pixTLTRBLBR_Alpha.Set( LeftTop.pixel.Alpha, RightTop.pixel.Alpha,
															 LeftBottom.pixel.Alpha, RightBottom.pixel.Alpha );
		
				// average result
				pixTLTRBLBR_Alpha.v = __UHADD8(pixTLTRBLBR_Alpha.v, pixLevelAlpha.v);
				
				pixTLTRBLBR_Luma.Set( LeftTop.pixel.Luma, RightTop.pixel.Luma,
															LeftBottom.pixel.Luma, RightBottom.pixel.Luma );
		
				// average result
				pixTLTRBLBR_Luma.v = __UHADD8(pixTLTRBLBR_Luma.v, pixLevelLuma.v);
			}
			
			// must always enable Z for "additional" pixels drawn for AA
			DrawPixel<OLED::FRONT_BUFFER,((RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)) & ~FOG_ENABLE)>( oLeftRight.pixels.p0, oTopBottom.pixels.p0, xDMA2D::AlphaLuma(pixTLTRBLBR_Alpha.pixels.p0, pixTLTRBLBR_Luma.pixels.p0).v );
			DrawPixel<OLED::FRONT_BUFFER,((RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)) & ~FOG_ENABLE)>( oLeftRight.pixels.p1, oTopBottom.pixels.p0, xDMA2D::AlphaLuma(pixTLTRBLBR_Alpha.pixels.p1, pixTLTRBLBR_Luma.pixels.p1).v );
			DrawPixel<OLED::FRONT_BUFFER,((RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)) & ~FOG_ENABLE)>( oLeftRight.pixels.p0, oTopBottom.pixels.p1, xDMA2D::AlphaLuma(pixTLTRBLBR_Alpha.pixels.p2, pixTLTRBLBR_Luma.pixels.p2).v );
			DrawPixel<OLED::FRONT_BUFFER,((RenderingFlags | (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE|BLOOM_DISABLE)) & ~FOG_ENABLE)>( oLeftRight.pixels.p1, oTopBottom.pixels.p1, xDMA2D::AlphaLuma(pixTLTRBLBR_Alpha.pixels.p3, pixTLTRBLBR_Luma.pixels.p3).v );
		}
	}
	
	template<uint32_t const TargetBuffer = OLED::BACK_BUFFER, uint32_t const RenderingFlags = (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE)>
	 __attribute__((always_inline)) STATIC_INLINE void DrawPixelAA( uint32_t const x, uint32_t const y, xDMA2D::AlphaLuma const AlphaLuma )
	{
		if constexpr ( OLED::BACK_BUFFER == TargetBuffer ) // Statically evaluated @ compile time
			DrawPixelAA_BackBuffer<RenderingFlags>(x, y, AlphaLuma); // AA Shade Only
		else
			DrawPixelAA_FrontBuffer<RenderingFlags>(x, y, AlphaLuma); // AA Shade + Transparency
	}
		
	template<uint32_t const TargetBuffer = OLED::BACK_BUFFER, uint32_t const RenderingFlags = (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE)>
	__attribute__((always_inline)) STATIC_INLINE void DrawPixel_Clipped( int32_t const x, int32_t const y, xDMA2D::AlphaLuma const AlphaLuma )
	{
		DrawPixel<TargetBuffer, RenderingFlags>( __USAT(x, OLED::Width_SATBITS), __USAT(y, OLED::Height_SATBITS), AlphaLuma );
	}

	//** Tried DMA2D for line functions, doesn't work properly for packing into 8bit output format for single pixel width lines
	
	__attribute__((always_inline)) STATIC_INLINE_PURE bool const CheckHLine( int32_t const x, int32_t const y, uint32_t const Width ) // DONE TO PREVENT UNECCESSARY EXPENSIVE FUNCTION CALLS
	{
		return( 0 != Width && (x & y) >= 0 && y < OLED::SCREEN_HEIGHT && x < OLED::SCREEN_WIDTH );
	}
	template<uint32_t const TargetBuffer = OLED::BACK_BUFFER, uint32_t const RenderingFlags = (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE)>
	__ramfunc void DrawHLine( uint32_t x, uint32_t const y, uint32_t const Width, uint32_t const AlphaLuma )
	{
		// x will always be at least 1 pixel
		// clamp x width
		uint32_t RunWidth = min(Width, OLED::SCREEN_WIDTH - x);
		
		// only first and last pixekl need anti-aliasing (edges)
		DrawPixelAA<TargetBuffer, RenderingFlags>( x, y, AlphaLuma ); // first

		if ( 0 != --RunWidth )	// Single pixel case, don't want to draw second AA pixel
		{
			++x;
			
			while ( 0 != --RunWidth )
			{
				DrawPixel<TargetBuffer, RenderingFlags>( x, y, AlphaLuma );
				++x;
			}
			
			DrawPixelAA<TargetBuffer, RenderingFlags>( x, y, AlphaLuma ); // last
		}
	}
	
	__attribute__((always_inline)) STATIC_INLINE_PURE bool const CheckVLine_XAxis( int32_t const x ) // DONE TO PREVENT UNECCESSARY EXPENSIVE FUNCTION CALLS
	{
		return( x >= 0 && x < OLED::SCREEN_WIDTH );
	}
	__attribute__((always_inline)) STATIC_INLINE_PURE bool const CheckVLine_YAxis( int32_t const y, int32_t const Height ) // DONE TO PREVENT UNECCESSARY EXPENSIVE FUNCTION CALLS
	{
		return( ((Height | (y + Height)) > 0) && y < OLED::SCREEN_HEIGHT );
	}
	__attribute__((always_inline)) STATIC_INLINE_PURE bool const CheckVLine( int32_t const x, int32_t const y, int32_t const Height ) // DONE TO PREVENT UNECCESSARY EXPENSIVE FUNCTION CALLS
	{
		return( CheckVLine_XAxis(x) & CheckVLine_YAxis(y, Height) );
	}
	
	
	
	template<uint32_t const TargetBuffer = OLED::BACK_BUFFER, uint32_t const RenderingFlags = (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE)>
	__ramfunc inline void DrawVLine( uint32_t const x, uint32_t y, uint32_t Height, xDMA2D::AlphaLuma const AlphaLuma )
	{
		Height = min(Height, OLED::SCREEN_HEIGHT - y);	// OLED::SCREEN_HEIGHT - y will never be zero if checked
		
		// only first and last pixekl need anti-aliasing (edges)
		DrawPixelAA<TargetBuffer, RenderingFlags>( x, y, AlphaLuma ); // first
		
		if ( 0 != --Height )	// Single pixel case, don't want to draw second AA pixel
		{
			++y;
			
			while ( 0 != --Height )
			{
				DrawPixel<TargetBuffer, RenderingFlags>( x, y, AlphaLuma );
				++y;
			}
			
			DrawPixelAA<TargetBuffer, RenderingFlags>( x, y, AlphaLuma ); // last
		}
	}
	
	typedef struct sMaskDesc
	{
		uint8_t const* __restrict Mask __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)));
		uint32_t Stride;							// Width of a line in Mask	
		point2D_t const Origin;				// Original Origin (to be removed)
		point2D_t const MaskOrigin;		// New Origin to replace original
		uint32_t CurOffset;						// Height Offset
		
	} MaskDesc;

	__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE uint32_t const getMaskPixel(uint32_t const x, uint32_t const y, MaskDesc const& descMask)
	{
		// must be casted to uint8_t otherwise is accessed in "word" sized chunks
		
		// mirroring moved to initialization to optimize runtime performance
		//if (descMask.Mirrored)
		//	return( ((uint8_t)*(descMask.Mask + (y * descMask.Stride) + (descMask.Stride - 1 - x))) );
		//else
			return( ((uint8_t)*(descMask.Mask + (y * descMask.Stride) + x)) );
	}

	template<uint32_t const RenderingFlags = (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE)>
	NOINLINE void DrawVLine_Masked(point2D_t const start, uint32_t const Height, xDMA2D::AlphaLuma const AlphaLuma, MaskDesc const& descMask)
	{
		// Original drawing start coordinates
		uint32_t x, y, xMask, yMask;
		
		{
			// Get Mask start position to be relative to Mask Origin
			point2D_t maskStart = p2D_sub(start, descMask.Origin);
			maskStart = p2D_add(maskStart, descMask.MaskOrigin);
			
			// Original drawing start coordinates
			x = start.pt.x;
			y = start.pt.y;

			xMask = maskStart.pt.x;
			yMask = maskStart.pt.y;
			
			// Also Add Offset to appropriate component
			yMask += descMask.CurOffset;
			//if (MaskMirrored)
			//	maskStart.pt.x -= (descMask.Stride - 1);
		}

		// y will always be at least 1 pixel
		// clamp y height
		uint32_t RunHeight = min(Height, OLED::SCREEN_HEIGHT - y);

		xDMA2D::AlphaLuma MixedAlphaLuma( (AlphaLuma.v * getMaskPixel(xMask, yMask, descMask)) >> 8 );

		// only first and last pixekl need anti-aliasing (edges)
		DrawPixelAA<OLED::BACK_BUFFER, RenderingFlags>(x, y, MixedAlphaLuma); // first

		if (0 != --RunHeight)	// Single pixel case, don't want to draw second AA pixel
		{
			++y; ++yMask;

			while (0 != --RunHeight)
			{
				MixedAlphaLuma.v = (AlphaLuma.v * getMaskPixel(xMask, yMask, descMask)) >> 8;

				DrawPixel<OLED::BACK_BUFFER, RenderingFlags>(x, y, MixedAlphaLuma);
				++y; ++yMask;
			}

			MixedAlphaLuma.v = (AlphaLuma.v * getMaskPixel(xMask, yMask, descMask)) >> 8; 
			
			DrawPixelAA<OLED::BACK_BUFFER, RenderingFlags>(x, y, MixedAlphaLuma); // last
		}
	}
	
	STATIC_INLINE void DrawHLineBlendFast( uint32_t x, uint32_t const y, uint32_t const Width, xDMA2D::AlphaLuma const oAlphaLuma, int32_t const Threshold = Constants::n255) 	// nice has curvature illusion, don't change
	{
		// clamp x width
		uint32_t RunWidth = min(Width, OLED::SCREEN_WIDTH - x);
		
		int32_t const Luma( oAlphaLuma.pixel.Luma - Constants::n16 );
		
		while ( 0 != RunWidth )
		{
			int32_t CurPixel = 0xFF & GetPixel(x, y);
			uint32_t Alpha(0);
			
			int32_t const DeltaPixel(CurPixel - Constants::n16);
			
			if ( DeltaPixel > (Threshold) )
				Alpha = 0;
			else if ( DeltaPixel > (Threshold>>1) )
				Alpha = (oAlphaLuma.pixel.Alpha>>1);
			else
				Alpha = oAlphaLuma.pixel.Alpha;
			
			if ( 0 != Alpha ) {
				CurPixel = ((CurPixel + Luma) >> 1);
				if ( CurPixel > (Threshold >> 1) ) {
					DrawPixel<OLED::FRONT_BUFFER, SHADE_ENABLE>( x, y, xDMA2D::AlphaLuma( Alpha, __USAT( CurPixel, Constants::SATBIT_256 )).v );
				}
			}
			
			++x;
			--RunWidth;
		}
	}
	template<uint32_t const TargetBuffer, uint32_t const RenderingFlags = (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE)>
	STATIC_INLINE void DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, xDMA2D::AlphaLuma const AlphaLuma)
	{
		 static constexpr int16_t const NumLevels = 256,
																		IntensityBits = 7;
		 
		 int16_t const BaseShade(AlphaLuma.v);
		 int16_t IntensityShift, ErrorAdj, ErrorAcc;
		 int16_t ErrorAccTemp, Weighting, WeightingComplementMask;
		 int16_t DeltaX, DeltaY, XDir;

		 setDepthLevel_ToFarthest();
		 
		 /* Make sure the line runs top to bottom */
		 if (Y0 > Y1) {
			 std::swap(X0, X1);
			 std::swap(Y0, Y1);
		 }

		 if ((DeltaX = X1 - X0) >= 0) {
				XDir = 1;
		 } else {
				XDir = -1;
				DeltaX = -DeltaX; /* make DeltaX positive */
		 }
		 /* Special-case horizontal, vertical, and diagonal lines, which
				require no weighting because they go right through the center of
				every pixel */
		 if ( 0 == (DeltaY = Y1 - Y0) ) {
				/* Horizontal line */
			 if (CheckHLine( __USAT(min(X0, X1), OLED::Width_SATBITS), __USAT(Y0, OLED::Height_SATBITS), DeltaX)) {
				DrawHLine<TargetBuffer, RenderingFlags>( __USAT(min(X0, X1), OLED::Width_SATBITS), __USAT(Y0, OLED::Height_SATBITS), DeltaX);
			 }
			 return;
		 }
		 if ( 0 == DeltaX ) {
				/* Vertical line */
			 if (CheckVLine( X0, min(Y0, Y1), absolute(DeltaY))) {
				DrawVLine<TargetBuffer, RenderingFlags>(X0, min(Y0, Y1), absolute(DeltaY));
			 }
		 }

		/* Draw the initial pixel, which is always exactly intersected by
				the line and so needs no weighting */
		 DrawPixel_Clipped<TargetBuffer, RenderingFlags>(X0, Y0, BaseShade);
		 
		 /* Line is not horizontal, diagonal, or vertical */
		 ErrorAcc = 0;  /* initialize the line error accumulator to 0 */
		 /* # of bits by which to shift ErrorAcc to get intensity level */
		 IntensityShift = 16 - IntensityBits;
		 /* Mask used to flip all bits in an intensity weighting, producing the
				result (1 - intensity weighting) */
		 WeightingComplementMask = NumLevels - 1;
		 
		 /* Is this an X-major or Y-major line? */
		 if (DeltaY > DeltaX) {
				/* Y-major line; calculate 16-bit fixed-point fractional part of a
					 pixel that X advances each time Y advances 1 pixel, truncating the
					 result so that we won't overrun the endpoint along the X axis */
				ErrorAdj = ((unsigned long) DeltaX << 16) / (unsigned long) DeltaY;
				/* Draw all pixels other than the first and last */
				while (--DeltaY) {
					 ErrorAccTemp = ErrorAcc;   /* remember currrent accumulated error */
					 ErrorAcc += ErrorAdj;      /* calculate error for next pixel */
					 if (ErrorAcc <= ErrorAccTemp) {
							/* The error accumulator turned over, so advance the X coord */
							X0 += XDir;
					 }
					 ++Y0; /* Y-major, so always advance Y */
					 /* The IntensityBits most significant bits of ErrorAcc give us the
							intensity weighting for this pixel, and the complement of the
							weighting for the paired pixel */
					 Weighting = ErrorAcc >> IntensityShift;
					 DrawPixel_Clipped<TargetBuffer, RenderingFlags>(X0, Y0, BaseShade + Weighting);
					 DrawPixel_Clipped<TargetBuffer, RenderingFlags>(X0 + XDir, Y0,
									 BaseShade + (Weighting ^ WeightingComplementMask));
				}
		 }
		 else
		 {
			 /* It's an X-major line; calculate 16-bit fixed-point fractional part of a
					pixel that Y advances each time X advances 1 pixel, truncating the
					result to avoid overrunning the endpoint along the X axis */
			 ErrorAdj = ((unsigned long) DeltaY << 16) / (unsigned long) DeltaX;
			 /* Draw all pixels other than the first and last */
			 while (--DeltaX) {
					ErrorAccTemp = ErrorAcc;   /* remember currrent accumulated error */
					ErrorAcc += ErrorAdj;      /* calculate error for next pixel */
					if (ErrorAcc <= ErrorAccTemp) {
						 /* The error accumulator turned over, so advance the Y coord */
						 ++Y0;
					}
					X0 += XDir; /* X-major, so always advance X */
					/* The IntensityBits most significant bits of ErrorAcc give us the
						 intensity weighting for this pixel, and the complement of the
						 weighting for the paired pixel */
					Weighting = ErrorAcc >> IntensityShift;
					DrawPixel_Clipped<TargetBuffer, RenderingFlags>(X0, Y0, BaseShade + Weighting);
					DrawPixel_Clipped<TargetBuffer, RenderingFlags>(X0, Y0 + 1,
									BaseShade + (Weighting ^ WeightingComplementMask));
			 }
		 }
		 
		 /* Draw the final pixel, which is always exactly intersected by the line
				and so needs no weighting */
		 DrawPixel_Clipped<TargetBuffer, RenderingFlags>(X1, Y1, BaseShade);
	}
	
	//Line Clipping Algorithm
	//Liang-Barsky

	__attribute__((always_inline)) STATIC_INLINE bool const clipTest(float const p, float const q, float& ou1, float& ou2)
	{
		 float const u1( ou1 ), u2( ou2 );
		 float r;
		 if (p < 0.0f)
		 {
				r = q / p;
				if (r > u2)
					 return(false);
				else if (r > u1)
					 ou1 = r;
		 }
		 else if (p > 0.0f)
		 {
				r = q / p;
				if (r < u1)
					 return(false);
				else if (r < u2)
					 ou2 = r;
		 }
		 else if (q < 0.0f)
				return(false);
		 
		 return(true);;
	}

	__attribute__((always_inline)) STATIC_INLINE void clipLine(int& ox1, int& oy1, int& ox2, int& oy2)
	{
		static constexpr float X_MAX = OLED::SCREEN_WIDTH,
													 Y_MAX = OLED::SCREEN_HEIGHT;
		
		float const x1(ox1), y1(oy1), x2(ox2), y2(oy2);
		float const dx(x2 - x1),
							  dy(y2 - y1);
		
		float u1 = 0.0f,
					u2 = 1.0f;
		
		 if ((clipTest(-dx, x1/*-X_MIN*/, u1, u2)) &&
			   (clipTest( dx, X_MAX-x1, u1, u2)) &&
				 (clipTest(-dy, y1/*-Y_MIN*/, u1, u2)) &&
		     (clipTest( dy, Y_MAX-y1, u1, u2)))
		 {
				if (u1 > 0.0f) {
					ox1 = __USAT( int32::__roundf(__fma(u1, dx, x1)), OLED::Width_SATBITS);
					oy1 = __USAT( int32::__roundf(__fma(u1, dy, y1)), OLED::Height_SATBITS);
				}
				if (u2 < 1.0f) {
					 ox2 = __USAT( int32::__roundf(__fma(u2, dx, x1)), OLED::Width_SATBITS);
					 oy2 = __USAT( int32::__roundf(__fma(u2, dy, y1)), OLED::Height_SATBITS);
				}
		 }
	}
	// Optimized variant that removes fixed point and the division
	// for drawing ****multiple vertical lines*** succesively and accurately!
	template<uint32_t const TargetBuffer = OLED::BACK_BUFFER, uint32_t const RenderingFlags = (SHADE_ENABLE|Z_ENABLE|ZWRITE_ENABLE)>
	__attribute__((always_inline)) STATIC_INLINE void DrawVLines(point2D_t P0, point2D_t P1, uint32_t const uiHeight, xDMA2D::AlphaLuma const AlphaLuma)
	{
		int32_t X0(P0.pt.x), Y0(P0.pt.y);
		int32_t X1(P1.pt.x), Y1(P1.pt.y);
		
		int32_t DeltaX, DeltaY, XDir;
		
		/* Make sure the line runs top to bottom */
		if (Y0 > Y1) {
			std::swap(X0, X1);
			std::swap(Y0, Y1);
		}
		
		if ((DeltaX = X1 - X0) >= 0) {
			XDir = 1;
		}
		else {
			XDir = -1;
			DeltaX = -DeltaX; /* make DeltaX positive */
		}
		DeltaX = __USAT(DeltaX, OLED::Width_SATBITS);
		DeltaY = __USAT(Y1 - Y0, OLED::Height_SATBITS);
		
		if ( 0 == (DeltaX|DeltaY) )
			return;
		
		/* Special-case horizontal, vertical, (slopes) which
		require no weighting because they go right through the center of
		every pixel */
		if (0 == DeltaX) {
			/* Vertical line slope */
			if ( CheckVLine(X0, Y0, DeltaY) )
				return(DrawVLine<TargetBuffer, RenderingFlags>(X0, Y0, DeltaY, AlphaLuma));
			// else
			return;	// skip drawing at all
		}
		if (0 == DeltaY) {
			// Horizontal line slope //
			int32_t xMin = min(X0, X1);
			
			do
			{
				if ( CheckVLine(xMin, Y0, uiHeight ) )
					DrawVLine<TargetBuffer, RenderingFlags>(xMin, Y0, uiHeight, AlphaLuma);
				
				++xMin;
			} while (0 != --DeltaX);
			return;
		}

		/* Draw the initial pixel, which is always exactly intersected by
		the line and so needs no weighting */
		if ( CheckVLine(X0, Y0, uiHeight) )
			DrawVLine<TargetBuffer, RenderingFlags>(X0, Y0, uiHeight, AlphaLuma);

		/* Vertical Line series slope is not horizontal, or vertical */
		
		/* Is this an X-major or Y-major line? */
		if (DeltaY > DeltaX) {
			
			DeltaX <<= 1;
			int32_t Balance = DeltaX - DeltaY;
			uint32_t Count = DeltaY;
			DeltaY <<= 1;
			/* Y-major line */
			
			/* Draw all pixels other than the first and last */
			while (--Count) {
				
				if (Balance >= 0) {
					/* The error accumulator turned over, so advance the X coord */
					X0 += XDir;
					Balance -= DeltaY;
				} 
				Balance += DeltaX;
				++Y0; /* Y-major, so always advance Y */
				
				if ( CheckVLine(X0, Y0, uiHeight) )
					DrawVLine<TargetBuffer, RenderingFlags>(X0, Y0, uiHeight, AlphaLuma);
			}
		}
		else
		{
			DeltaY <<= 1;
			int32_t Balance = DeltaY - DeltaX;
			uint32_t Count = DeltaX;
			DeltaX <<= 1;
			/* It's an X-major line */
			
			/* Draw all pixels other than the first and last */
			while (--Count) {
				
				if (Balance >= 0) {
					/* The error accumulator turned over, so advance the Y coord */
					++Y0;
					Balance -= DeltaX;
				} 
				Balance += DeltaY;
				X0 += XDir; /* X-major, so always advance X */
				
				if ( CheckVLine(X0, Y0, uiHeight) )
					DrawVLine<TargetBuffer, RenderingFlags>(X0, Y0, uiHeight, AlphaLuma);
			}
		}

		/* Draw the final pixel, which is always exactly intersected by the line
		and so needs no weighting */
		if ( CheckVLine(X1, Y1, uiHeight) )
			DrawVLine<TargetBuffer, RenderingFlags>(X1, Y1, uiHeight, AlphaLuma);
	}

	// image[y][x] // assuming this is the original orientation 
	// image[x][original_width - y] // rotated 90 degrees ccw
	// image[original_height - x][y] // 90 degrees cw 
	// image[original_height - y][original_width - x] // 180 degrees

	
	
	
	
	
	
	
	
	
}// end namespace (root - OLED(


#endif

