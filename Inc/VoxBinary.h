/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

The VOX File format is Copyright to their respectful owners.

 */

#ifndef VOX_BINARY_H
#define VOX_BINARY_H

#include "math_3d.h"
#include "shade_ops.h"
#include "IsoVoxel.h"
#include "voxelModel.h"

#include <vector>

#ifdef VOX_DEBUG_ENABLED
#include "debug.cpp"
#endif
	

namespace Volumetric
{
namespace voxB
{
	// see voxelModel.h
	
// builds the voxel model, loading from magickavoxel .vox format, returning the model with the voxel traversal
// supporting 16x16x16 (4KB) size voxel model.
NOINLINE bool const Load( voxelModelBase* const __restrict pDestMem, uint8_t const * const pSourceVoxBinaryData, uint8_t*& __restrict FRAMWritePointer );

} // end namespace voxB

// ## forward declarations first
template<Shading::shade_op_default OPTop, Shading::shade_op_default_normal OPSides, uint32_t const TargetBuffer, uint32_t const RenderingFlags>
STATIC_INLINE void RenderVoxelModel( vec2_t const vObjectOrigin, vec2_rotation_t const vR, voxB::voxelModel<voxB::DYNAMIC> const* const __restrict pModel, float const fAdditionalHeight = 0.0f);																																																

template<Shading::shade_op_default OP, uint32_t const TargetBuffer, uint32_t const RenderingFlags>
STATIC_INLINE void RenderVoxelModel( vec2_t const vObjectOrigin, voxB::voxelModel<voxB::STATIC> const* const __restrict pModel);

STATIC_INLINE __attribute__((pure)) bool const isVoxModelInstanceNotVisible( vec2_t vOrigin, float const fRadius );
} // end namespace

// define VOX_IMPL before inclusion of this header VoxBinary.h if implementation is required 
// (resolves circular dependency of world.h) //
#ifdef VOX_IMPL
#include "world.h"

namespace Volumetric
{
namespace internal
{
	
