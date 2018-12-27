/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef VOLUMETRICRADIALGRID_H
#define VOLUMETRICRADIALGRID_H

#include "commonmath.h"
#include "math_3d.h"
#include "point2D.h"
#include "IsoVoxel.h"
#include "oled.h"
#include "world.h"

#include <float.h>
#include <vector>

#include "debug.cpp"
//#define DEBUG_RANGE 

namespace Volumetric
{
	static constexpr uint32_t const NO_OPTIONS = 0,
																	RESERVED = (1 << 0),
																	CULL_EXPANDING = (1 << 1),
																	CULL_INNERCIRCLE_GROWING = (1 << 2),
																	FOLLOW_GROUND_HEIGHT = (1 << 3);
	
	static constexpr float const MAXIMUM_HEIGHT_FPIXELS = (float)(Iso::TINY_GRID_RADII << 2);
	
	typedef struct sxRow
	{
																																																				// Length Variable optimized out
		__attribute__((pure)) inline bool const operator<(sxRow const& rhs) const														// is identity, always absolute(2 * x)
		{																																						  											// before: vecRows.push_back( xRow(-x, y, x * 2)); // after: vecRows.push_back( xRow(-x, y));
				return( Start.y > rhs.Start.y );		 
		}

		vec2_t	 	Start;
		float     RangeLeft, RangeRight;

		sxRow(float const x, float const y)
			: Start(x, y), RangeLeft(0.0f), RangeRight(0.0f)
		{}

	} xRow;
	
	typedef struct sRadialGridInstance
	{
		__attribute__((pure)) inline bool const  isInvalidated() const {
			return(Invalidated);
		}
		
		__attribute__((pure)) inline uint32_t const  getLifetime() const {
			return(tLife);
		}
		__attribute__((pure)) inline bool const  isDead() const {
			return(0 == tLife);
		}
		
		__attribute__((pure)) inline float const getLocalTime() const {
			return(tLocal);
		}
				
		__attribute__((pure)) inline vec2_t const  getOrigin() const {
			return(Origin);
		}

		__attribute__((pure)) inline float const  getRadius() const {
			return(CurrentRadius);
		}

		__attribute__((pure)) inline float const  getInvRadius() const {
			return(InvCurrentRadius);
		}
		
		inline void setRadius(float const newRadius) {
			CurrentRadius = newRadius; InvCurrentRadius = 1.0f/newRadius; Invalidated = true;
		}
		
		inline void resetInvalidated() {
			Invalidated = false;
		}  
		
		inline bool const UpdateLocalTime(uint32_t const tNow) // returns true (alive), false (dead) 
		{
			if ( 0 != tLastUpdate && tNow > tLastUpdate ) {
				uint32_t const tDelta(tNow - tLastUpdate);
				
				tLife -= tDelta;
				
				if (tLife <= 0) {
					tLife = 0;
					return(false);
				}
				else {
					float const ftDelta( Constants::ConvTimeToFloat * (float)tDelta );
					tLocal += ftDelta;	// done this way to always be forward, no ping-ponging possible
				}
				
			}
			
			tLastUpdate = tNow;
			
			return(true);
		}
		
		bool const Update(uint32_t const tNow);	// returns true (alive), false (dead)
		
		sRadialGridInstance( vec2_t const WorldCoordOrigin, float const Radius, uint32_t const tLifeMS,
												 std::vector<Volumetric::xRow> const& __restrict Rows )
			: Origin(WorldCoordOrigin), CurrentRadius(Radius), InvCurrentRadius(1.0f/Radius), 
				tLife(tLifeMS), tLastUpdate(0),
				tLocal(0.0f), Invalidated(true),
				InstanceRows(Rows)
#ifdef DEBUG_RANGE
			, RangeMin(FLT_MAX), RangeMax(-FLT_MAX)
#endif
		{}
			
	public:
		std::vector<Volumetric::xRow> const& __restrict InstanceRows;
#ifdef DEBUG_RANGE
		float RangeMin, RangeMax;
#endif
	private:
		vec2_t const				Origin;
		float								CurrentRadius,
												InvCurrentRadius,
												tLocal;
		uint32_t 			 			tLastUpdate;
		int32_t							tLife;
		bool								Invalidated;
	} RadialGridInstance;
	
