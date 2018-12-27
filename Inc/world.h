/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef WORLD_H
#define WORLD_H
#include "globals.h"
#include "point2D.h"
#include "math_3d.h"
#include "isoVoxel.h"
#include "shade_ops.h"
#include "oled.h"
#include "Lighting.h"
#include "voxelModel.h"
#include "VoxBinary.h" // no implementation

class cAIBot;// forward declaration

namespace world
{
static constexpr uint32_t const NUM_DISTINCT_GROUND_HEIGHTS = 7; // forget about zero
static constexpr uint32_t const GROUND_HEIGHT_NOISE_STEP = (UINT8_MAX - 129) / NUM_DISTINCT_GROUND_HEIGHTS;			// is a good value for more "flat" landmass		
	
// Corresponding Height Step in Pixel units
static constexpr float const SCALING_FACTOR_GROUND = 1.8f;
static constexpr uint32_t const HEIGHT_FACTOR_GROUND = (Iso::GRID_RADII / 4) * NUM_DISTINCT_GROUND_HEIGHTS;

static constexpr uint32_t const GROUND_HEIGHT_NOISE_7 = UINT8_MAX - GROUND_HEIGHT_NOISE_STEP,		// These values are for
																GROUND_HEIGHT_NOISE_6 = GROUND_HEIGHT_NOISE_7 - GROUND_HEIGHT_NOISE_STEP,		// height testing of generated noise only
																GROUND_HEIGHT_NOISE_5 = GROUND_HEIGHT_NOISE_6 - GROUND_HEIGHT_NOISE_STEP,
																GROUND_HEIGHT_NOISE_4 = GROUND_HEIGHT_NOISE_5 - GROUND_HEIGHT_NOISE_STEP,
																GROUND_HEIGHT_NOISE_3 = GROUND_HEIGHT_NOISE_4 - GROUND_HEIGHT_NOISE_STEP,	
																GROUND_HEIGHT_NOISE_2 = GROUND_HEIGHT_NOISE_3 - GROUND_HEIGHT_NOISE_STEP,
																GROUND_HEIGHT_NOISE_1 = GROUND_HEIGHT_NOISE_2 - GROUND_HEIGHT_NOISE_STEP;

template< uint32_t const HeightStep >
static constexpr uint32_t const getGroundHeight_InPixels()
{
	// t * t * (3.0f - 2.0f * t) 
	constexpr float fStepNormalized = (float)HeightStep / ((float)HEIGHT_FACTOR_GROUND / (float)SCALING_FACTOR_GROUND);
	return( HEIGHT_FACTOR_GROUND * (fStepNormalized * fStepNormalized * ( 3.0f - 2.0f * fStepNormalized )) );
}
uint32_t const getGroundHeight_InPixels(uint32_t const HeightStep);
uint32_t const getBuildingHeight_InPixels(uint32_t const HeightStep);

// Corresponding Height in Pixel units
static constexpr uint32_t const MAX_GROUND_HEIGHT = getGroundHeight_InPixels<NUM_DISTINCT_GROUND_HEIGHTS>();
static constexpr uint32_t const DEFAULT_FOG_HEIGHT_PIXELS = getGroundHeight_InPixels<NUM_DISTINCT_GROUND_HEIGHTS - 1>();

static constexpr uint32_t const NUM_AIBOTS = 1;
static constexpr uint32_t const TURN_INTERVAL = 2200; //ms

#define WORLD_CENTER (vec2_t(0.0f, 0.0f))
#define WORLD_TOP (vec2_t(0.0f, -Iso::WORLD_GRID_FHALFSIZE))
#define WORLD_BOTTOM (vec2_t(0.0f, Iso::WORLD_GRID_FHALFSIZE))
#define WORLD_LEFT (vec2_t(-Iso::WORLD_GRID_FHALFSIZE, 0.0f))
#define WORLD_RIGHT (vec2_t(Iso::WORLD_GRID_FHALFSIZE, 0.0f))
#define WORLD_TL (vec2_t(-Iso::WORLD_GRID_FHALFSIZE, -Iso::WORLD_GRID_FHALFSIZE))
#define WORLD_TR (vec2_t(Iso::WORLD_GRID_FHALFSIZE, -Iso::WORLD_GRID_FHALFSIZE))
#define WORLD_BL (vec2_t(-Iso::WORLD_GRID_FHALFSIZE, Iso::WORLD_GRID_FHALFSIZE))
#define WORLD_BR (vec2_t(Iso::WORLD_GRID_FHALFSIZE, Iso::WORLD_GRID_FHALFSIZE))
	
NOINLINE void Init( uint32_t const tNow );
NOINLINE void DeferredInit( uint32_t const tNow );
bool const isDeferredInitComplete();

void Update( uint32_t const tNow );

void Render( uint32_t const tNow );

// ########## Camera
__attribute__((pure)) uint32_t const& __restrict getCameraState();
__attribute__((pure)) point2D_t const p2D_GridToScreen(point2D_t thePt);
__attribute__((pure)) vec2_t const    v2_GridToScreen(vec2_t thePt);

// Therefore the neighbours of a voxel, follow same isometric layout/order //
/*

											NBR_TL
							NBR_L						NBR_T
		 NRB_BL						VOXEL						NBR_TR
							NBR_B						NRB_R		
											NBR_BR
											
*/
static constexpr uint32_t const NBR_TL(0),			// use these indices into ADJACENT[] to get a relativeOffset
															  NBR_T (1),			// or iterate thru a loop of ADJACENT_NEIGHBOUR_COUNT with
															  NBR_TR(2),			// the loop index into the ADJACENT[] 
															  NBR_R (3),
															  NBR_BR(4),
															  NBR_B (5),
															  NBR_BL(6),
															  NBR_L (7);

static constexpr uint32_t const ADJACENT_NEIGHBOUR_COUNT = 8;
extern point2D_t const ADJACENT[ADJACENT_NEIGHBOUR_COUNT];

// Grid Space (-x,-y) to (X, Y) Coordinates Only
__attribute__((pure)) Iso::Voxel const* const __restrict getNeighbour(point2D_t voxelIndex, point2D_t const relativeOffset);

// Grid Space (-x,-y) to (X, Y) Coordinates Only
__attribute__((pure)) Iso::Voxel const * const __restrict getVoxelAt( point2D_t voxelIndex );
__attribute__((pure)) Iso::Voxel const * const __restrict getVoxelAt( vec2_t const Location );

// Grid Space (-x,-y) to (X, Y) Coordinates Only
bool const setVoxelAt( point2D_t voxelIndex, Iso::Voxel const newData );
bool const setVoxelAt( vec2_t const Location, Iso::Voxel const newData );

// Grid Space (-x,-y) to (X, Y) Coordinates Only
__attribute__((pure)) bool const isPointNotInVisibleVoxel( vec2_t const Location );
__attribute__((pure)) STATIC_INLINE Iso::Voxel const* const __restrict getVoxel_IfVisible( vec2_t const Location );

// Grid Space (-x,-y) to (X, Y) Coordinates Only
__attribute__((pure)) bool const getVoxelDepth( vec2_t const Location, 
																								Iso::Voxel const* __restrict& __restrict pVoxelOut, int32_t& __restrict Depth );

// Grid Space (-x,-y) to (X, Y) Coordinates Only - Center of Screen in GridSpace
__attribute__((pure)) vec2_t const& __restrict getOrigin();

// Grid Space (-x,-y) to (X, Y) Coordinates Only
bool const moveTo( vec2_t const Dest, uint32_t const tMove );
void follow( vec2_t const* const __restrict pvTarget, uint32_t const tUpdate );
void resetFollow();

__attribute__((pure)) cAIBot const& getPendingTurnAIBot();

// In Iso::Voxel Heightstep increments, eg.) 0, 1, 3, 6, 7 etc, not pixel values
__attribute__((pure)) uint32_t const getFogLevel();
void setFogLevel(uint32_t const uiFogHeightInPixels); 

Volumetric::voxB::voxelModelInstance_Dynamic* const __restrict getActiveMissile();
Volumetric::voxB::voxelModelInstance_Dynamic* const __restrict CreateMissile( uint32_t const tNow, vec2_t const WorldCoord, vec2_t const WorldCoordTarget );
void CreateExplosion( uint32_t const tNow, vec2_t const WorldCoord, bool const bShockwave = true );

// ScreenSpace Coordinates Only
// This test does not take into effect of height of the bottom rows of voxels
 STATIC_INLINE_PURE bool const TestPoint_Not_OnScreen(point2D_t const oTest) 
{
	// zero based test
	point2D_t const screenmin (Iso::GRID_RADII_X2, Iso::GRID_RADII),
									screenmax (OLED::SCREEN_WIDTH + Iso::GRID_RADII_X2, OLED::SCREEN_HEIGHT + Iso::GRID_RADII);
		
	point2D_t result;
	
	// zero based tests (performance)
	
	// Test case, outside bounds left | top
	result = p2D_add(oTest, screenmin);
	
	// If either component is negative, the point was greater than screen bounds
	if ( (result.pt.x | result.pt.y) >= 0 ) { //test the sign bit if it exists
		
		// Continue and verify not greater than screen bounds
		
		// Test case, outside bounds right | bottom
		result = p2D_sub(screenmax, oTest);
		
		// If either component is negative, the point was greater than screen bounds
		//test the sign bit if it exists
		return((result.pt.x | result.pt.y) < 0); // returning zero for successfully tested, point is onscreen
	}
		
	return(true); 
}
 STATIC_INLINE_PURE bool const TestPoint_Not_OnScreen(point2D_t const oTest, uint32_t const MaxHeight) 
{
	// zero based test
	point2D_t const screenmin (Iso::GRID_RADII_X2, Iso::GRID_RADII),
									screenmax (OLED::SCREEN_WIDTH + Iso::GRID_RADII_X2, OLED::SCREEN_HEIGHT + Iso::GRID_RADII + MaxHeight);
		
	point2D_t result;
	
	// zero based tests (performance)
	
	// Test case, outside bounds left | top
	result = p2D_add(oTest, screenmin);
	
	// If either component is negative, the point was greater than screen bounds
	if ( (result.pt.x | result.pt.y) >= 0 ) { //test the sign bit if it exists
		
		// Continue and verify not greater than screen bounds
		
		// Test case, outside bounds right | bottom
		result = p2D_sub(screenmax, oTest);
		
		// If either component is negative, the point was greater than screen bounds
		//test the sign bit if it exists
		return((result.pt.x | result.pt.y) < 0); // returning zero for successfully tested, point is onscreen
	}
		
	return(true); 
}

// Grid Space (-x,-y) to (X, Y) Coordinates Only
__attribute__((pure)) STATIC_INLINE Iso::Voxel const* const __restrict getVoxel_IfVisible( vec2_t const Location )
{
	point2D_t const voxelIndex( v2_to_p2D(Location) );		// Whole part in (-x,-y)=>(x,y) GridSpace
	
	Iso::Voxel const * const __restrict pVoxel = getVoxelAt(voxelIndex);
		
	if ( nullptr != pVoxel ) // Test
	{		
		// Same Test that RenderGrid Uses, simpler
		point2D_t const pixelScreenSpace = p2D_GridToScreen(voxelIndex);
		
		if ( false == TestPoint_Not_OnScreen(pixelScreenSpace) ) {
			return(pVoxel);
		}
	}
	return(nullptr);
}

} // end namespace

