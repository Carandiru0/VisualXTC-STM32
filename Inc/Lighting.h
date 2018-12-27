/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef LIGHTING_H
#define LIGHTING_H
#include "commonmath.h"
#include "math_3d.h"
#include "point2d.h"
#include "globals.h"
#include "oled.h"
#include "rng.h"
#include "vector_rotation.h"

//#define DEBUG_LIGHTING 

// helper macros
#define ISFOGENABLED(RenderingFlags) (OLED::FOG_ENABLE == (OLED::FOG_ENABLE & RenderingFlags))

namespace Lighting
{
	static constexpr uint32_t const NORMAL_LEFTFACE = 0,
																	NORMAL_FRONTFACE = 1,
																	NORMAL_TOPFACE = 2,
																	NORMAL_SPECIFIC = 3;

	// see shade_op.h for custom shading / lighting
extern union uLightingDefaults
{	
	struct {
		vec3_t const vNormaLeftFace,
								 vNormaFrontFace,
								 vNormaLTopFace;// Cached normals for isometric "cube" or "voxel"
		vec3_t 			 vNormalSpecific;
	};
	
	vec3_t vNormal[4];
	
	uLightingDefaults();
	
} LightingDefaults __attribute__((section (".dtcm")));
	
STATIC_INLINE __attribute__((pure)) vec3_t const getVoxelNormal( uint32_t const uiNormalID ) { return(LightingDefaults.vNormal[uiNormalID]); }

typedef struct sMaterial
{
	float Shade;
	
	inline __attribute__((pure)) operator float const() const { return(Shade); }
	
	inline sMaterial(float const& shade)
		: Shade(shade)
	{}
} Material;

typedef struct sLight
{
	static constexpr float const MAX_INCIDENT_ANGLE = MathConstants::kPI / 8.0f,								// tweaked values found debugly, don't change
															 BRIGHTNESS_FALLOFF = 0.55f,												// lower value equals higher brightness
															 AMBIENT_ATTENUATION_FACTOR = 0.76f*0.76f,
															 ATTENUATION_NONLINEARITY = 1.14f;
	
	float IntensityDiffuse,		// should be in range of 0.0f to 1.0f
		    IntensityAmbient;		// also ambient plus diffuse should not be greater than 1.0f

	vec3_t Position;
	vec2_rotation_t Angle;
	float FalloffMaxDistance,
				InvFalloffMaxDistance;
	
	inline sLight()
		: IntensityDiffuse(0.0f), IntensityAmbient(0.0f),
			FalloffMaxDistance(0.0f), InvFalloffMaxDistance(0.0f)
	{}
	inline sLight(float const& diffuse, float const& ambient, float const& falloff)
		: IntensityDiffuse(diffuse), IntensityAmbient(ambient), Angle(0.0f), 
			FalloffMaxDistance(falloff*ATTENUATION_NONLINEARITY), InvFalloffMaxDistance((1.0f / falloff) * BRIGHTNESS_FALLOFF)
	{}
	inline sLight(float const& diffuse, float const& ambient, float const& falloff, vec3_t const& position)
		: IntensityDiffuse(diffuse), IntensityAmbient(ambient), Position(position), Angle(0.0f), 
			FalloffMaxDistance(falloff*ATTENUATION_NONLINEARITY), InvFalloffMaxDistance((1.0f / falloff) * BRIGHTNESS_FALLOFF)
	{}
} Light;

static constexpr uint32_t const MAX_NUM_ACTIVE_LIGHTS = 3;
extern struct ActiveLightSet
{
	static constexpr uint32_t const LIGHTING_UPDATE_RATE = 33; // ms
	static constexpr uint32_t const INFINITE_DURATION = UINT32_MAX;
	static constexpr uint32_t const REACTIVATION_DURATION = 150;	// ms
	
	static constexpr int32_t const ACTIVATE = 0,
																 REACTIVATE = 1,
																 DEACTIVATE = -1;
	typedef struct sLightState
	{
		Light* 										Light;
		sLight										Original;
		uint32_t									Started,
															Duration,
															EaseOutDuration;
		int32_t										State; // true = activate, false = deactivate
		
		void Capture(uint32_t const tNow, uint32_t const tDuration, uint32_t const tEaseOutDuration);
		void UnCapture(uint32_t const tNow, uint32_t const tDuration);
		
		void Initialize(sLight* const __restrict pLight, uint32_t const tNow);
		
	} LightState;
	
	constexpr uint32_t const getNumMaxLights() const { return(MAX_NUM_ACTIVE_LIGHTS); }
	__attribute__((pure)) __inline uint32_t const getNumActiveLights() const { return(NumActive); }
	
	Light const*							Lights[MAX_NUM_ACTIVE_LIGHTS];
	Light const*							RestoreLights[MAX_NUM_ACTIVE_LIGHTS];
	
	LightState								LightStates[MAX_NUM_ACTIVE_LIGHTS];
	uint32_t								  NumActive,
														tLastUpdate;
	
	ActiveLightSet();
	
	void Initialize( sLight* const (&ppLights)[MAX_NUM_ACTIVE_LIGHTS], uint32_t const tNow );
	
	// Duration is total time active, ease out is "part" of total and must be less than or equal to Duration
	bool const ActivateLight(vec2_t WorldLocation, uint32_t const LightIndex, 
													 uint32_t const tNow, uint32_t const tDuration, uint32_t const tEaseOutDuration = 0);	// simple activate for duration, Location is in world grid space
	bool const DeActivateLight(uint32_t const LightIndex, uint32_t const tNow, uint32_t const tDuration = INFINITE_DURATION);
	void UpdateActiveLighting(uint32_t const tNow);
	
private:
	void RestoreLightSettings(Light* const __restrict Dest, Light const& __restrict Src);
	void UpdateLightPointers();
} ActiveLighting __attribute__((section (".dtcm")));


STATIC_INLINE_PURE vec3_t const reflect(vec3_t const I, vec3_t const N)	// incident ray vector, normal of surface
{
	// t = I - 2.0f * N * dot(N,I)
	return( v3_sub(I, v3_mul( v3_muls(N, 2.0f), v3_dot(N, I) )) );
}
STATIC_INLINE_PURE vec3_t const refract(vec3_t const I, vec3_t const N, float const etaRatio) // incident ray vector, normal of surface,
{																																															// coefficient of refractivity, ie.) 1.0 air, 1.5 water
	float const cosI = v3_dot( v3_negate(I), N );
	float const cosT2 = 1.0f - etaRatio * etaRatio * (1.0f - cosI * cosI);
	
	// t = etaRatio * I + (etaRatio*cosI - sqrt(abs(cosT2))) * N
	// t = t * (cosT2 > 0.0f)
	vec3_t const T = v3_muls(   v3_add( v3_muls(I, etaRatio), v3_muls(N, etaRatio * cosI - __sqrtf(__fabsf(cosT2))) ),
															(float const)(cosT2 > 0.0f)
													);
	
	return ( T );
}

// does not write any global memory, however call to UpdateFogShade does, but has its own function attributes set, even if inlined
template<bool const FogEnabled>
__ramfunc __attribute__((pure)) inline float const Shade_FullRange(vec3_t const vPoint, uint32_t const uiNormalID, float const fDiffuseMaterial) // main lighting routine, should be leveraged not duplicated
{	
	float lighting(0.0f);

	for ( int32_t iDx = ActiveLighting.getNumActiveLights() - 1 ; iDx >= 0 ; --iDx )
	{
		vec3_t vLight;
		float fDistance, fAttenuation;
		
		Light const* const __restrict pLight(ActiveLighting.Lights[iDx]);
			
		vLight = v3_sub(vPoint, pLight->Position);
		vLight = v3_normalize(vLight, &fDistance);
			
		// partial calculation, deferring rest of calc to exploit __sqrtf (which can be 1 cycle if result is not immediately used vs 14 cycles)
		fAttenuation = __sqrtf( fDistance * pLight->FalloffMaxDistance );
		
		float diffuse(0.0f);
		
		{
			float const LdotN = __fmaxf(0.0f, v3_dot(vLight, getVoxelNormal(uiNormalID)));
			
			if (LdotN > Light::MAX_INCIDENT_ANGLE) 
			{
				// add diffuse lighting contribution
				// Attenuate diffuse based on falloff				// this seems to not have a huge impact on perf, but looks much better for diffuse
				diffuse = (LdotN * pLight->IntensityDiffuse) * fDiffuseMaterial;
				
				
#ifdef DEBUG_LIGHTING
				// draw incident ray
				point2D_t MidPointOnLine(point);
				point2D_t LightPoint(pLight->Position);
				OLED::DrawLine<OLED::FRONT_BUFFER>(MidPointOnLine.pt.x, MidPointOnLine.pt.y,
																					 LightPoint.pt.x, LightPoint.pt.y, OLED::g3);
#endif	
			}
			
			fAttenuation = (fDistance + fAttenuation) * pLight->InvFalloffMaxDistance;
		
			// finsl lighting calculation
			// add diffuse lighting contribution
			// Attenuate ambient based on falloff

			lighting += __fma(pLight->IntensityAmbient * fDiffuseMaterial, 
												1.0f / (fAttenuation * Light::AMBIENT_ATTENUATION_FACTOR), 
												diffuse);
			
		} // nullptr check
		
	} // for

	if constexpr( FogEnabled ) {
		IsoDepth::UpdateActiveFogShade(vPoint, lighting);
	}
	
	return(lighting);
}

// ### for full range value return and a custom normal input
template<bool const FogEnabled>
__attribute__((always_inline)) STATIC_INLINE float const Shade_FullRange(vec3_t const vPoint, vec3_t const vNormal, float const fDiffuse) 
{	
	LightingDefaults.vNormal[NORMAL_SPECIFIC] = vNormal;
	return( Shade_FullRange<FogEnabled>(vPoint, NORMAL_SPECIFIC, fDiffuse) );
}

// #### for limited range value return
template<bool const FogEnabled>
__attribute__((always_inline)) STATIC_INLINE __attribute__((pure)) uint32_t const Shade(vec3_t const vPoint, uint32_t const uiNormalID, float const fDiffuse)
{
	float const fLighting = Shade_FullRange<FogEnabled>(vPoint, uiNormalID, fDiffuse);
	
	return( __USAT( int32::__roundf(fLighting * Constants::nf255), Constants::SATBIT_256) );
}

// #### for limited range value return and a custom normal input
template<bool const FogEnabled>
__attribute__((always_inline)) STATIC_INLINE uint32_t const Shade(vec3_t const vPoint, vec3_t const vNormal, float const fDiffuse)
{	
	LightingDefaults.vNormal[NORMAL_SPECIFIC] = vNormal;
	return( Shade<FogEnabled>(vPoint, NORMAL_SPECIFIC, fDiffuse) );
}

// default lighting op (fog enabled):
// op does not write any global memory
__attribute__((always_inline)) STATIC_INLINE __attribute__((pure)) uint32_t const shade_op_defaultlighting_fog(vec3_t const vPoint, uint32_t const uiNormalID, float const fDiffuse) 
{ 
	return( Shade<true>(vPoint, uiNormalID, fDiffuse) );
}

// default lighting op (with specific normal) (fog enabled):
// op does not write any global memory
__attribute__((always_inline)) STATIC_INLINE __attribute__((pure)) uint32_t const shade_op_defaultlighting_normal_fog(vec3_t const vPoint, vec3_t const vNormal, float const fDiffuse) 
{ 
	return( Shade<true>(vPoint, vNormal, fDiffuse) );
}

// default lighting op:
// op does not write any global memory
__attribute__((always_inline)) STATIC_INLINE __attribute__((pure)) uint32_t const shade_op_defaultlighting(vec3_t const vPoint, uint32_t const uiNormalID, float const fDiffuse) 
{ 
	return( Shade<false>(vPoint, uiNormalID, fDiffuse) );
}

// default lighting (with specific normal) op:
// op does not write any global memory
__attribute__((always_inline)) STATIC_INLINE __attribute__((pure)) uint32_t const shade_op_defaultlighting_normal(vec3_t const vPoint, vec3_t const vNormal, float const fDiffuse) 
{ 
	return( Shade<false>(vPoint, vNormal, fDiffuse) );
}

} // end namespace

#endif

