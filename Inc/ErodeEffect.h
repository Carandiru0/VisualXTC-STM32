/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef ERODE_EFFECT_H
#define ERODE_EFFECT_H
#include "oled.h"

static constexpr uint32_t const ANIM_UNLOADED = UNLOADED,
																ANIM_PENDING = PENDING,
																ANIM_RUNNING = LOADED;

typedef struct sErodeEffect
{
	static constexpr uint32_t const INTERVAL_TRANSITION_ERODE_STEP = 36;
	static constexpr uint32_t const ERODE_THRESHOLD_DONE = 512;
	static constexpr uint32_t const BLUR_1_THRESHOLD = (OLED::SCREEN_WIDTH * OLED::SCREEN_HEIGHT) >> 1,
																	BLUR_2_THRESHOLD = BLUR_1_THRESHOLD >> 1,
																	BLUR_3_THRESHOLD = BLUR_2_THRESHOLD >> 1,
																  BLUR_4_THRESHOLD = BLUR_3_THRESHOLD >> 1;
	
private:	
	uint8_t * const __restrict WorkingRenderBuffer  				
	__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))),
					* const __restrict NextRenderBuffer  						// SDF Render finalizes to this
	__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))),
				  * const __restrict CurRenderBuffer							// for linear interpolation between rendered SDF
	__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)));
	
public:
	uint32_t FrontOrBackBuffer;
	uint32_t TransitionRenderState;
	uint32_t tLastErode,
					 tNextErode;
	uint8_t  Opacity;
		
	void Reset();
	void InitializeEffectBuffers( uint8_t const * const __restrict srcBufferL8 );
					
	uint32_t const Erode();
	void Blur( uint32_t const& Radius );
					
	void Render( uint32_t const tNow );
					
	sErodeEffect();
		
} ErodeEffect;

#endif



