/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef ISODEPTH_H
#define ISODEPTH_H
#include "globals.h"
#include "point2D.h"
#include "DMA2D.hpp"

extern struct sIsoDepth_Private
{
uint32_t FogShade;
	
int32_t  FogDynamicRangeMin,
				 FogDynamicRangeMax,
				 CurFogDensityRangeMin, 
				 CurFogDensityRangeMax;

int32_t CurSelectedDepthLevel,
							 DynamicRangeMin,
							 DynamicRangeMax,
							 CurFrameRangeMin,
							 CurFrameRangeMax;

float	InvDynamicRangeDepthLength,
			FogInvDynamicRangeLength;

vec3_t vEyePt;

} IsoDepth_Private __attribute__((section (".dtcm")));
 

namespace IsoDepth
{     
    static constexpr bool const DEPTH_TEST_FAIL = false,
                                DEPTH_TEST_PASS = true;
     
		__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE uint32_t const getActiveFogShade() { return(IsoDepth_Private.FogShade); }
		__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE int32_t const getCurSelectedDepth() { return(IsoDepth_Private.CurSelectedDepthLevel); }
    __attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE int32_t const getDynamicRangeMin() { return(IsoDepth_Private.DynamicRangeMin); }
    __attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE int32_t const getDynamicRangeMax() { return(IsoDepth_Private.DynamicRangeMax); }

		NOINLINE void Init();
    NOINLINE void UpdateDynamicRange();	// CALLED DURING PARALLEL DMA2D OP, noinline WORKS FINE HERE
		
    STATIC_INLINE void 										UpdateActiveFogShade(vec3_t const vPoint, float const fLightingShade);
		xDMA2D::AlphaLuma const 							getFogShadeAtPixel( uint32_t const pixelCoordX, uint32_t const pixelCoordY );
		STATIC_INLINE void 										setActiueFogShade( xDMA2D::AlphaLuma const FogShade ) { IsoDepth_Private.FogShade = FogShade.v; }
		STATIC_INLINE void 										ResetActiveFogShade() { IsoDepth_Private.FogShade = 0; };
				
		__attribute__((always_inline)) STATIC_INLINE float const getDistance(int32_t const Depth = IsoDepth_Private.CurSelectedDepthLevel )
    {
			// in [-1.0f ... 1.0f] range
			return( ((float)(Depth) * IsoDepth_Private.InvDynamicRangeDepthLength) );
		}
    __attribute__((always_inline)) STATIC_INLINE float const getDensity(int32_t const FogAlpha)
    {
			// in [0.0f ... 1.0f] range
			return( ((float)((int32_t)FogAlpha - (int32_t)IsoDepth_Private.FogDynamicRangeMin) * IsoDepth_Private.FogInvDynamicRangeLength) );
		}
		
		__attribute__((always_inline)) STATIC_INLINE void NewDepthLevelSet(int32_t NewDepth)
    { 
			// for expanding the range/gradient to max and min that is possible
			NewDepth = __SSAT(NewDepth, 8);
			IsoDepth_Private.CurSelectedDepthLevel = __SSAT(int32::__roundf(getDistance(NewDepth) * Constants::nf127), 8);
			
      // Update min/max current frame trackingrange
      IsoDepth_Private.CurFrameRangeMin = min(IsoDepth_Private.CurFrameRangeMin, NewDepth);
      IsoDepth_Private.CurFrameRangeMax = max(IsoDepth_Private.CurFrameRangeMax, NewDepth);
    }
		
		// reads and writes global memory, only place this is called should be Lighting::Shade - which is a ramfunc
		__attribute__((always_inline)) STATIC_INLINE void UpdateActiveFogShade(vec3_t const vPoint, float const fLightingShade)
		{
			static constexpr float const FOG_SHADE_MAX = 0.5f;	// Fog Color
			static constexpr float const C = 0.55f,	// Found with stepping valu debugly
																	 B = -0.38f; // importanmt to negate
			
			float fFogAlpha, fInScattering;
			
			{
				float fogDistance(0.0f);   // fog distance is accurate do not scale
				vec3_t const vViewRay = v3_normalize(v3_sub(vPoint, IsoDepth_Private.vEyePt), &fogDistance);
				fogDistance = __fma(getDistance(), 0.5f, fogDistance);
				
				fInScattering = (1.0f - ARM__expf(fogDistance*vViewRay.y*B)/vViewRay.y);

				fFogAlpha = clampf( (C * ARM__expf(IsoDepth_Private.vEyePt.y*B)) * fInScattering);
			}
			
			float fFogShade;
			{
				float const FogLighting = ARM__expf(-fLightingShade*(fInScattering)*fFogAlpha) + 0.8f;
				
				//float const Noise = Noise::getBlueNoiseSample( vec2_t(vPoint.x, vPoint.z) ); noise is limited to the midpoint or height of a single voxel block
				// needs to be per pixel to get effect
				fFogShade = mix(fLightingShade, FogLighting, fFogAlpha);
			}
		
			int32_t const FogAlpha( int32::__roundf(fFogAlpha * Constants::nf255) );
			
			// Update min/max current frame trackingrange
			IsoDepth_Private.CurFogDensityRangeMin = min( IsoDepth_Private.CurFogDensityRangeMin, FogAlpha );
			IsoDepth_Private.CurFogDensityRangeMax = max( IsoDepth_Private.CurFogDensityRangeMax, FogAlpha );
			
			// combine results and saturate the faster way
			Pixels oAlphaLuma( int32::__roundf(getDensity(FogAlpha) * Constants::nf255), int32::__roundf(fFogShade * Constants::nf255) );
			oAlphaLuma.Saturate();
	
			// packing saturated result, and returning in required uint32_t ARGB format
			IsoDepth_Private.FogShade = xDMA2D::AlphaLuma(oAlphaLuma.pixel.Alpha, 
																										oAlphaLuma.pixel.Luma
																									 ).v;
		}
} // end namespace
#endif