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
#include "explosion.h"
#include "volumetricradialgrid.h"
#include "lighting.h"
#include "noise.h"
#include "rng.h"

#include <vector>

#ifdef DEBUG_RANGE
#include "debug.cpp"
#endif

static constexpr uint32_t const LIFETIME_ANIMATION = 6000;//ms

std::vector<Volumetric::xRow>	ExplosionInstance::ExplosionRows __attribute__((section (".dtcm.explosion_vector")));  // vector memory is persistance across instances
																																																										 // only one shockwave instance is active at a time
																																																										 // this improves performance and stability
																																																										 // by avoiding reallocation of vector
																																																										 // for any new or deleted shockwave instance

sExplosionInstance::sExplosionInstance( vec2_t const WorldCoordOrigin, float const Radius )
		: sRadialGridInstance(WorldCoordOrigin, Radius, LIFETIME_ANIMATION, ExplosionRows),
	PatternModSeedScaled( (RandomFloat() * 2.0f - 1.0f) * EXPLOSION_NOISE_PATTERN_SCALAR )
	{}								// 0.0f=>1.0f to -1.0f=>1.0f * SCALAR
		
// op does not r/w any global memory
__attribute__((always_inline)) STATIC_INLINE __attribute__((const)) uint32_t const shade_op_explosion(vec3_t const vPoint, point2D_t const vOrigin) 
{ 
	static constexpr float const MAXIMUM_DISTANCE = 0.04f * Volumetric::MAXIMUM_HEIGHT_FPIXELS;

	// everything by this point is pre-transformed to screenspace coordinates
	// so the screenspace coordinates relative to the screen space position of the light is correct
	// the y component (height) is also in pixels, x component is x pixels (isometric), z component is y pixels (isometric)
	// so needing gridspace world coordinates is uneccessary - everything is transformed to screenspace already
	// and can be relative to each other in pixels
	vec3_t const vScreenSpacePoint( vPoint.x, vPoint.y * Volumetric::MAXIMUM_HEIGHT_FPIXELS, vPoint.z );
	vec3_t const vScreenSpaceOrigin( vOrigin.pt.x, 0.0f, vOrigin.pt.y );
	// vPoint.y is normalized, thus the half height of the voxel object is always 0.5f * Volumetric::MAXIMUM_HEIGHT_FPIXELS
	vec3_t const vDir( v3_sub(vScreenSpacePoint, vScreenSpaceOrigin) );
	//vec3_t const vNormal( v3_normalize(vDir) );
	
	//lighting = v3_length( vDir ) - vPoint.y * Volumetric::MAXIMUM_HEIGHT_FPIXELS * 2.0f;
	//lighting = 1.0f - __expf(-4.5f * lighting);
	//lighting = v3_dot(vec3_t(0.0f, 1.0f, 0.0f), vNormal);
	//lighting = vPoint.y * 0.3f;
	
	float const fDistance(vScreenSpacePoint.y*0.25f);
	float fDensity(0.0f), fLighting;
	
	// point light calculations
	{
		float const lDist = __fmaxf(v3_length( v3_negate(vDir) ), 0.001f);

		fLighting = __expf(lDist*lDist*lDist*-0.000008f); // bloom
	}
	float ld(0.0f);
	if ( fDistance > MAXIMUM_DISTANCE )
	{
		// compute local density 
		ld = fDistance - MAXIMUM_DISTANCE;
					
		// compute weighting factor 
		float const w = (1.0f - fDensity) * ld;
	 
		// accumulate density
		fDensity += w + 0.005f;	// 1.0f/200.0f
	}
	
	//(td) * (1.0f - sum);
	fDensity = (fDensity+ld) * (1.0f - fLighting);
	
	fLighting = fDensity * (1.0f/Volumetric::MAXIMUM_HEIGHT_FPIXELS);
	
	// using Pixels structure to get simd saturation
	Pixels oAlphaLuma(0xCF, 255 - int32::__roundf( fLighting * Constants::nf255 ));
	oAlphaLuma.Saturate();
	
	// packing saturated result, and returning in required uint32_t ARGB format
	return( xDMA2D::AlphaLuma(oAlphaLuma.pixel.Alpha, oAlphaLuma.pixel.Luma).v );
}

