/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef EXPLOSION_H
#define EXPLOSION_H

#include "volumetricradialgrid.h"
#include <vector>

typedef struct sExplosionInstance : public Volumetric::RadialGridInstance
{
	static constexpr float const EXPLOSION_NORMALIZATION_AMPLITUDE_MAX = 2.0f,
															 EXPLOSION_NOISE_PATTERN_SCALAR = 0.1f;
	
	sExplosionInstance( vec2_t const WorldCoordOrigin, float const Radius );
	
	float const PatternModSeedScaled;
public:
	static std::vector<Volumetric::xRow> ExplosionRows __attribute__((section (".dtcm.explosion_vector")));

} ExplosionInstance;

__attribute__((pure)) bool const isExplosionVisible( ExplosionInstance const* const __restrict Instance );
bool const UpdateExplosion( uint32_t const tNow, ExplosionInstance* const __restrict Instance );
void RenderExplosion( ExplosionInstance const* const __restrict Instance );

#endif


