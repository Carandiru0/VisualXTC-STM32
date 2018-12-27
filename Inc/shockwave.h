/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef SHOCKWAVE_H
#define SHOCKWAVE_H

#include "volumetricradialgrid.h"
#include <vector>

typedef struct sShockwaveInstance : public Volumetric::RadialGridInstance
{
	static constexpr float const SHOCKWAVE_NORMALIZATION_AMPLITUDE_MAX = 100.0f;
	
public:
	sShockwaveInstance( vec2_t const WorldCoordOrigin, float const Radius );
		
public:
	static std::vector<Volumetric::xRow> ShockwaveRows __attribute__((section (".dtcm.shockwave_vector")));

} ShockwaveInstance;

__attribute__((pure)) bool const isShockwaveVisible( ShockwaveInstance const * const __restrict Instance );
bool const UpdateShockwave( uint32_t const tNow, ShockwaveInstance* const __restrict Instance );
void RenderShockwave( ShockwaveInstance const* const __restrict Instance );

#endif


