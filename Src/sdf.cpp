/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef SDF_CPP
#define SDF_CPP

#include "sdf.h"
#include "oled.h"
#include "math_3d.h"

__attribute__((always_inline)) STATIC_INLINE_PURE float const distVal(float32_t const dist)
{
	return( __fma((dist - Constants::nfPoint5), SDFConstants::RANGE, Constants::nfPoint5) );
}
__attribute__((always_inline)) STATIC_INLINE_PURE float const distValInverted(float32_t const dist)
{
	return( 1.0f - distVal(dist) );
}
/*
	double x = pos.x*w-.5;
    double y = pos.y*h-.5;
    int l = (int) floor(x);
    int b = (int) floor(y);
    int r = l+1;
    int t = b+1;
    double lr = x-l;
    double bt = y-b;
    l = clamp(l, w-1), r = clamp(r, w-1);
    b = clamp(b, h-1), t = clamp(t, h-1);
    return mix(mix(bitmap(l, b), bitmap(r, b), lr), mix(bitmap(l, t), bitmap(r, t), lr), bt);
	*/
//template <typename T, std::size_t const N>
//constexpr std::size_t const countof(T const (&)[N]) noexcept { return(N); }

__attribute__((always_inline)) STATIC_INLINE void ShadePixel(float const Alpha, uint8_t const Shade, uint8_t const LastShade, uint32_t const xPixel, uint32_t const yPixel,
																														 uint8_t * const __restrict RenderBuffer)
{
	// Lerp 
	// This is correct rendering, no gaps or cracks and blending between layer gradients.
	uint8_t const LerpShade = __USAT( int32::__roundf(smoothstep(LastShade, Shade, Alpha)), Constants::SATBIT_256);
	
	*(RenderBuffer + yPixel * OLED::SCREEN_WIDTH + xPixel) = LerpShade;
}

template<uint32_t const Orientation, bool const Inverted>  // statically evaluated template parameter @ compile time
void RenderSignedDistanceField(uint8_t * const __restrict RenderBuffer,
																					uint8_t const * const __restrict p2DSDF, uint8_t const Shade, uint8_t const LastShade,
																					SignedDistanceFieldParams const * const __restrict Config)
{
	/*
	int w = output.width(), h = output.height();
    pxRange *= (double) (w+h)/(sdf.width()+sdf.height());
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float s = sample(sdf, Point2((x+.5)/w, (y+.5)/h));
            output(x, y) = distVal(s, pxRange);
        }
	*/
	Vector2 const Step = Config->InverseOutputWidthHeight;
	vec2_t Start( (float32_t)Config->xOffset, (float32_t)Config->yOffset );
	
	//arm_offset_f32(Start.v, 0.5f, Start.v, 2);
	//arm_mult_f32(Start.v, (float32_t*)Step.v, Start.v, 2);
	Start = v2_adds(Start, 0.5f);
	Start = v2_mul(Start, (vec2_t)Step);
	
	Vector2 Pos = Vector2(Start.x, Start.y);
	
	uint32_t const clampWidth = min(Config->outputWidth, OLED::SCREEN_WIDTH);
	uint32_t clampHeight = min(Config->outputHeight, OLED::SCREEN_HEIGHT);
		
	uint32_t yPixel = 0;
	
	while( 0 != clampHeight )
  {
		uint32_t wlen = clampWidth;
		uint32_t xPixel = 0;
		Pos.pt.x = Start.x;
		
		while( 0 != wlen ) 
		{		
			// alphablended, by layer and bkgrnd, antialiasing
			
			Vector2 PosOrient(Pos);
			if (Orient_CW == Orientation) // statically evaluated template parameter @ compile time
			{
				// image[original_height - x][y] // 90 degrees cw 
				PosOrient.pt.y = 1.0f - Pos.pt.x;
				PosOrient.pt.x = Pos.pt.y;
			}
			else if (Orient_CCW == Orientation)
			{
				// image[x][original_width - y] // rotated 90 degrees ccw
				PosOrient.pt.y = Pos.pt.x;
				PosOrient.pt.x = 1.0f - Pos.pt.y;
			}
			else if ( Orient_Flipped == Orientation)
			{
				// image[original_height - y][original_width - x] // 180 degrees 
				PosOrient.pt.y = 1.0f - Pos.pt.y;
				PosOrient.pt.x = 1.0f - Pos.pt.x;
			}
				
			float Alpha;
			
			if (Inverted) // statically evaluated template parameter @ compile time
			{
				Alpha = distValInverted( OLED::Texture::textureSampleBilinear_1D<float, SDFConstants::SDF_SATBITS>(p2DSDF, PosOrient) );
			}
			else
			{
				Alpha = distVal( OLED::Texture::textureSampleBilinear_1D<float, SDFConstants::SDF_SATBITS>(p2DSDF, PosOrient) );
			}
			
			if (Alpha > 0.0f)
				ShadePixel(Alpha, Shade, LastShade, xPixel, yPixel, RenderBuffer);

			Pos.pt.x += Step.pt.x;
			++xPixel;
			--wlen;
		}
		
		Pos.pt.y += Step.pt.y;
		++yPixel;
		--clampHeight;
	}
}