	__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE vec3_t const getPlotPosition(vec3_t const maxDimensions, vec3_t const maxDimensionsInv,
																																																	float const fScale,
																																																  float& __restrict fNormalizedHeightOffset,
																																																	voxB::voxelDescPacked const* const __restrict pVoxel)
	{
		// Use relative coordinates in signed fashion so origin is forced to middle of model
		vec3_t vPlotGridSpace(pVoxel->getPosition());
		// normalize relative coordinates
		vPlotGridSpace = v3_mul(vPlotGridSpace, maxDimensionsInv);
		fNormalizedHeightOffset = vPlotGridSpace.z; // used for lighting height calculation must be [0.0 ... 1.0]
		
		// change from [0.0f ... 1.0f] tp [-1.0f ... 1.0f]
		vPlotGridSpace = v3_subs(vPlotGridSpace, 0.5f);
		vPlotGridSpace = v3_muls(vPlotGridSpace, 2.0f);
		
		// now scale by dimension size 
		vPlotGridSpace = v3_mul(vPlotGridSpace, maxDimensions);
		float const fGridScale = fScale * Iso::VERY_TINY_GRID_HALF_SCALE;
		vPlotGridSpace.x *= fGridScale;
		vPlotGridSpace.y *= fGridScale;
		//vPlotGridSpace.z *= Iso::VERY_TINY_GRID_SCALE;
		// vPlotGridSpace.z at this point contains the relative height offset
		
		return(vPlotGridSpace);
	}
	
} // end namespace internal
	
// ######### static inline definitions only
template<Shading::shade_op_default OPTop, Shading::shade_op_default_normal OPSides, uint32_t const TargetBuffer, uint32_t const RenderingFlags>
STATIC_INLINE void RenderVoxelModel( vec2_t const vObjectOrigin, vec2_rotation_t const vR, voxB::voxelModel<voxB::DYNAMIC> const* const __restrict pModel, float const fAdditionalHeight )
{
	voxB::voxelDescPacked const* __restrict pTraversal( /*pModel->Voxels.data()*/ pModel->VoxelsFRAM );
	uint32_t numTraverse( pModel->numVoxels );
	vec3_t const maxDimensions(pModel->maxDimensions), maxDimensionsInv(pModel->maxDimensionsInv);
	float const modelHeightOffset(maxDimensions.z + fAdditionalHeight),
							fScale(pModel->Scalar);
	
	point2D_t rotationX, rotationY;
	
	{ // rotation is agnostic to coordinate space, can be used for all voxels of model
		// rotation is done using floating point, the accurary of the rotation increases tenfold, especially for "small" voxels animation		
		vec2_rotation_t const vRNegated(-vR - RotationConstants.v45); // rotation is off 45 degrees
		float const fRadiiScaled = fScale * Iso::VERY_TINY_GRID_FRADII;
		rotationX = v2_to_p2D_rounded(v2_rotate_screenspace(vRNegated, vec2_t( fRadiiScaled, 0.0f )));
		rotationY = v2_to_p2D_rounded(v2_rotate_screenspace(vRNegated, vec2_t( 0.0f, fRadiiScaled)));
	}
	
	do
	{
		float fNormalizedHeightOffset;
		
		vec3_t vPlotRelative = internal::getPlotPosition(maxDimensions, maxDimensionsInv, fScale, fNormalizedHeightOffset, pTraversal);
		
		// currently cordinates are "normalized relative units", can be fractional 
		// transform by chosen 2D World Coordinates, will now be grid space
		
		// rotate point (skipping subtract origin, rotate, then add origin back in)
		vec2_t vPlotIsometric = v2_rotate(vR, vec2_t(vPlotRelative));
		vPlotIsometric = v2_add( vPlotIsometric, vObjectOrigin );
		
		Iso::Voxel const* pVoxelFound(nullptr);
	
		if ( nullptr != (pVoxelFound = world::getVoxel_IfVisible(vPlotIsometric)) )	// only within bounds of world and if visible onscreen 
		{
			// Transform from GridSpace to ScreenSpace
			world::RenderTinyVoxel_Complex<OPTop, OPSides, TargetBuffer, RenderingFlags>
																		( fNormalizedHeightOffset, vPlotRelative.z - modelHeightOffset, fScale,
																			pTraversal->getAdjAndShade(),
																		  v2_to_p2D_rounded(  world::v2_GridToScreen( vPlotIsometric ) ), 
																			rotationX, rotationY );
		}
		
		++pTraversal;
		
	} while ( 0 != --numTraverse );

}

template<Shading::shade_op_default OP, uint32_t const TargetBuffer, uint32_t const RenderingFlags>
	STATIC_INLINE void RenderVoxelModel( vec2_t const vObjectOrigin, voxB::voxelModel<voxB::STATIC> const* const __restrict pModel)
{
	voxB::voxelDescPacked const* __restrict pTraversal( /*pModel->Voxels.data()*/ pModel->VoxelsFRAM );
	uint32_t numTraverse( pModel->numVoxels );
	vec3_t const maxDimensions(pModel->maxDimensions), maxDimensionsInv(pModel->maxDimensionsInv);
	float const modelHeightOffset(maxDimensions.z),
							fScale(pModel->Scalar);
	
	do
	{
		float fNormalizedHeightOffset;
		
		vec3_t vPlotRelative = internal::getPlotPosition(maxDimensions, maxDimensionsInv, fScale, fNormalizedHeightOffset, pTraversal);
		
		// currently cordinates are "normalized relative units", can be fractional 
		// transform by chosen 2D World Coordinates, will now be grid space
		
		// rotate point (skipping subtract origin, rotate, then add origin back in)
		vec2_t vPlotIsometric = v2_add( vPlotRelative, vObjectOrigin );
		
		Iso::Voxel const* pVoxelFound(nullptr);
	
		if ( nullptr != (pVoxelFound = world::getVoxel_IfVisible(vPlotIsometric)) )	// only within bounds of world and if visible onscreen 
		{
			// Transform from GridSpace to ScreenSpace		
			world::RenderTinyVoxel_Static<OP, TargetBuffer, RenderingFlags>
																		( fNormalizedHeightOffset, vPlotRelative.z - modelHeightOffset,
																			pTraversal->getAdjAndShade(),
																		  v2_to_p2D( world::v2_GridToScreen( vPlotIsometric ) ) );
		}
		
		++pTraversal;
		
	} while ( 0 != --numTraverse );

}

STATIC_INLINE __attribute__((pure)) bool const isVoxModelInstanceNotVisible( vec2_t vOrigin, float const fRadius )
{
	vec2_t const vExtent = world::v2_GridToScreen( v2_adds(vOrigin, fRadius) );
	vOrigin = world::v2_GridToScreen( vOrigin );
	
	float const DistanceSquared = v2_distanceSquared( v2_sub(vExtent, vOrigin) );
		
	// approximation, floored location while radius is ceiled, overcompensated, to use interger based visibility test
	return( OLED::TestStrict_Not_OnScreen( v2_to_p2D(vOrigin), int32::__ceilf(DistanceSquared) ) );
}

} // end namespace Volumetric
#endif // VOX_IMPL
#endif // VOX_BINARY_H