	STATIC_INLINE_PURE float const getRowLength(float const xDisplacement) { return(__fabsf(xDisplacement * 2.0f)); }
	
	// Template inline function pointers!!!
	
	// op CAN READ BUT NOT MODIFY GLOBAL MEMORY, including Volumetric::RadialGridInstance Instance
	// if Volumetric::RadialGridInstance Instance is not used/touched underlying op can be :
	// STATIC_ONLINE_PURE ( __attribute__((const)) __STATIC_INLINE)
	// WHICH MEANS NO global MEMORY CAN BE ACCESSED READ OR WRITE AT ALL
	using sdf_op = float const (* const)(vec2_t const, float const, Volumetric::RadialGridInstance const* const __restrict); // signature for all valid template params
	template<sdf_op op>
	__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE float const do_sdf_op(vec2_t const vDisplacement, float const tLocal, Volumetric::RadialGridInstance const* const __restrict Instance)
	{
		return( op(vDisplacement, tLocal, Instance) );
	}

	// public declarations only 
	NOINLINE void radialgrid_generate(float const radius, std::vector<xRow>& __restrict vecRows);
	__attribute__((always_inline)) STATIC_INLINE __attribute__((pure)) bool const isRadialGridNotVisible( vec2_t vOrigin, float const fRadius );
	
	template<Volumetric::sdf_op op, Shading::shade_op_min OP, uint32_t const Options = Volumetric::NO_OPTIONS, uint32_t const TargetBuffer = OLED::BACK_BUFFER, uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE)>
	__attribute__((always_inline)) STATIC_INLINE void renderRadialGrid( float const fAmplitude, Volumetric::RadialGridInstance const* const __restrict radialGrid );

} // end namespace



// ######### private static inline definitions only

template<Volumetric::sdf_op op, Shading::shade_op_min OP, uint32_t const Options, uint32_t const TargetBuffer, uint32_t const RenderingFlags>
STATIC_INLINE bool const renderVoxel(vec2_t const vDisplacement, float const tLocal, Volumetric::RadialGridInstance const* const __restrict radialGrid)
{	
	vec2_t const vObjectOrigin( radialGrid->getOrigin() );
	vec2_t const vPlotGridSpace( v2_add(vObjectOrigin, vDisplacement) );
	
	Iso::Voxel const* pVoxelFound(nullptr);
	
	if ( nullptr != (pVoxelFound = world::getVoxel_IfVisible(vPlotGridSpace)) )
	{		
		float const fHeightDistanceNormalized = Volumetric::do_sdf_op<op>(vDisplacement, tLocal, radialGrid);
#ifdef DEBUG_RANGE
		const_cast<Volumetric::RadialGridInstance* const __restrict>(radialGrid)->RangeMin = __fminf(radialGrid->RangeMin, fHeightDistanceNormalized);
		const_cast<Volumetric::RadialGridInstance* const __restrict>(radialGrid)->RangeMax = __fmaxf(radialGrid->RangeMax, fHeightDistanceNormalized);
#endif

		if ( fHeightDistanceNormalized > 0.0f ) {
			uint32_t uiPixelHeight = uint32::__roundf(fHeightDistanceNormalized * Volumetric::MAXIMUM_HEIGHT_FPIXELS);
			
			if ( 0 != uiPixelHeight ) {
				
				Iso::Voxel const oVoxelFound(*pVoxelFound); // Read
				
				if constexpr ( Volumetric::FOLLOW_GROUND_HEIGHT == (Volumetric::FOLLOW_GROUND_HEIGHT & Options) ) // statically evaluated @ compile time
				{
					if ( isGround(oVoxelFound) )
					{
						uiPixelHeight += (world::getGroundHeight_InPixels(getHeightStep(oVoxelFound)));
					}
				}
				
				// Transform from GridSpace to ScreenSpace		
				world::RenderTinyVoxel<OP, TargetBuffer, RenderingFlags>
															(fHeightDistanceNormalized,
															uiPixelHeight, 
															v2_to_p2D(  world::v2_GridToScreen( vPlotGridSpace ) ),
															v2_to_p2D(  world::v2_GridToScreen( vObjectOrigin ) ) );
				
				return(true);
			}
		}
	}
	return(false);
}

