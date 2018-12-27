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
#include "fogeffect.h"
#include "oled.h"
#include "noise.h"
#include "isoVoxel.h"
#include "math_3d.h"
#include "debug.cpp"

static constexpr uint32_t const FRAMEBUFFER_Width_SATBITS = Constants::SATBIT_64,
														    FRAMEBUFFER_Height_SATBITS = Constants::SATBIT_16;	// should be same aspect ratio as OLED
static constexpr uint32_t const FRAMEBUFFER_WIDTH = (1 << FRAMEBUFFER_Width_SATBITS),
																FRAMEBUFFER_HEIGHT = (1 << FRAMEBUFFER_Height_SATBITS);

static constexpr float const INV_FRAMEBUFFER_WIDTH = 1.0f / (float)FRAMEBUFFER_WIDTH,
														 INV_FRAMEBUFFER_HEIGHT = 1.0f / (float)FRAMEBUFFER_HEIGHT;

constexpr float const ct_addInverse(float const fSum, float const fIter)
{
	if ( fIter > 0.0f ) {
		return( ct_addInverse( fSum + (1.0f/fIter), fIter - 1.0f ) );
	}
	
	return(fSum);
}

static constexpr uint32_t const fBmOctaves = 8;
static constexpr float const fMaxIterations = (float) ( fBmOctaves - 1 );
static constexpr float const ct_getInvMaximum()
{	
	constexpr float fMaximum = 0.0f;
	
	return( 1.0f / ct_addInverse(fMaximum, fMaxIterations) );
}
// Fractal Brownian motion, or something that passes for it anyway: range [-1, 1]
__ramfunc static float const fBm(vec3_t const uv)
{
	static constexpr float const maximum = ct_getInvMaximum();
	float sum(0.0f);

	for (uint32_t i = fBmOctaves - 1; i != 0 ; --i) {
		float const f2 = i+1;
		vec3_t const uvscaled = v3_muls(uv, f2);
		
		sum += Noise::getSimplexNoise3D(uvscaled.x, uvscaled.y, uvscaled.z) / f2;
	}
	return(sum * maximum);
}

// Simple vertical gradient:
STATIC_INLINE vec2_t const gradient(vec2_t const uv, vec2_t const cloudDimension) {
	
	vec2_t p(uv);// = v2_muls( v2_adds( uv, 1.0f), 0.5f);
	
	vec2_t pDistort = v2_adds( v2_muls( v2_length(p), 0.3f), 1.0f);
	p = v2_mul( pDistort, p );
	
 	return( v2_mul( v2_mul(p, p), cloudDimension) );   
}

static xDMA2D::do_chain_op const FinishUpsamplingStage_8bit(uint32_t const SourceAddress, uint32_t const OutputAddress, uint32_t const OutputWidth, uint32_t const OutputHeight);
void RenderFogSprite(float const tNow)
{
	static constexpr float const cloudDensity = 0.45f; 	// overall density [0,1]
	static constexpr float const noisiness = 1.0f; 	// overall strength of the noise effect [0,1]
	static constexpr float const speed = 0.11f;			// controls the animation speed [0, 0.1 ish)
	
	vec2_t const cloudDimension(2.5f, 4.8f); 	// (inverse) height of the input gradient [0,...)

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
			float shade(0.0f);
			
			uv.x = lerp(0.0f, 1.0f, (float)xPixel * INV_FRAMEBUFFER_WIDTH);

			vec2_t const uvo(uv);
			
			//uv = Iso::v2_ScreenToIso(uv);
			vec3_t p = vec3_t(uvo.x,  uvo.y, tNow*speed);
			
			float const f3DNoiseFractal = fBm(p);
			
			vec2_t duv = v2_muls( vec2_t(f3DNoiseFractal * 0.5f, f3DNoiseFractal), noisiness);
    
			vec2_t q2 = v2_muls( gradient( v2_add(uv,duv), cloudDimension ), cloudDensity);

			q2.x *= 0.1f * (0.5f - uv.x + 0.5f);
			q2.y *= (0.5f - uv.y + 0.5f);

			float const dist = v2_length(v2_sub(vec2_t(0.5f,0.5f), uv))*1.414213f; // Distance to Center
    
			constexpr float const OuterVig = 0.96f; // Position for the Outer vignette
			constexpr float const InnerVig = 0.6f; // Position for the Outer vignette
			constexpr float const InvDifference = 1.0f / (OuterVig-InnerVig);
			
			shade = q2.y * clampf( (OuterVig-dist)*InvDifference );
			
			*UserFrameBuffer++ = __USAT(int32::__roundf(shade * Constants::nf255), Constants::SATBIT_256 );

			--xPixel;
		}
		
		--yPixel;
	}
	
	//OLED::Render8bitScreenLayer_Back(EffectFrameBuffer);
	// 25 ms for all above + OLED::RenderScreenLayer_Upscale<uint8_t, FRAMEBUFFER_Width_SATBITS, FRAMEBUFFER_Height_SATBITS>((uint8_t* const __restrict)DTCM::getBackBuffer(), EffectFrameBuffer);
	// 16 ms for all above + OLED::upSample_256x64_from_x_8bit((uint8_t* const __restrict)DTCM::getBackBuffer(), EffectFrameBuffer, FRAMEBUFFER_Width_SATBITS);
	//PROFILE_START();
	//OLED::upSample_256x64_from_x_8bit((uint8_t* const __restrict)DTCM::getBackBuffer(), EffectFrameBuffer, FRAMEBUFFER_Width_SATBITS);
	/*
	xDMA2D::RESIZE_InitTypedef Resize = {
		(uint32_t)OLED::getEffectFrameBuffer_0(),     								// source bitmap Base Address // 
		(uint32_t)DTCM::getFrontBuffer(), // output bitmap Base Address //
		nullptr,																						// "done" callback (optional) //
			
		FRAMEBUFFER_WIDTH>>2,     // source pixel pitch //  
		LL_DMA2D_INPUT_MODE_ARGB8888,      // source color mode //
		0,              // souce X //  
		0,              // sourceY //
		FRAMEBUFFER_WIDTH>>2,            // source width // 
		FRAMEBUFFER_HEIGHT,            // source height //
		
		OLED::SCREEN_WIDTH,            // output pixel pitch //  
		0,              // output X //  
		0,              // output Y //
		OLED::SCREEN_WIDTH,            // output width // 
		OLED::SCREEN_HEIGHT            // output height //
	};

	xDMA2D::D2D_Resize_Setup(&Resize);
	*/
	// 64x16 -> 128x32
	xDMA2D::UpSample_8bit( xDMA2D::getDualFilterTarget(), OLED::getEffectFrameBuffer_0(), FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, &FinishUpsamplingStage_8bit );
	//PROFILE_END(750, 5000);
}