namespace world
{

// For voxels extruded from ground level - ScreenSpace Coords only
template<Shading::shade_op_min OP, uint32_t const TargetBuffer = OLED::BACK_BUFFER, uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE)>
__attribute__((always_inline)) STATIC_INLINE void RenderTinyVoxel( float const fNormalizedHeight, uint32_t const uiPixelHeight,
																																	 point2D_t const VoxelOrigin, point2D_t const ObjectOrigin )
{
	point2D_t xPoint, yPoint, yPointTopReset;
	
	{
		// get plot corner extents (bottom/top diamond)
		halfdiamond2D_t cornersB;
			
		// get corner extents (bottom diamond)
		{
			point2D_t const ptExtents(0, (Iso::TINY_GRID_RADII)>>1);
			cornersB.pts.t = p2D_sub(VoxelOrigin, ptExtents);
			cornersB.pts.b = p2D_add(VoxelOrigin, ptExtents);
		}
		
		OLED::setDepthLevel( p2D_sub(cornersB.pts.b, p2D_subh(cornersB.pts.b, cornersB.pts.t)).pt.y );  // solves depth for shockwave! yay!!
		
		// get corner extents (top diamond)
		halfdiamond2D_t cornersT;
		{
			point2D_t const ptExtents(0, uiPixelHeight);
			cornersT.pts.t = p2D_sub(cornersB.pts.t, ptExtents);
			cornersT.pts.b = p2D_sub(cornersB.pts.b, ptExtents);
		}
		// Draw Vertical Lines Filling Left side & front side of voxel
		// starting from bottom work to left & RIGHT
		
		xPoint = point2D_t(cornersT.pts.b.pt.x, cornersB.pts.b.pt.x);
		yPoint = point2D_t(cornersT.pts.b.pt.y, cornersB.pts.b.pt.y);
		yPointTopReset = point2D_t(cornersT.pts.t.pt.y, cornersT.pts.t.pt.y);
	}
	
	uint32_t const LumaTiny = Shading::do_shading_op_min<OP>(vec3_t(VoxelOrigin.pt.x, fNormalizedHeight, VoxelOrigin.pt.y), ObjectOrigin);

	for (int iDx = (Iso::TINY_GRID_RADII) - 1, iD = 0; iDx >= 0; iDx--, iD++)
	{
		point2D_t curXPoints, curYPoints;
		point2D_t const ptIncrement(iD >> 1, iD >> 1);
		
		curXPoints = p2D_add(xPoint, point2D_t(-iD, iD));
		curYPoints = p2D_sub(yPoint, ptIncrement);

		{ // scope rooftop
			// Draw Rooftop
			point2D_t yPointTop;

			yPointTop = p2D_add(yPointTopReset, ptIncrement);
			yPointTop = p2D_saturate<OLED::Height_SATBITS>(yPointTop);

			// Leverage left and front(right) x point iterations, and leverage y point aswell for length
			int32_t const iLength = (curYPoints.pt.x - yPointTop.pt.x);
			if (likely(iLength > 0)) {
	
				if ( OLED::CheckVLine(curXPoints.pt.x, yPointTop.pt.x, iLength) ) {
					OLED::DrawVLine<TargetBuffer, RenderingFlags>(curXPoints.pt.x, yPointTop.pt.x, iLength, LumaTiny);
				}
				if ( OLED::CheckVLine(curXPoints.pt.y, yPointTop.pt.x, iLength) ) {
					OLED::DrawVLine<TargetBuffer, RenderingFlags>(curXPoints.pt.y, yPointTop.pt.x, iLength, LumaTiny);
				}
			}
		} // scope rooftop
		
		if ( OLED::CheckVLine_YAxis(curYPoints.pt.x, (curYPoints.pt.y - curYPoints.pt.x)) ) 
		{
			// saturate y axis only to clip lines, x axis bounds are fully checked, y axis must be clipped to zero for line length calculation
			curYPoints = p2D_saturate<OLED::Height_SATBITS>(curYPoints);
			int32_t const iLength = (curYPoints.pt.y - curYPoints.pt.x);
			
			if (likely(iLength > 0))
			{
				// Calculate lighting per vertical line
				// incident light vector
				if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) { // only for single diamond, or left most diamond
						// Left Face
						OLED::DrawVLine<TargetBuffer, RenderingFlags>(curXPoints.pt.x, curYPoints.pt.x, iLength, LumaTiny);
				} // xaxis check
				
				if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
						// Front Face
						OLED::DrawVLine<TargetBuffer, RenderingFlags>(curXPoints.pt.y, curYPoints.pt.x, iLength, LumaTiny);
				} // xaxis check
			}
		} // y axis check
	} // for
}