template<Volumetric::sdf_op op, Shading::shade_op_min OP, uint32_t const Options, uint32_t const TargetBuffer, uint32_t const RenderingFlags>
__attribute__((always_inline)) STATIC_INLINE void renderVoxelRow_CullInnerCircleGrowing( Volumetric::xRow const* const __restrict pCurRow, float const tLocal,
																																	                       Volumetric::RadialGridInstance const* const __restrict radialGrid )
{
	vec2_t vDisplacement(pCurRow->Start);
	float fCurRangeLeft(pCurRow->RangeLeft), fCurRangeRight(pCurRow->RangeRight);
		
	float RunWidth = Volumetric::getRowLength(vDisplacement.x);
		
	do
	{
			if (vDisplacement.x <= fCurRangeLeft || vDisplacement.x >= fCurRangeRight) { // Growing
				if ( renderVoxel<op, OP, Options, TargetBuffer, RenderingFlags>(vDisplacement, tLocal, radialGrid) ) {
					
					if ( vDisplacement.x < 0.0f ) { 
						if ( 0.0f == fCurRangeLeft )
							fCurRangeLeft = vDisplacement.x;
						else
							fCurRangeLeft = __fmaxf(fCurRangeLeft, vDisplacement.x);
					}
					else {
						if ( 0.0f == fCurRangeRight )
							fCurRangeRight = vDisplacement.x;
						else
							fCurRangeRight = __fminf(fCurRangeRight, vDisplacement.x);
					}
				}
				
				vDisplacement.x += Iso::TINY_GRID_SCALE;
				RunWidth -= Iso::TINY_GRID_SCALE;
			}
			else { // Inside the "inner circle" culling zone
				// Move Displacement to the right side
				RunWidth -= (fCurRangeRight - vDisplacement.x);
				vDisplacement.x = fCurRangeRight;
			}

	} while ( RunWidth >= 0.0f );
	
	// Finish with storing updated range for this row
	const_cast<Volumetric::xRow* const __restrict>(pCurRow)->RangeLeft = fCurRangeLeft;
	const_cast<Volumetric::xRow* const __restrict>(pCurRow)->RangeRight = fCurRangeRight;
}

template<Volumetric::sdf_op op, Shading::shade_op_min OP, uint32_t const Options, uint32_t const TargetBuffer, uint32_t const RenderingFlags>
__attribute__((always_inline)) STATIC_INLINE void renderVoxelRow_CullExpanding( Volumetric::xRow const* const __restrict pCurRow, float const tLocal,
																																	              Volumetric::RadialGridInstance const* const __restrict radialGrid )
{
	vec2_t vDisplacement(pCurRow->Start);

	float const HalfWidth = Volumetric::getRowLength(vDisplacement.x) * 0.5f;
	float const SymmetricStart = vDisplacement.x + HalfWidth;
	
	{
		float RunHalfWidth(HalfWidth);
		vDisplacement.x = SymmetricStart;
		// Render LeftSide until renderVoxel returns false starting from middle
		do
		{
			if ( !renderVoxel<op, OP, Options, TargetBuffer, RenderingFlags>(vDisplacement, tLocal, radialGrid) )
				break;	// only rendering expanding volume, stop rendering side once a zero height/undefined voxel is hit
			
			vDisplacement.x -= Iso::TINY_GRID_SCALE;	// Middle to left
			RunHalfWidth -= Iso::TINY_GRID_SCALE;
		} while (RunHalfWidth >= 0.0f);
	}
	
	{
		float RunHalfWidth(HalfWidth);
		vDisplacement.x = SymmetricStart + Iso::TINY_GRID_SCALE;	// start right side offset by one tiny voxel to not render same voxel twice
		// Render RightSide until renderVoxel returns false starting from middle
		do
		{
			if ( !renderVoxel<op, OP, Options, TargetBuffer, RenderingFlags>(vDisplacement, tLocal, radialGrid) )
				break;	// only rendering expanding volume, stop rendering side once a zero height/undefined voxel is hit
			
			vDisplacement.x += Iso::TINY_GRID_SCALE;	// Middle to right
			RunHalfWidth -= Iso::TINY_GRID_SCALE;
		} while (RunHalfWidth >= 0.0f);
	}
}