STATIC_INLINE_PURE float const sdSphere( float const r, float const s )	// signed distance function for a sphere
{
	// v2_length(p) - s
  return( r - s );
}

// op does not write any global memory
__attribute__((pure)) STATIC_INLINE float const volumetric_explosion_distance(vec2_t const vDisplacement, float const tLocal, Volumetric::RadialGridInstance const* const __restrict Instance)
{
	static float constexpr BASE_PATTERN_SCALE = 0.40132f;
	sExplosionInstance const* const ExplosionInstance( reinterpret_cast<sExplosionInstance const* const __restrict>(Instance) ); // this is not an expensive downcast, type is always able to be determined to be sExplosionInstance
	
	float const fObjectMaxRadius( Instance->getRadius() );
	float const fRadius = v2_length(vDisplacement);
	
	float fDistance = sdSphere(fRadius, fObjectMaxRadius * tLocal);
	
	if (fDistance >= 0.0f)	// (negated) optimization to drop drawing this voxel right now
		return(0.0f); 
	
	// final += SpiralNoiseC(vec3(p.xy,final)*0.4132+333.)*2.4f; //1.25;
	// first parameter controls frequency of entire pattern, last parameter is a seed and scalar controls frequency of patterns inside
	// changing 1st parameter gives large scale closer to 0.0, and small scale closer to 1.0
	
	fDistance += 0.5f * fDistance * (fObjectMaxRadius - fRadius*0.25f) * 	
		Noise::getSpiralNoise3D( v3_fmas( BASE_PATTERN_SCALE + ExplosionInstance->PatternModSeedScaled, vec3_t( vDisplacement.x, vDisplacement.y, fRadius ), 0.5f*tLocal ), tLocal );
	
	return(fDistance);
}

__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE float const sdf_op_fnExplosion(vec2_t const vDisplacement, float const tLocal, Volumetric::RadialGridInstance const* const __restrict Instance)
{		
	//vec2_t const vRotatedDisplacement( v2_rotate( vDisplacement, /*(M_PI*0.008f)+*/(tLocal*0.75f) ) );
	
	float const fDistance = volumetric_explosion_distance( v2_muls(vDisplacement, 2.0f), tLocal, Instance) * 0.5f;
	
	return( (fDistance) * /*(-1.0f/24.0f)*/ Instance->getInvRadius() );/* ExplosionInstance::EXPLOSION_NORMALIZATION_AMPLITUDE_MAX*/// );
}

__attribute__((pure)) bool const isExplosionVisible( ExplosionInstance const* const __restrict Instance )
{
	// approximation, floored location while radius is ceiled, overcompensated, to use interger based visibility test
	return( !Volumetric::isRadialGridNotVisible(Instance->getOrigin(), Instance->getRadius())  );
}

bool const UpdateExplosion( uint32_t const tNow, ExplosionInstance* const __restrict Instance )
{
// Update base first
	if ( Instance->Update(tNow) )
	{

#ifdef DEBUG_RANGE
		static uint32_t tLastDebug;	// place holder testing
		if (tNow - tLastDebug > 250) {

			if ( Instance->RangeMax > -FLT_MAX && Instance->RangeMin < FLT_MAX ) {
				//DebugMessage("%.03f to %.03f", Instance->RangeMin, Instance->RangeMax);
			}
			tLastDebug = tNow;
		}
#endif
		
		return(true);
	}
	
	return(false); // dead instance
}

void RenderExplosion( ExplosionInstance const* const __restrict Instance )
{	
	//PROFILE_START();
	
	Volumetric::renderRadialGrid<sdf_op_fnExplosion, shade_op_explosion, 
															 Volumetric::CULL_EXPANDING, 
															 OLED::FRONT_BUFFER, (OLED::SHADE_ENABLE|OLED::Z_ENABLE/*|OLED::ZWRITE_ENABLE*/)>(Instance); 
	
	//PROFILE_END(750, 25000);
}
