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
#include "VoxMissile.h"

#include "IsoVoxel.h"
#include "voxelModelsFRAM.h"
#include "Lighting.h"
#include "vector_rotation.h"

#ifdef VOX_DEBUG_ENABLED
#include "debug.cpp"
#endif

static constexpr uint32_t const SNAP_TIME = 500; // ms
static constexpr float const MISSILE_VELOCITY = 0.022f,
														 MISSILE_ALTITUDE = 36.0F,
														 BEGIN_DESCENT = 0.82f;		// Beginning [ 0.0f .... 1.0f ] End
	
VoxMissile::VoxMissile( uint32_t const tNow, vec2_t const worldCoord, vec2_t const worldCoordTarget )
	: Volumetric::voxB::voxelModelInstance_Dynamic(Volumetric::voxFRAM::voxelModelBlackBird, worldCoord),
		vStart(worldCoord), vTarget( worldCoordTarget ), tStart(tNow), fAltittude(MISSILE_ALTITUDE),
		YMajor( __fabsf(worldCoordTarget.y - worldCoord.y) > __fabsf(worldCoordTarget.x - worldCoord.x) )
{
	// get distance
	float const fDistance = v2_length( v2_sub(worldCoordTarget, worldCoord) );
	// find total duration v = d/t, t = d/v
	float const tTotal = fDistance / MISSILE_VELOCITY;
	tDuration = uint32::__ceilf(tTotal); // must ge greater or equal to
	
	// save inverse for interpolation calculation
	fInvDuration = 1.0f / tTotal;	// for correct [0.0f ... 1.0f] interpolation range
	
	// Initial orientation
	Update(tStart);
}

bool const VoxMissile::isVisible() const
{
	// approximation, floored location while radius is ceiled, overcompensated, to use interger based visibility test
	return( !Volumetric::isVoxModelInstanceNotVisible(vLoc, Radius)  );
}
bool const VoxMissile::Update( uint32_t const tNow )
{
	uint32_t const tDelta(tNow - tStart);
	
	if ( tDelta < (tDuration) ) {
		float tDeltaNormalized = (float)tDelta * fInvDuration;
		
		vec2_t vNext;
		if ( (tDeltaNormalized - 1.0f) < 0.0f )
		{
			if ( YMajor ) {
				vLoc.x = ease_out_quadratic(vStart.x, vTarget.x, tDeltaNormalized);
				vLoc.y = ease_in_quadratic(vStart.y, vTarget.y, tDeltaNormalized);
				
				// calculate the next location 16 ms from now
				tDeltaNormalized = clampf((float)(tDelta + Constants::n16) * fInvDuration);
				
				vNext.x = ease_out_quadratic(vStart.x, vTarget.x, tDeltaNormalized);
				vNext.y = ease_in_quadratic(vStart.y, vTarget.y, tDeltaNormalized);
			}
			else {
				vLoc.x = ease_in_quadratic(vStart.x, vTarget.x, tDeltaNormalized);
				vLoc.y = ease_out_quadratic(vStart.y, vTarget.y, tDeltaNormalized);
				
				// calculate the next location 16 ms from now
				tDeltaNormalized = clampf((float)(tDelta + Constants::n16) * fInvDuration);
				
				vNext.x = ease_in_quadratic(vStart.x, vTarget.x, tDeltaNormalized);
				vNext.y = ease_out_quadratic(vStart.y, vTarget.y, tDeltaNormalized);
			}
			
			
			// point in the direction of "heading"
			vR.vR = v2_normalize( v2_sub(vNext, vLoc) );
			// off by 90 degrees
		//	std::swap(vR.vR.x, vR.vR.y);
		//	vR.vR.y = -vR.vR.y;
			// save angle as computed sincos pair, this is the leveraged transformation
			
			// MOVE ALTITUDE TOWARD ZERO 
			tDeltaNormalized -= BEGIN_DESCENT;
			if ( tDeltaNormalized > 0.0f ) {
				
				fAltittude = ease_inout_quadratic(MISSILE_ALTITUDE, 0.0f, tDeltaNormalized/(1.0f - BEGIN_DESCENT));
			}
			return(true); // still alive
		}
	}

	return(false);   // dead instance
}
void VoxMissile::Render() const
{
	Volumetric::RenderVoxelModel<Lighting::shade_op_defaultlighting, Lighting::shade_op_defaultlighting_normal, 
															 OLED::FRONT_BUFFER, (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE)>
									             (vLoc, vR, &model, fAltittude);
}