template<Volumetric::sdf_op op, Shading::shade_op_min OP, uint32_t const Options, uint32_t const TargetBuffer, uint32_t const RenderingFlags>
__attribute__((always_inline)) STATIC_INLINE void renderVoxelRow_Brute( Volumetric::xRow const* const __restrict pCurRow, float const tLocal,
																																	      Volumetric::RadialGridInstance const* const __restrict radialGrid )
{
	vec2_t vDisplacement(pCurRow->Start);
	float fCurRangeLeft(pCurRow->RangeLeft), fCurRangeRight(pCurRow->RangeRight);
		
	float RunWidth = Volumetric::getRowLength(vDisplacement.x);
		
	do
	{
			renderVoxel<op, OP, Options, TargetBuffer, RenderingFlags>(vDisplacement, tLocal, radialGrid);
		
			vDisplacement.x += Iso::TINY_GRID_SCALE;
			RunWidth -= Iso::TINY_GRID_SCALE;

	} while ( RunWidth >= 0.0f );
}
	


namespace Volumetric
{
	// public static inline definitions only
	
	__attribute__((always_inline)) STATIC_INLINE __attribute__((pure)) bool const isRadialGridNotVisible( vec2_t vOrigin, float const fRadius )
	{
		vec2_t const vExtent = world::v2_GridToScreen( v2_adds(vOrigin, fRadius) );
		vOrigin = world::v2_GridToScreen( vOrigin );
		
		float const DistanceSquared = v2_distanceSquared( v2_sub(vExtent, vOrigin) );
			
		// approximation, floored location while radius is ceiled, overcompensated, to use interger based visibility test
		return( OLED::TestStrict_Not_OnScreen( v2_to_p2D(vOrigin), int32::__ceilf(DistanceSquared) ) );
	}
	
	template<Volumetric::sdf_op op, Shading::shade_op_min OP, uint32_t const Options, uint32_t const TargetBuffer, uint32_t const RenderingFlags>
	__attribute__((always_inline)) STATIC_INLINE void renderRadialGrid( Volumetric::RadialGridInstance const* const __restrict radialGrid )
	{
		xRow const* __restrict pCurRow;
		uint32_t NumRows;
		
		{
			std::vector<Volumetric::xRow> const& __restrict Rows(radialGrid->InstanceRows);
			
			// Fastest way to iterate over a vector!
			NumRows = Rows.size();
			pCurRow = Rows.data();
		}
		
		float const tLocal(radialGrid->getLocalTime());
		
		while( 0 != NumRows ) {
			
			// statically evaluated @ compile time
			if constexpr ( Volumetric::CULL_EXPANDING == (Volumetric::CULL_EXPANDING & Options) ) {								// expanding volumes ie explosion
				renderVoxelRow_CullExpanding<op, OP, Options, TargetBuffer, RenderingFlags>( pCurRow, tLocal, radialGrid );
			}
			else if constexpr ( Volumetric::CULL_INNERCIRCLE_GROWING == (Volumetric::CULL_INNERCIRCLE_GROWING & Options) ) { // growing volume with inside skippped as it grows ie shockwave
				renderVoxelRow_CullInnerCircleGrowing<op, OP, Options, TargetBuffer, RenderingFlags>( pCurRow, tLocal, radialGrid );
			}
			else {
				renderVoxelRow_Brute<op, OP, Options, TargetBuffer, RenderingFlags>( pCurRow, tLocal, radialGrid );
			}
			
			++pCurRow;
			--NumRows;
		}
	}
} // end namespace

#endif