/*template<uint32_t const Orientation, bool const Inverted>  // statically evaluated template parameter @ compile time
void RenderSignedDistanceField_DMA2D(uint8_t * const __restrict RenderBuffer, uint8_t const * const __restrict p2DSDF,
																																				 SignedDistanceFieldParams const * const __restrict Config)
{
	Vector2 const Step = Config->InverseOutputWidthHeight;
	vec2_t Start( (float32_t)Config->xOffset, (float32_t)Config->yOffset );
	
	Start = v2_adds(Start, 0.5f);
	Start = v2_mul(Start, (vec2_t)Step);
	
	//arm_offset_f32(Start.v, 0.5f, Start.v, 2);
	//arm_mult_f32(Start.v, (float32_t*)Step.v, Start.v, 2);
	
	Vector2 Pos = Vector2(Start.x, Start.y);
	
	uint32_t const clampWidth = min(Config->outputWidth, OLED::SCREEN_WIDTH);
	uint32_t clampHeight = min(Config->outputHeight, OLED::SCREEN_HEIGHT);
		
	uint32_t yPixel = 0;
	
	while( 0 != clampHeight )
  {
		uint32_t wlen = clampWidth;
		uint32_t xPixel = 0;
		Pos.pt.x = Start.x;
		
		while( 0 != wlen ) 
		{		
			// alphablended, by layer and bkgrnd, antialiasing
			
			Vector2 PosOrient(Pos);
			if (Orient_CW == Orientation) // statically evaluated template parameter @ compile time
			{
				// image[original_height - x][y] // 90 degrees cw 
				PosOrient.pt.y = 1.0f - Pos.pt.x;
				PosOrient.pt.x = Pos.pt.y;
			}
			else if (Orient_CCW == Orientation)
			{
				// image[x][original_width - y] // rotated 90 degrees ccw
				PosOrient.pt.y = Pos.pt.x;
				PosOrient.pt.x = 1.0f - Pos.pt.y;
			}
			else if ( Orient_Flipped == Orientation)
			{
				// image[original_height - y][original_width - x] // 180 degrees 
				PosOrient.pt.y = 1.0f - Pos.pt.y;
				PosOrient.pt.x = 1.0f - Pos.pt.x;
			}
				
			uint8_t Alpha;
			
			if (Inverted) // statically evaluated template parameter @ compile time
			{
				 Alpha = distValInverted( sampleSDF<SDFConstants::SDF_DIMENSION>(p2DSDF, &PosOrient));
			}
			else
			{
				 Alpha = distVal( sampleSDF<SDFConstants::SDF_DIMENSION>(p2DSDF, &PosOrient));
			}
			
			*(RenderBuffer + yPixel * OLED::SCREEN_WIDTH + xPixel) = Alpha;

			Pos.pt.x += Step.pt.x;
			++xPixel;
			--wlen;
		}
		
		Pos.pt.y += Step.pt.y;
		++yPixel;
		--clampHeight;
	}
}*/
#endif

