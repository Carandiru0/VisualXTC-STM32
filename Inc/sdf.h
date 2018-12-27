/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef SDF_H
#define SDF_H
#include "PreprocessorCore.h"

#include "globals.h"
#include "commonmath.h"
#include "oled.h"

namespace SDFConstants
{
	
static constexpr uint32_t const SDF_DIMENSION = 128,
																SDF_SATBITS = Constants::SATBIT_128;
static constexpr float const PXRANGE = 2.0f;
static constexpr float const SDF_TO_SVG_SCALE = ((float)SDF_DIMENSION) / 2048.0f;
static constexpr float const RANGE = (PXRANGE / SDF_TO_SVG_SCALE) * SDF_TO_SVG_SCALE;
	
};

constexpr uint32_t const Orient_Normal = 0,
												 Orient_CW = 1,
												 Orient_CCW = 2,
												 Orient_Flipped = 3;

typedef struct sSignedDistanceFieldParams
{
	uint32_t xOffset, yOffset, 
					 outputWidth, outputHeight;
	
	Vector2 InverseOutputWidthHeight;

} SignedDistanceFieldParams;

template<uint32_t const Orientation = Orient_Normal, bool const Inverted = false>
void RenderSignedDistanceField(uint8_t * const __restrict RenderBuffer, 
																					uint8_t const * const __restrict p2DSDF, uint8_t const Shade, uint8_t const LastShade,
																					SignedDistanceFieldParams const * const __restrict Config);																	 
	
template<uint32_t const Orientation = Orient_Normal, bool const Inverted = false>
void RenderSignedDistanceField_DMA2D(uint8_t * const __restrict RenderBuffer, uint8_t const * const __restrict p2DSDF,
																					SignedDistanceFieldParams const * const __restrict Config);
#include "..\src\sdf.cpp"
																				
#endif

