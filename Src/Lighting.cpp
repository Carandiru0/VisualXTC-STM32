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
#include "lighting.h"
#include "IsoVoxel.h"
#include "world.h"

using namespace Lighting;

STATIC_INLINE_PURE vec3_t const CalcNormalLeftFace()
{
	float fLength;
	// get plot corner extents (bottom/top diamond)
	point2D_t const Origin(0,0);
	diamond2D_t cornersB;
		
	// get corner extents (bottom diamond)
	cornersB.pts.l = p2D_sub(Origin, point2D_t(Iso::GRID_RADII, 0));
	cornersB.pts.r = p2D_add(Origin, point2D_t(Iso::GRID_RADII, 0));
	cornersB.pts.t = p2D_sub(Origin, point2D_t(0, Iso::GRID_RADII>>1));
	cornersB.pts.b = p2D_add(Origin, point2D_t(0, Iso::GRID_RADII>>1));

	// Calculate normals
	vec3_t vNormalLeftFace;
	point2D_t vNormal;

	// Left Face
	vNormal = p2D_sub(cornersB.pts.r, cornersB.pts.b);
	vNormalLeftFace = vec3_t(vNormal.pt.x, 0.0f, vNormal.pt.y);
	return( v3_normalize(vNormalLeftFace) );
}
STATIC_INLINE_PURE vec3_t const CalcNormalFrontFace()
{
	// get plot corner extents (bottom/top diamond)
	point2D_t const Origin(0,0);
	diamond2D_t cornersB;
		
	// get corner extents (bottom diamond)
	cornersB.pts.l = p2D_sub(Origin, point2D_t(Iso::GRID_RADII, 0));
	cornersB.pts.r = p2D_add(Origin, point2D_t(Iso::GRID_RADII, 0));
	cornersB.pts.t = p2D_sub(Origin, point2D_t(0, Iso::GRID_RADII>>1));
	cornersB.pts.b = p2D_add(Origin, point2D_t(0, Iso::GRID_RADII>>1));

	// Calculate normals
	vec3_t vNormalFrontFace;
	point2D_t vNormal;

	// Front Face
	vNormal = p2D_sub(cornersB.pts.t, cornersB.pts.r);
	vNormalFrontFace = vec3_t(vNormal.pt.x, 0.0f, vNormal.pt.y);
	return( v3_normalize(vNormalFrontFace) );
}
STATIC_INLINE_PURE vec3_t const CalcNormalTopFace()
{
	return( vec3_t(0.0f, -1.0f, 0.0f) );
}