static xDMA2D::do_chain_op const FinishUpsamplingStage_32bit(uint32_t const SourceAddress, uint32_t const OutputAddress, uint32_t const OutputWidth, uint32_t const OutputHeight)
{
	return(nullptr); // end of chain
}
static void ChainUpsamplingStage_32bit(uint32_t const SourceAddress, uint32_t const OutputAddress, uint32_t const OutputWidth, uint32_t const OutputHeight)
{
	xDMA2D::UpSample_32bit( (uint32_t* const __restrict)DTCM::getFrontBuffer(), (uint32_t* const __restrict)OutputAddress, OutputWidth, OutputHeight, &FinishUpsamplingStage_32bit );
}
static xDMA2D::do_chain_op const FinishUpsamplingStage_8bit(uint32_t const SourceAddress, uint32_t const OutputAddress, uint32_t const OutputWidth, uint32_t const OutputHeight)
{
	// 128x32 -> 256x64
	return(&ChainUpsamplingStage_32bit); // chain operation
}
/*
float cloudDensity = 0.8; 	// overall density [0,1]
float noisiness = 0.35; 	// overall strength of the noise effect [0,1]
float speed = 0.1;			// controls the animation speed [0, 0.1 ish)
vec2 cloudDimension = vec2(2.0, 5.0); 	// (inverse) height of the input gradient [0,...)

/// Cloud stuff:
const float maximum = 1.0/1.0 + 1.0/2.0 + 1.0/3.0 + 1.0/4.0;// + 1.0/5.0 + 1.0/6.0 + 1.0/7.0 + 1.0/8.0;
// Fractal Brownian motion, or something that passes for it anyway: range [-1, 1]
float fBm(vec3 uv)
{
    float sum = 0.0;
    for (int i = 0; i < 8; ++i) {
        float f = float(i+1);
        sum += snoise(uv*f) / f;
    }
    return sum / maximum;
}

// Simple vertical gradient:
vec2 gradient(vec2 uv) {
 	return (uv * uv * cloudDimension);   
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 uv = fragCoord.xy / iResolution.xy;
    vec3 p = vec3(uv, iTime*speed);
    vec3 someRandomOffset = vec3(0.1, 0.3, 0.2);
    vec2 duv = vec2(fBm(p), fBm(p + someRandomOffset)) * noisiness;
    
    vec2 q2 = gradient(uv + duv) * cloudDensity;

    q2.x *= 0.0f;//(0.5f * uv.x) - 0.5f;
    q2.y *= (0.5f - uv.y + 0.5f);
    
    float q = (q2.x + q2.y);// * 0.5f;
    
    fragColor = vec4(q,q,q, 1.0);
}
*/
