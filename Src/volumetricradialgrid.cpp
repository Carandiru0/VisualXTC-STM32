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
#include "VOLUMETRICRADIALGRID.h"

namespace Volumetric
{
	
bool const sRadialGridInstance::Update(uint32_t const tNow)
{			
	if (isInvalidated()) {
		Volumetric::radialgrid_generate(getRadius(), const_cast<std::vector<Volumetric::xRow>& __restrict>(InstanceRows));
		resetInvalidated();
	}
	
	return(UpdateLocalTime(tNow));
}
	
	// Stored as relative displacement, not absolute positions
	// Length now optimized out, see xRow struct
#define plot4points(x, y, vecRows) 				\
{ 																				\
	float const xn(-x); 										\
																					\
	vecRows.push_back( xRow(xn, y) ); 			\
																					\
	if (0.0f != y) 													\
		vecRows.push_back( xRow(xn, -y) ); 		\
}  																				\

NOINLINE void radialgrid_generate(float const radius, std::vector<xRow>& __restrict vecRows)
{
	float  error(-radius),
			   x(radius),
			   y(0.0f);
	
	vecRows.clear(); // important!!!!
	vecRows.reserve((((uint32_t)radius) << 1) * ((uint32_t)Iso::TINY_GRID_INVSCALE) + 1);
	
	while (x >= y)
	{
		float const lastY(y);

		error += y;
		y += Iso::TINY_GRID_SCALE;
		error += y;

		plot4points(x, lastY, vecRows);

		if (error >= 0.0f)
		{
			if (x != lastY)
				plot4points(lastY, x, vecRows);		

			error -= x;
			x -= Iso::TINY_GRID_SCALE;
			error -= x;
		}
	}
	
	std::sort(vecRows.begin(), vecRows.end());
	//vecRows.shrink_to_fit(); kinda not sure avoiding deallocations for more stability, therefore vector only grows in capacity
																																										// if needed by reserve above (less fragmentation of heap)
}

} // end namespace