/*
__attribute__((always_inline)) STATIC_INLINE void RenderVoxel(point2D_t const VoxelOrigin, vec2_rotation const Rotation)
{
	static uint32_t uiRooftopHeight(0);

	// get corner extents (top diamond)
	uint32_t const uiHeight = HEIGHTS[6]; //getHeight(xIndex, zIndex)>>3;

	// get corner extents (bottom diamond)
	diamond2D_t cornersB;

	cornersB.pts.l = p2D_sub(Origin, point2D_t(GRID_RADII, 0));
	cornersB.pts.r = p2D_add(Origin, point2D_t(GRID_RADII, 0));
	cornersB.pts.t = p2D_sub(Origin, point2D_t(0, GRID_RADII >> 1));
	cornersB.pts.b = p2D_add(Origin, point2D_t(0, GRID_RADII >> 1));

	diamond2D_t cornersBR;
	cornersBR.pts.l = p2D_add(Iso::CartToISO(p2D_rotate(Rotation, Origin, p2D_sub(Origin, point2D_t(GRID_RADII, 0)))), point2D_t(64, -38));
	cornersBR.pts.r = p2D_add(Iso::CartToISO(p2D_rotate(Rotation, Origin, p2D_add(Origin, point2D_t(GRID_RADII, 0)))), point2D_t(64, -38));
	cornersBR.pts.t = p2D_add(Iso::CartToISO(p2D_rotate(Rotation, Origin, p2D_sub(Origin, point2D_t(0, GRID_RADII)))), point2D_t(64, -38));
	cornersBR.pts.b = p2D_add(Iso::CartToISO(p2D_rotate(Rotation, Origin, p2D_add(Origin, point2D_t(0, GRID_RADII)))), point2D_t(64, -38));

	diamond2D_t cornersTR;
	cornersTR.pts.l = p2D_sub(cornersBR.pts.l, point2D_t(0, uiHeight));
	cornersTR.pts.r = p2D_sub(cornersBR.pts.r, point2D_t(0, uiHeight));
	cornersTR.pts.t = p2D_sub(cornersBR.pts.t, point2D_t(0, uiHeight));
	cornersTR.pts.b = p2D_sub(cornersBR.pts.b, point2D_t(0, uiHeight));

	//DrawLine<OLED::FRONT_BUFFER>(cornersBR.pts.b.pt.x, cornersBR.pts.b.pt.y, cornersBR.pts.r.pt.x, cornersBR.pts.r.pt.y);
	//DrawLine<OLED::FRONT_BUFFER>(cornersBR.pts.r.pt.x, cornersBR.pts.r.pt.y, cornersBR.pts.t.pt.x, cornersBR.pts.t.pt.y);
	//DrawLine<OLED::FRONT_BUFFER>(cornersBR.pts.t.pt.x, cornersBR.pts.t.pt.y, cornersBR.pts.l.pt.x, cornersBR.pts.l.pt.y);
	//DrawLine<OLED::FRONT_BUFFER>(cornersBR.pts.l.pt.x, cornersBR.pts.l.pt.y, cornersBR.pts.b.pt.x, cornersBR.pts.b.pt.y);

	//DrawVLine<OLED::FRONT_BUFFER>(cornersTR.pts.b.pt.x, cornersTR.pts.b.pt.y, uiHeight);
	//DrawVLine<OLED::FRONT_BUFFER>(cornersTR.pts.r.pt.x, cornersTR.pts.r.pt.y, uiHeight);
	//DrawVLine<OLED::FRONT_BUFFER>(cornersTR.pts.t.pt.x, cornersTR.pts.t.pt.y, uiHeight);
	//DrawVLine<OLED::FRONT_BUFFER>(cornersTR.pts.l.pt.x, cornersTR.pts.l.pt.y, uiHeight);

	//DrawLine<OLED::FRONT_BUFFER>(cornersTR.pts.b.pt.x, cornersTR.pts.b.pt.y, cornersTR.pts.r.pt.x, cornersTR.pts.r.pt.y);
	//DrawLine<OLED::FRONT_BUFFER>(cornersTR.pts.r.pt.x, cornersTR.pts.r.pt.y, cornersTR.pts.t.pt.x, cornersTR.pts.t.pt.y);
	//DrawLine<OLED::FRONT_BUFFER>(cornersTR.pts.t.pt.x, cornersTR.pts.t.pt.y, cornersTR.pts.l.pt.x, cornersTR.pts.l.pt.y);
	//DrawLine<OLED::FRONT_BUFFER>(cornersTR.pts.l.pt.x, cornersTR.pts.l.pt.y, cornersTR.pts.b.pt.x, cornersTR.pts.b.pt.y);
	
	// Draw Rooftop face first (some overdraw that is hidden/drawn over with the side faces
	// Get the maximum height first
	
	uint32_t uiMaxHeight(0);
	for (int32_t iDx = 1; iDx >= 0; --iDx)
	{
		uiMaxHeight = max(uiMaxHeight, (cornersTR.vs[InActiveFaces[iDx].Face[0]].pt.y
										- cornersTR.vs[ActiveFaces[iDx].Face[0]].pt.y) << 1);
		uiMaxHeight = max(uiMaxHeight, (cornersTR.vs[InActiveFaces[iDx].Face[1]].pt.y
										- cornersTR.vs[ActiveFaces[iDx].Face[1]].pt.y) << 1);
	}
	uiRooftopHeight = max(uiRooftopHeight, uiMaxHeight);

	for (int32_t iDx = 1; iDx >= 0; --iDx)
	{
		oOLED.setGrayLevel(0xFF);
		DrawVLines<OLED::BACK_BUFFER>(cornersTR.vs[InActiveFaces[iDx].Face[0]].pt.x,
									  cornersTR.vs[InActiveFaces[iDx].Face[0]].pt.y,
									  cornersTR.vs[InActiveFaces[iDx].Face[1]].pt.x,
								      cornersTR.vs[InActiveFaces[iDx].Face[1]].pt.y, uiRooftopHeight);
	}
	//DrawVLines<OLED::BACK_BUFFER>(cornersTR.pts.l.pt.x, cornersTR.pts.l.pt.y, cornersTR.pts.b.pt.x, cornersTR.pts.b.pt.y, uiHeight);

	// draw the 2 visible side faces //
	for (int32_t iDx = 1; iDx >= 0; --iDx)
	{
		oOLED.setGrayLevel(ActiveFaces[iDx].Shade);
		DrawVLines<OLED::BACK_BUFFER>(	cornersTR.vs[ActiveFaces[iDx].Face[0]].pt.x,
										cornersTR.vs[ActiveFaces[iDx].Face[0]].pt.y,
										cornersTR.vs[ActiveFaces[iDx].Face[1]].pt.x,
										cornersTR.vs[ActiveFaces[iDx].Face[1]].pt.y, uiHeight);
	}
	
	//GrayLevelLeft = Shade(vLineMidpoint, vNormalLeftFace, lAmbient);
	//uint8_t const GrayLevelRooftop = Shade(vec3_t(Origin.pt.x, uiHeight, Origin.pt.y), vec3_t(0.0f, -1.0f, 0.0f), lAmbient);
}
*/
// for voxels centered above ground level and not snapped to grid
// can be an "object instance" with fractional WorldCoordinates //
// - ScreenSpace Coords only
template<Shading::shade_op_default OPTop, Shading::shade_op_default_normal OPSides, uint32_t const TargetBuffer = OLED::BACK_BUFFER, uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE)>
__attribute__((always_inline)) STATIC_INLINE void RenderTinyVoxel_Complex( float const fNormalizedHeightOffset,
																																					 float const fRelativeHeightOffset,
																																					 float const fScale,
																																					 uint16_t const AdjAndShade,
																																	         point2D_t VoxelOrigin,
																																					 point2D_t const rotationX, point2D_t const rotationY)
{
	// scale is applied later during runtime
	static constexpr uint32_t const uiRooftopHeight = ct_sqrt( (Iso::VERY_TINY_GRID_RADII*Iso::VERY_TINY_GRID_RADII) + (Iso::VERY_TINY_GRID_RADII*Iso::VERY_TINY_GRID_RADII) ); //extents are based on "rotated" cornersBR
	
	diamond2D_t corners;
	
	{
		/*{
		
		diamond2D_t cornersB;
		point2D_t const pixelRadiiX( point2D_t(uiPixelRadii, 0) );
			
		// setup corners in screenspace
		cornersB.pts.l = p2D_sub(VoxelOrigin, pixelRadiiX);
		cornersB.pts.r = p2D_add(VoxelOrigin, pixelRadiiX);
		cornersB.pts.t = p2D_sub(VoxelOrigin, pixelHalfRadiiY);
		cornersB.pts.b = p2D_add(VoxelOrigin, pixelHalfRadiiY);

		OLED::setDepthLevel( p2D_sub(cornersB.pts.b, p2D_subh(cornersB.pts.b, cornersB.pts.t)).pt.y );  // depth is set at what would be the bottom
																																																		// this will set correct depth level for floating
																																																		// voxel so that it is infront of other objects
																																																		// don't want rotated version here of Corners
		}*/
		// get corner extents (bottom diamond)	
		corners.pts.l = p2D_sub(VoxelOrigin, rotationX);
		corners.pts.r = p2D_add(VoxelOrigin, rotationX);
		corners.pts.t = p2D_sub(VoxelOrigin, rotationY);
		corners.pts.b = p2D_add(VoxelOrigin, rotationY);

		// this is only working kinda, distance from any face to center is same, needs more accurate bottom center point of voxel
		OLED::setDepthLevel( p2D_sub(corners.pts.b, p2D_subh(corners.pts.b, corners.pts.t)).pt.y ); // depth is set at what would be the bottom
																																																			// this will set correct depth level for floating
																																																			// voxel so that it is infront of other objects
																																																			// ????
		
		// Modiify after depth level set, now offset the origin relative corners by pixelOffset (the floating space)
		float const fHalfRadiiScaled = fScale * Iso::VERY_TINY_GRID_FRADII_HALF;
		point2D_t const pixelOffset( 0, int32::__roundf(__fms(fRelativeHeightOffset,fHalfRadiiScaled,fHalfRadiiScaled)) );
		// pixelOffset = pixelOffset - pixelHalfRadii now, so bottom rotated corner becomes top rotated corner
		corners.pts.l = p2D_add(corners.pts.l, pixelOffset);
		corners.pts.r = p2D_add(corners.pts.r, pixelOffset);
		corners.pts.t = p2D_add(corners.pts.t, pixelOffset);
		corners.pts.b = p2D_add(corners.pts.b, pixelOffset);
		
		// update origin including the offset ? (used for shade calculation)
		VoxelOrigin = p2D_sub(corners.pts.b, p2D_subh(corners.pts.b, corners.pts.t));
		
		// top diamond is relative to corners, so will have same offset 
		
		// get corner extents (top diamond) based off rotated bottom corner extents
		//point2D_t const pixelHalfRadiiY( point2D_t(0, Iso::VERY_TINY_GRID_RADII) );
		
		// noe corners becomes top diamond
		//corners.pts.l = p2D_sub(corners.pts.l, pixelHalfRadiiY);
		//corners.pts.r = p2D_sub(corners.pts.r, pixelHalfRadiiY);
		//corners.pts.t = p2D_sub(corners.pts.t, pixelHalfRadiiY);
		//corners.pts.b = p2D_sub(corners.pts.b, pixelHalfRadiiY);
	}
	
	// Calculate normals
	vec3_t vNormalLeftFace, vNormalFrontFace;
	{
		point2D_t vNormal;

		// Left Face
		vNormal = p2D_sub(corners.pts.r, corners.pts.b);
		vNormalLeftFace = vec3_t(vNormal.pt.x, 0.0f, vNormal.pt.y);
		vNormalLeftFace = v3_normalize_fast(vNormalLeftFace);

		// Front Face
		vNormal = p2D_sub(corners.pts.t, corners.pts.r);
		vNormalFrontFace = vec3_t(vNormal.pt.x, 0.0f, vNormal.pt.y);
		vNormalFrontFace = v3_normalize_fast(vNormalFrontFace);
	}
	
	typedef struct InvisibleFaces
	{
		uint32_t Face[2];

		InvisibleFaces()
			: Face{ 0 }
		{}
		InvisibleFaces(uint32_t const Vertex0, uint32_t const Vertex1)
			: Face{ Vertex0, Vertex1 }
		{}
	} InvisibleFaces;
	typedef struct VisibleFaces : InvisibleFaces
	{
		xDMA2D::AlphaLuma Shade;

		VisibleFaces()
			: Shade(0)
		{}
		VisibleFaces(uint32_t const Vertex0, uint32_t const Vertex1, uint32_t const inShade)
			: InvisibleFaces(Vertex0, Vertex1), Shade(inShade)
		{}
	} VisibleFaces;
	
	float const DiffuseShade( ((float)((uint8_t)AdjAndShade)) * Constants::inverseUINT8 );
	
	VisibleFaces ActiveFaces[2];
	InvisibleFaces InActiveFaces[2]; // still used to make up rooftop face
	int32_t faceCount(0); // side face active count
	
	{ // scope var
	vec3_t const vNormalView(0.0f, 0.0f, -1.0f);

	InvisibleFaces* __restrict pInActive(InActiveFaces);
	if ( v3_dot(vNormalView, vNormalFrontFace) < 0.0f ) {
		
		if ( !Volumetric::voxB::testAdj(AdjAndShade, Volumetric::voxB::BIT_ADJ_FRONT)) {
			ActiveFaces[faceCount++] = VisibleFaces( diamond2D_t::TOP, diamond2D_t::LEFT, // Front
																							 __USAT( Shading::do_shading_op_default_normal<OPSides>(vec3_t(VoxelOrigin.pt.x, fNormalizedHeightOffset, VoxelOrigin.pt.y), 
																											 (vNormalFrontFace), DiffuseShade) + Volumetric::voxB::BASE_AMBIENT, Constants::SATBIT_256 ) );
		}
		*pInActive++ = InvisibleFaces(diamond2D_t::BOTTOM, diamond2D_t::RIGHT);
	}
	else {
		
		if ( !Volumetric::voxB::testAdj(AdjAndShade, Volumetric::voxB::BIT_ADJ_BACK)) {
			ActiveFaces[faceCount++] = VisibleFaces( diamond2D_t::BOTTOM, diamond2D_t::RIGHT,  // Back
																							 __USAT( Shading::do_shading_op_default_normal<OPSides>(vec3_t(VoxelOrigin.pt.x, fNormalizedHeightOffset, VoxelOrigin.pt.y), 
																											 (vNormalFrontFace), DiffuseShade) + Volumetric::voxB::BASE_AMBIENT, Constants::SATBIT_256 ) );
		}
		*pInActive++ = InvisibleFaces(diamond2D_t::TOP, diamond2D_t::LEFT);
	}
	
	if ( v3_dot(vNormalView, vNormalLeftFace) < 0.0f ) {
		
		if ( !Volumetric::voxB::testAdj(AdjAndShade, Volumetric::voxB::BIT_ADJ_LEFT)) {
			ActiveFaces[faceCount++] = VisibleFaces( diamond2D_t::RIGHT, diamond2D_t::TOP,  // Left
																							 __USAT( Shading::do_shading_op_default_normal<OPSides>(vec3_t(VoxelOrigin.pt.x, fNormalizedHeightOffset, VoxelOrigin.pt.y), 
																											 (vNormalLeftFace), DiffuseShade) + Volumetric::voxB::BASE_AMBIENT, Constants::SATBIT_256 ) );
		}
		*pInActive++ = InvisibleFaces(diamond2D_t::LEFT, diamond2D_t::BOTTOM);
	}
	else {
		
		if ( Volumetric::voxB::testAdj(AdjAndShade, Volumetric::voxB::BIT_ADJ_LEFT)) {
			ActiveFaces[faceCount++] = VisibleFaces( diamond2D_t::LEFT, diamond2D_t::BOTTOM,  // Right
																								 __USAT( Shading::do_shading_op_default_normal<OPSides>(vec3_t(VoxelOrigin.pt.x, fNormalizedHeightOffset, VoxelOrigin.pt.y), 
																												(vNormalLeftFace), DiffuseShade) + Volumetric::voxB::BASE_AMBIENT, Constants::SATBIT_256 ) );
		}
		*pInActive++ = InvisibleFaces(diamond2D_t::RIGHT, diamond2D_t::TOP);
	}
	
	} // scope var
	
	// Draw Rooftop face first (some overdraw that is hidden/drawn over with the side faces
	if ( !Volumetric::voxB::testAdj(AdjAndShade, Volumetric::voxB::BIT_ADJ_ABOVE) ) {
		xDMA2D::AlphaLuma RooftopShade( __USAT( Shading::do_shading_op_default<OPTop>(vec3_t(VoxelOrigin.pt.x, fNormalizedHeightOffset, VoxelOrigin.pt.y), 
																				    Lighting::NORMAL_TOPFACE, DiffuseShade) + Volumetric::voxB::BASE_AMBIENT, Constants::SATBIT_256 ) );
		
		if constexpr (OLED::FRONT_BUFFER == TargetBuffer)
			RooftopShade.makeOpaque();
		
		uint32_t const uiRooftopHeightScaled( max(1, uint32::__roundf(uiRooftopHeight * fScale)) );
		#pragma unroll
		for (int32_t iDx = 1; iDx >= 0; --iDx)
		{
			OLED::DrawVLines<TargetBuffer, RenderingFlags>(corners.vs[InActiveFaces[iDx].Face[0]],
																										corners.vs[InActiveFaces[iDx].Face[1]],
																										uiRooftopHeightScaled, 
																										RooftopShade );
		}
	}
	
	// draw the visible side faces
	uint32_t const uiRadiiScaled( max(1, uint32::__roundf(fScale * Iso::VERY_TINY_GRID_FRADII)) );
	#pragma unroll
	for (int32_t iDx = faceCount - 1; iDx >= 0; --iDx)
	{
		if constexpr (OLED::FRONT_BUFFER == TargetBuffer)
			ActiveFaces[iDx].Shade.makeOpaque();
		
		OLED::DrawVLines<TargetBuffer, RenderingFlags>(corners.vs[ActiveFaces[iDx].Face[0]],
																									 corners.vs[ActiveFaces[iDx].Face[1]],
																									 uiRadiiScaled, ActiveFaces[iDx].Shade);
	}
	
}


