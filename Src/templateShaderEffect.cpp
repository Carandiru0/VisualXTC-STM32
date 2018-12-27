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
#include "fractalboxesEffect.h"
#include "FLASH\imports.h"
#include "oled.h"
#include "math_3d.h"

static constexpr uint32_t const NOISE_TEXTURE_SATBITS = Constants::SATBIT_256,
																NOISE_TEXTURE_NBBYTES = (1 << NOISE_TEXTURE_SATBITS)*(1 << NOISE_TEXTURE_SATBITS);

static constexpr uint32_t const FRAMEBUFFER_Width_SATBITS = Constants::SATBIT_64,
														    FRAMEBUFFER_Height_SATBITS = Constants::SATBIT_32;
static constexpr float const FRAMEBUFFER_WIDTH = (1 << FRAMEBUFFER_Width_SATBITS),
														 FRAMEBUFFER_HEIGHT = (1 << FRAMEBUFFER_Height_SATBITS);

static constexpr float const INV_FRAMEBUFFER_WIDTH = 1.0f / (float)FRAMEBUFFER_WIDTH,
														 INV_FRAMEBUFFER_HEIGHT = 1.0f / (float)FRAMEBUFFER_HEIGHT;


__attribute__((section("ramfunc"))) void RenderFractalBoxes(float const tNow)
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

			
			*UserFrameBuffer++ = __USAT(val, Constants::SATBIT_256 );

			--xPixel;
		}
		
		--yPixel;
	}
	
	//OLED::Render8bitScreenLayer_Back(EffectFrameBuffer);
	OLED::Render8bitScreenLayer_Upscale<FRAMEBUFFER_Width_SATBITS, FRAMEBUFFER_Height_SATBITS>(EffectFrameBuffer);
}
