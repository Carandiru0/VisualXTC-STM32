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
#include "fireeffect.h"
#include "oled.h"
#include "math_3d.h"
#include "noise.h"

static constexpr uint32_t const FRAMEBUFFER_Width_SATBITS = Constants::SATBIT_64,
														    FRAMEBUFFER_Height_SATBITS = Constants::SATBIT_32;
static constexpr float const FRAMEBUFFER_WIDTH = (1 << FRAMEBUFFER_Width_SATBITS),
														 FRAMEBUFFER_HEIGHT = (1 << FRAMEBUFFER_Height_SATBITS);

static constexpr float const INV_FRAMEBUFFER_WIDTH = 1.0f / (float)FRAMEBUFFER_WIDTH,
														 INV_FRAMEBUFFER_HEIGHT = 1.0f / (float)FRAMEBUFFER_HEIGHT;

STATIC_INLINE float const fractalNoise(vec2_t coord) {
	float value(0.0f);
	float scale(0.5f);

	#pragma unroll
	for (uint32_t i = 4; i != 0; --i ) {
		value += Noise::getBlueNoiseSample<float>(coord) * scale;
		coord = v2_adds(coord, 2.0f);
		scale *= 0.6f;
	}
	return(value);
}
STATIC_INLINE_PURE float const acesToneMapping(float const x) {
    return( (x * __fma(2.51f, x, 0.03f)) / __fma(x, __fma(2.43f, x, 0.59f), 0.14f) );
}
 void RenderFire(float const tNow, float const fSeed)
{
	static constexpr float const SQUARED_UINT8_MAX = UINT8_MAX*UINT8_MAX;
	
	uint8_t * const __restrict EffectFrameBuffer( OLED::getEffectFrameBuffer_0() );
	uint32_t yPixel(FRAMEBUFFER_HEIGHT); // inverted y-axis

	vec2_t uv;
	while (0 != yPixel)
	{
		uint32_t xPixel(FRAMEBUFFER_WIDTH);

		uint8_t * __restrict UserFrameBuffer = EffectFrameBuffer + (yPixel << FRAMEBUFFER_Width_SATBITS);

		uv.y = lerp(0.0f, 1.0f, 1.0f - (float)yPixel * INV_FRAMEBUFFER_HEIGHT);

		while (0 != xPixel)
		{
			uv.x = lerp(0.0f, 1.0f, (float)xPixel * INV_FRAMEBUFFER_WIDTH);

			// The following part is the original algorithm:
			vec2_t uv_scaled = v2_mul(uv, vec2_t(0.031f, 0.01f));

			float val = fractalNoise( v2_sub(uv_scaled, v2_muls(vec2_t(0.0f, 1.0f), tNow * 0.014f)) );
			val += fractalNoise( v2_sub(uv_scaled, v2_muls(vec2_t(0.0f, 1.0f), tNow * 0.007f)) );

			// Shape the flame
			val *= 1.4f - uv.y;
			val *= 1.0f - __fabsf(uv.x - 0.5f);
			
			// Non-linear mapping -- This is the crucial part to 
			// give it a flame-like appearance.
			
			val = __powf(__fmaxf(val - 0.1f, 0.0f), __fma(4.0f, (fSeed - 0.5f), 5.0f)) * 3.0f;
			val += smoothstep(0.0f, 0.2f, clampf(val));
			
			// The bottom part
			val += __powf(1.0f - __fmaxf(0.0f, uv.y - 0.1f), 16.0f) * 0.05f;
			
			val = acesToneMapping(val * (0.2f - fSeed * 0.1f)) * SQUARED_UINT8_MAX;
			val = __sqrtf(val); // sqrt can be 1 cycle when last fpu instruction
			
			*UserFrameBuffer++ = __USAT(int32::__roundf(val), Constants::SATBIT_256 );

			--xPixel;
		}
		
		--yPixel;
	}
	
	//OLED::Render8bitScreenLayer_Back(EffectFrameBuffer);
	OLED::RenderScreenLayer_Upscale<uint8_t, FRAMEBUFFER_Width_SATBITS, FRAMEBUFFER_Height_SATBITS>(getBackBuffer(), EffectFrameBuffer);
}