namespace Lighting
{

uLightingDefaults LightingDefaults 
	__attribute__((section (".dtcm")));

uLightingDefaults::uLightingDefaults()
: vNormal{ CalcNormalLeftFace(), CalcNormalFrontFace(), CalcNormalTopFace(), vec3_t(0.0f) }
{
}

ActiveLightSet::ActiveLightSet()
	: Lights{nullptr}, NumActive(0), tLastUpdate(0)
{
}

void ActiveLightSet::sLightState::Initialize( sLight* const __restrict pLight, uint32_t const tNow )
{
	Light = pLight;
	Original = *pLight; // saving original light data for restoration 
	Started = tNow;
	Duration = 0;
	EaseOutDuration = 0;
	State = ACTIVATE;
}
void ActiveLightSet::sLightState::Capture( uint32_t const tNow, uint32_t const tDuration, uint32_t const tEaseOutDuration )
{
	Started = tNow;
	Duration = tDuration;
	EaseOutDuration = tEaseOutDuration;
	State = ACTIVATE;
}
void ActiveLightSet::sLightState::UnCapture(uint32_t const tNow, uint32_t const tDuration)
{
	Started = tNow;
	Duration = tDuration;
	EaseOutDuration = 0;
	State = DEACTIVATE;
}

void ActiveLightSet::Initialize( sLight* const (&ppLights)[MAX_NUM_ACTIVE_LIGHTS], uint32_t const tNow )
{
	for ( int32_t iDx = MAX_NUM_ACTIVE_LIGHTS - 1 ; iDx >= 0 ; --iDx ) {
		LightStates[iDx].Initialize(ppLights[iDx], tNow);
	}
}

void ActiveLightSet::RestoreLightSettings(Light* const __restrict Dest, Light const& __restrict Src)
{
	// only restore settings to light that are neccessary and will not have a visible impact
	Dest->IntensityDiffuse = Src.IntensityDiffuse;
	Dest->IntensityAmbient = Src.IntensityAmbient;
}
void ActiveLightSet::UpdateLightPointers()
{
	// Tweak array of active lights so nullptrs are at end
	Light const* pTweak[MAX_NUM_ACTIVE_LIGHTS] = {nullptr};

	memcpy32((uint32_t* const __restrict)pTweak, (uint32_t const* const __restrict)Lights, MAX_NUM_ACTIVE_LIGHTS);
	memset(Lights, 0, (sizeof(Light const*) * MAX_NUM_ACTIVE_LIGHTS));
	
	Light const** pActiveDest = Lights;
	Light const* const* pActiveSrc = pTweak;
	
	for ( uint32_t iDx = MAX_NUM_ACTIVE_LIGHTS ; iDx != 0 ; --iDx ) {
		
		if ( nullptr != *pActiveSrc ) {
			*pActiveDest = *pActiveSrc;
			++pActiveDest;
		}
		
		++pActiveSrc;
	}
}

// Duration is total time active, ease out is "part" of total and must be less than or equal to Duration
bool const ActiveLightSet::ActivateLight(vec2_t Location, uint32_t const LightIndex, 
																				 uint32_t const tNow, uint32_t const tDuration, uint32_t const tEaseOutDuration)	// simple activate for duration, Location in world grid space
{
	if ( NumActive < MAX_NUM_ACTIVE_LIGHTS && LightIndex < MAX_NUM_ACTIVE_LIGHTS )
	{
		Location = world::v2_GridToScreen(Location);

		LightState* pLightState(nullptr);
		if ( 0 == LightStates[LightIndex].Duration ) { // free to use?
			pLightState = &LightStates[LightIndex];
		}
		
		if ( nullptr != pLightState ) {
			pLightState->Capture(tNow, tDuration, tEaseOutDuration);
			// set position
			pLightState->Light->Position = vec3_t(Location.x, pLightState->Light->Position.y, Location.y);
			// add to the active lights array
			Lights[++NumActive] = pLightState->Light;
			
			// must run:
			UpdateLightPointers();
			
			return(true);
		}
		
	}
	return(false);
}
bool const ActiveLightSet::DeActivateLight(uint32_t const LightIndex, uint32_t const tNow, uint32_t const tDuration)
{
	if ( NumActive > 0 && LightIndex < MAX_NUM_ACTIVE_LIGHTS )
	{
		LightState* pLightState(nullptr);
		if ( 0 != LightStates[LightIndex].Duration ) { // is currently active?
			pLightState = &LightStates[LightIndex];
		}
		
		if ( nullptr != pLightState ) {
			
			bool bActiveLightRemoved(false);
			
			for ( int32_t iDx = MAX_NUM_ACTIVE_LIGHTS - 1 ; iDx >= 0 ; --iDx ) {
				Light const*& pActiveLight = Lights[iDx];
				if ( nullptr != pActiveLight ) {
					if ( pActiveLight == pLightState->Light ) {
							
						pActiveLight = nullptr;
						--NumActive;
						bActiveLightRemoved = true;
						break;
					}
				}
			}
			
			if ( bActiveLightRemoved ) {
				// simple case (remove/complete disable indefinite)
				if ( INFINITE_DURATION == tDuration ) {
					pLightState->Duration = 0;		// reset duration (flag as free to use)
				}
				else {	// duration used for deactvation time, light is not flagged as free to use
					pLightState->UnCapture(tNow, tDuration);
				}
				
				// must run:
				UpdateLightPointers();
				return(true);
			}
		}
	}
	return(false);
}
void ActiveLightSet::UpdateActiveLighting(uint32_t const tNow)
{
	if ( (tNow - tLastUpdate > LIGHTING_UPDATE_RATE) ) {
			
		for ( int32_t iDy = MAX_NUM_ACTIVE_LIGHTS - 1 ; iDy >= 0 ; --iDy ) {
			LightState* const pLightState(&LightStates[iDy]);
			if ( 0 != pLightState->Duration && INFINITE_DURATION != pLightState->Duration ) // only care about active lights
			{
				uint32_t const tDelta(tNow - pLightState->Started);
				
				if ( pLightState->State >= 0 ) // activate/reactivate
				{
					if ( tDelta >= pLightState->Duration ) {
						// find and remove from active lights array (if not a reactivation....)
						for ( int32_t iDx = MAX_NUM_ACTIVE_LIGHTS - 1 ; iDx >= 0 ; --iDx ) {
							Light const* const pActiveLight = Lights[iDx];
							if ( nullptr != pActiveLight ) {
								if ( pActiveLight == pLightState->Light ) {
																		
									// restore light settings to original if it was modified
									if ( nullptr != RestoreLights[iDy] ) {	// there is a straight one to one mapping of the arrays RestoreLights and LightStates
										RestoreLightSettings(pLightState->Light, pLightState->Original);
										// remove from restore lights cache array
										RestoreLights[iDx] = nullptr;
									}
									
									if ( 0 == pLightState->State ) // activate only
									{
										// this is it remove from active light array
										Lights[iDx] = nullptr;
										pLightState->Duration = 0;		// reset duration (flag as free to use)
										--NumActive;   // Decrement # of active lights
									}
									else { // reactivation, change duration to indefinte
										pLightState->Duration = INFINITE_DURATION;
									}
									break; 
								}
							}
						}
					}
					else if ( tDelta >= pLightState->EaseOutDuration ) {
						// fade out lighting
						if ( nullptr == RestoreLights[iDy] ) { // there is a straight one to one mapping of the arrays RestoreLights and LightStates
							RestoreLights[iDy] = &pLightState->Original;	// for restore done later...
						}
											
						// tDelta is current Duration, better to interpolate over time using Current duration, minus the easeout duration, compared with total duration
						//                                                     aka)								tNow							tStart										tEnd
						float const fTDurationFromEaseOutStart(tDelta - pLightState->EaseOutDuration);
						float const fTFinalTotalDuration(pLightState->Duration - pLightState->EaseOutDuration);
						
						float const fTInterpolation = clampf(fTDurationFromEaseOutStart / fTFinalTotalDuration); // ensure we donn't go past 1.0f
						
						// diffuse
						float const fDiffuse = smoothstep(pLightState->Original.IntensityDiffuse, 0.0f, fTInterpolation);
						// ambient
						float const fAmbient = smoothstep(pLightState->Original.IntensityAmbient, 0.0f, fTInterpolation);
						
						// modify existing light diffuse * ambient intensities
						pLightState->Light->IntensityDiffuse = fDiffuse;
						pLightState->Light->IntensityAmbient = fAmbient;
					}
				}	
				else { // deactivate
					if ( tDelta >= pLightState->Duration ) {
						// temporarily set duration to 0 so activate light succeeds (faux free to use)
						pLightState->Duration = 0;	
						
						// re-activate light
						ActivateLight(vec2_t(0), iDy, tNow, REACTIVATION_DURATION);
						
						// change state from "activate" to reactivate
						pLightState->State = REACTIVATE;
					}
				}
			}
		}
		
		UpdateLightPointers();

		tLastUpdate = tNow;
	}
	
	// always ensure a light is active
	if ( 0 == NumActive ) {
		// force light 0 on
		ActivateLight(vec2_t(0), 0, tNow, INFINITE_DURATION);
	}
}

} // end namespace