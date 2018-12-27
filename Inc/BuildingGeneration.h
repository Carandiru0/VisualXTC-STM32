/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef BUILDINGGENERATION_H
#define BUILDINGGENERATION_H
#include "isoVoxel.h"
#include "point2D.h"

namespace BuildingGen
{
	static constexpr uint32_t const MIN_PLOT_XY = 1,
																	MAX_PLOT_XY = 4,
																	PLOT_SPACING = 1,
																	MATRIX_RADII = 4;
	
	static constexpr uint32_t const PLOT_SIZE_1x1 = MIN_PLOT_XY,
																	PLOT_SIZE_2x2 = 2,
																	PLOT_SIZE_3x3 = 3,
																	PLOT_SIZE_4x4 = MAX_PLOT_XY;
	
	static constexpr uint32_t const NUM_DISTINCT_BUILDING_HEIGHTS = 7; // forget about zero
	static constexpr uint32_t const BUILDING_HEIGHT_NOISE_STEP = 224 / NUM_DISTINCT_BUILDING_HEIGHTS;		// maximumize percentile range of value noise
																																																			// ~224 to ~28, 224 fits nicely
	// Corresponding Height Step in Pixel units
	static constexpr float const SCALING_FACTOR_BUILDING = 5.6f;
	static constexpr uint32_t const HEIGHT_FACTOR_BUILDING = (Iso::GRID_RADII) * (NUM_DISTINCT_BUILDING_HEIGHTS - 1);
	
	static constexpr uint32_t const BUILDING_HEIGHT_NOISE_7 = UINT8_MAX - BUILDING_HEIGHT_NOISE_STEP,		// These values are for
																	BUILDING_HEIGHT_NOISE_6 = BUILDING_HEIGHT_NOISE_7 - BUILDING_HEIGHT_NOISE_STEP,		// height testing of generated noise only
																	BUILDING_HEIGHT_NOISE_5 = BUILDING_HEIGHT_NOISE_6 - BUILDING_HEIGHT_NOISE_STEP,
																	BUILDING_HEIGHT_NOISE_4 = BUILDING_HEIGHT_NOISE_5 - BUILDING_HEIGHT_NOISE_STEP,
																	BUILDING_HEIGHT_NOISE_3 = BUILDING_HEIGHT_NOISE_4 - BUILDING_HEIGHT_NOISE_STEP,	
																	BUILDING_HEIGHT_NOISE_2 = BUILDING_HEIGHT_NOISE_3 - BUILDING_HEIGHT_NOISE_STEP,
																	BUILDING_HEIGHT_NOISE_1 = BUILDING_HEIGHT_NOISE_2 - BUILDING_HEIGHT_NOISE_STEP;
																	

template< uint32_t const HeightStep >
static constexpr uint32_t const getBuildingHeight_InPixels()
{
	// t * t * (3.0f - 2.0f * t) 
	constexpr float fStepNormalized = (float)HeightStep / ((float)HEIGHT_FACTOR_BUILDING / (float)SCALING_FACTOR_BUILDING);
	return( HEIGHT_FACTOR_BUILDING * (fStepNormalized * fStepNormalized * ( 3.0f - 2.0f * fStepNormalized )) );
}

NOINLINE void Init( uint32_t const tNow );
	
// Only Grid Space (-x,-y) to (X, Y) Coordinates 
uint32_t const GenPlotSizeForLocation( point2D_t const vPoint );
	
NOINLINE bool const GenPlot( point2D_t& minIndex, point2D_t& maxIndex, point2D_t const closestToIndex, point2D_t const Size );

NOINLINE void UpdateTracking(uint32_t const tNow);

// Only Grid Space (-x,-y) to (X, Y) Coordinates 
NOINLINE void GenPlotStructure( uint32_t const tNow, point2D_t const minIndex, point2D_t const maxIndex );

}

#endif

