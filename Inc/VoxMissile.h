/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef VOX_MISSILE_H
#define VOX_MISSILE_H
#define VOX_IMPL
#include "VoxBinary.h"
#include "math_3d.h"

typedef struct VoxMissile : Volumetric::voxB::voxelModelInstance_Dynamic
{
	vec2_t const vStart, vTarget;
	
	float  fInvDuration,
				 fAltittude;
	
	uint32_t const tStart;
	uint32_t tDuration;
	
	bool const YMajor;
	
	bool const isVisible() const;
	bool const Update( uint32_t const tNow); 
	void Render() const;
	
	static bool const LoadModel();	// to be called init world init, once once!
	VoxMissile( uint32_t const tNow, vec2_t const worldCoord, vec2_t const worldCoordTarget );
	~VoxMissile() = default;
	
} VoxMissile;

#endif

