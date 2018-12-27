/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#include "isodepth.h"
#include "noise.h"
#include "world.h"
#include "debug.cpp"

sIsoDepth_Private IsoDepth_Private
	__attribute__((section (".dtcm")));

static constexpr float const EYE_X = 0.64f,   // Values tweaked, don't change
														 EYE_Y = 0.36f;
static float const fEyeSinAngleX(ARM__sinf(EYE_X)), 
									 fEyeCosAngleX(ARM__cosf(EYE_X)),
								   fEyeSinAngleY(ARM__sinf(EYE_Y));

/*
static vec3_t const vForward
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) 							// used a lot
__attribute__((section (".dtcm_const")))(0.612375, 0.612375, -0.50000);
// https://gamedev.stackexchange.com/questions/61963/what-is-the-camera-direction-vector-for-an-isometric-view#61966
*/

namespace IsoDepth
{
	
NOINLINE void Init()
{
	// setup values so first frame is not affected by dynamic ranges for depth & fog (full range)
	memset( &IsoDepth_Private, 0, sizeof(sIsoDepth_Private));
	
	// DEPTH //
	IsoDepth_Private.DynamicRangeMax = INT8_MAX;
	IsoDepth_Private.DynamicRangeMin = INT8_MIN;
	IsoDepth_Private.InvDynamicRangeDepthLength = 1.0f / (float)(IsoDepth_Private.DynamicRangeMax);
	// Reset for next frame calculation
	IsoDepth_Private.CurFrameRangeMin = INT8_MAX;
	IsoDepth_Private.CurFrameRangeMax = INT8_MIN;
	
	// FOG //
	IsoDepth_Private.FogDynamicRangeMax = UINT8_MAX;
	IsoDepth_Private.FogDynamicRangeMin = 0;
	IsoDepth_Private.FogInvDynamicRangeLength = 1.0f / (float)IsoDepth_Private.FogDynamicRangeMax;
	// Reset for next frame calculation
	IsoDepth_Private.CurFogDensityRangeMin = INT32_MAX;
	IsoDepth_Private.CurFogDensityRangeMax = INT32_MIN;
}

NOINLINE void UpdateDynamicRange()
{
	static constexpr float const EYE_DISTANCE = 256.0f; // Found with stepping valu debugly

	bool bDelta(false);
	
	// DEPTH //
	if ( IsoDepth_Private.DynamicRangeMin != IsoDepth_Private.CurFrameRangeMin ) {
		IsoDepth_Private.DynamicRangeMin = IsoDepth_Private.CurFrameRangeMin;
		bDelta = true;
	}
	
	if ( IsoDepth_Private.DynamicRangeMax != IsoDepth_Private.CurFrameRangeMax ) {
		IsoDepth_Private.DynamicRangeMax = IsoDepth_Private.CurFrameRangeMax;
		bDelta = true;
	}

	if ( bDelta ) 
	{
		IsoDepth_Private.InvDynamicRangeDepthLength = 1.0f / (float)IsoDepth_Private.DynamicRangeMax;
		
		// Calculate Eye Point //
		float const EyeDistance = EYE_DISTANCE - ((float)(IsoDepth_Private.DynamicRangeMax - IsoDepth_Private.DynamicRangeMin)) * 0.5f;
		
		IsoDepth_Private.vEyePt = vec3_t( EyeDistance * 3.0f * fEyeSinAngleX * fEyeSinAngleY, EyeDistance * 2.0f * fEyeCosAngleX * fEyeSinAngleY, EyeDistance * fEyeSinAngleY );
	}
	// Reset for next frame calculation
	IsoDepth_Private.CurFrameRangeMin = INT8_MAX;
	IsoDepth_Private.CurFrameRangeMax = INT8_MIN;
	
	
	// FOG //
	bDelta = false;
	if ( IsoDepth_Private.FogDynamicRangeMin != IsoDepth_Private.CurFogDensityRangeMin ) {
		IsoDepth_Private.FogDynamicRangeMin = IsoDepth_Private.CurFogDensityRangeMin;
		bDelta = true;
	}
	
	if ( IsoDepth_Private.FogDynamicRangeMax != IsoDepth_Private.CurFogDensityRangeMax ) {
		IsoDepth_Private.FogDynamicRangeMax = IsoDepth_Private.CurFogDensityRangeMax;
		bDelta = true;
	}

	if ( bDelta ) 
	{
		IsoDepth_Private.FogInvDynamicRangeLength = 1.0f / (float)IsoDepth_Private.FogDynamicRangeMax;
	}
	
	// fog as well reset for next frame calculation 
	IsoDepth_Private.CurFogDensityRangeMin = INT32_MAX;
	IsoDepth_Private.CurFogDensityRangeMax = INT32_MIN;
	
	ResetActiveFogShade();
}

xDMA2D::AlphaLuma const getFogShadeAtPixel( uint32_t const pixelCoordX, uint32_t const pixelCoordY ) 
{ 
	return(DTCM::getFrontBuffer()[pixelCoordX][pixelCoordY]); 
}

} // end namespace