template<Shading::shade_op_default OP, uint32_t const TargetBuffer = OLED::BACK_BUFFER, uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE)>
__attribute__((always_inline)) STATIC_INLINE void RenderTinyVoxel_Static( float const fNormalizedHeightOffset,
																																					float const fRelativeHeightOffset,
																																					uint16_t const AdjAndShade,
																																	        point2D_t const VoxelOrigin )
{
	static constexpr uint32_t const uiPixelRadii = Iso::VERY_TINY_GRID_RADII << 1;
	int32_t const pixelOffset( int32::__roundf(fRelativeHeightOffset * (float)(uiPixelRadii<<1)) );
	point2D_t xPoint, yPoint, yPointTopReset;
	
	{
		// get plot corner extents (bottom/top diamond)
		halfdiamond2D_t cornersB;
			
		// get corner extents (bottom diamond)
		//cornersB.pts.l = p2D_sub(Origin, point2D_t(Iso::GRID_RADII, 0));
		//cornersB.pts.r = p2D_add(Origin, point2D_t(Iso::GRID_RADII, 0));
		cornersB.pts.t = p2D_sub(VoxelOrigin, point2D_t(0, uiPixelRadii>>1));
		cornersB.pts.b = p2D_add(VoxelOrigin, point2D_t(0, uiPixelRadii>>1));
		
		OLED::setDepthLevel( p2D_sub(cornersB.pts.b, p2D_subh(cornersB.pts.b, cornersB.pts.t)).pt.y );  // solves depth for shockwave! yay!!
		
		// Modiify after depth level set, now offset the origin relative corners by pixelOffset
		cornersB.pts.t = p2D_add(cornersB.pts.t, point2D_t(0, pixelOffset));
		cornersB.pts.b = p2D_add(cornersB.pts.b, point2D_t(0, pixelOffset));
		// top diamond is relative to cornersB, so will have same offset 
		
		// get corner extents (top diamond)
		halfdiamond2D_t cornersT;
		
		//cornersT.pts.l = p2D_sub(cornersB.pts.l, point2D_t(0, uiHeight));
		//cornersT.pts.r = p2D_sub(cornersB.pts.r, point2D_t(0, uiHeight));
		cornersT.pts.t = p2D_sub(cornersB.pts.t, point2D_t(0, uiPixelRadii>>1));
		cornersT.pts.b = p2D_sub(cornersB.pts.b, point2D_t(0, uiPixelRadii>>1));
		
		// Draw Vertical Lines Filling Left side & front side of voxel
		// starting from bottom work to left & RIGHT
		
		xPoint = point2D_t(cornersT.pts.b.pt.x, cornersB.pts.b.pt.x);
		yPoint = point2D_t(cornersT.pts.b.pt.y, cornersB.pts.b.pt.y);
		yPointTopReset = point2D_t(cornersT.pts.t.pt.y, cornersT.pts.t.pt.y);
	}
	
	// this could be batched
	float const DiffuseShade( ((float)((uint8_t)AdjAndShade)) * Constants::inverseUINT8 );
	int32_t LumaLeft(-1), LumaFront(-1), LumaTop(-1);
	
	if ( !Volumetric::voxB::testAdj(AdjAndShade, Volumetric::voxB::BIT_ADJ_LEFT) )
		LumaLeft = __USAT( Shading::do_shading_op_default<OP>(vec3_t(VoxelOrigin.pt.x, fNormalizedHeightOffset, VoxelOrigin.pt.y), Lighting::NORMAL_LEFTFACE, DiffuseShade) + Volumetric::voxB::BASE_AMBIENT, Constants::SATBIT_256 );
	if ( !Volumetric::voxB::testAdj(AdjAndShade, Volumetric::voxB::BIT_ADJ_FRONT) )
		LumaFront = __USAT( Shading::do_shading_op_default<OP>(vec3_t(VoxelOrigin.pt.x, fNormalizedHeightOffset, VoxelOrigin.pt.y), Lighting::NORMAL_FRONTFACE, DiffuseShade) + Volumetric::voxB::BASE_AMBIENT, Constants::SATBIT_256 );
	if ( !Volumetric::voxB::testAdj(AdjAndShade, Volumetric::voxB::BIT_ADJ_ABOVE) )
		LumaTop = __USAT( Shading::do_shading_op_default<OP>(vec3_t(VoxelOrigin.pt.x, fNormalizedHeightOffset, VoxelOrigin.pt.y), Lighting::NORMAL_TOPFACE, DiffuseShade) + Volumetric::voxB::BASE_AMBIENT, Constants::SATBIT_256 );

	for (int iDx = (uiPixelRadii) - 1, iD = 0; iDx >= 0; iDx--, iD++)
	{
		point2D_t curXPoints, curYPoints;
		point2D_t const ptIncrement(iD >> 1, iD >> 1);
		
		curXPoints = p2D_add(xPoint, point2D_t(-iD, iD));
		curYPoints = p2D_sub(yPoint, ptIncrement);

		if ( LumaTop >= 0 ) { // scope rooftop
			// Draw Rooftop
			point2D_t yPointTop;

			yPointTop = p2D_add(yPointTopReset, ptIncrement);
			yPointTop = p2D_saturate<OLED::Height_SATBITS>(yPointTop);

			// Leverage left and front(right) x point iterations, and leverage y point aswell for length
			int32_t const iLength = (curYPoints.pt.x - yPointTop.pt.x);
			if (likely(iLength > 0)) {
	
				if ( OLED::CheckVLine(curXPoints.pt.x, yPointTop.pt.x, iLength) ) {
					OLED::DrawVLine<TargetBuffer, RenderingFlags>(curXPoints.pt.x, yPointTop.pt.x, iLength, LumaTop);
				}
				if ( OLED::CheckVLine(curXPoints.pt.y, yPointTop.pt.x, iLength) ) {
					OLED::DrawVLine<TargetBuffer, RenderingFlags>(curXPoints.pt.y, yPointTop.pt.x, iLength, LumaTop);
				}
			}
		} // scope rooftop
		
		if ( OLED::CheckVLine_YAxis(curYPoints.pt.x, (curYPoints.pt.y - curYPoints.pt.x)) ) 
		{
			// saturate y axis only to clip lines, x axis bounds are fully checked, y axis must be clipped to zero for line length calculation
			curYPoints = p2D_saturate<OLED::Height_SATBITS>(curYPoints);
			int32_t const iLength = (curYPoints.pt.y - curYPoints.pt.x);
			
			if (likely(iLength > 0))
			{
				// Calculate lighting per vertical line
				// incident light vector
				if ( LumaLeft >= 0 ) {
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) { // only for single diamond, or left most diamond
							// Left Face
							OLED::DrawVLine<TargetBuffer, RenderingFlags>(curXPoints.pt.x, curYPoints.pt.x, iLength, LumaLeft);
					} // xaxis check
				}
				
				if ( LumaFront >= 0 ) {
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
							// Front Face
							OLED::DrawVLine<TargetBuffer, RenderingFlags>(curXPoints.pt.y, curYPoints.pt.x, iLength, LumaFront);
					} // xaxis check
				}
			}
		} // y axis check
	} // for
}

} // endnamespace

#endif

