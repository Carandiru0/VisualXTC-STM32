/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef ISOVOXEL_H
#define ISOVOXEL_H
#include "point2D.h"

namespace Iso
{
	
// cONSTANTS
	static constexpr uint32_t const GRID_RADII = 12,
																	GRID_RADII_X2 = (GRID_RADII<<1);
	
	static constexpr uint32_t const TINY_GRID_RADII = 3,
																	TINY_GRID_RADII_X2 = (TINY_GRID_RADII<<1);
	
	static constexpr uint32_t const VERY_TINY_GRID_RADII = 1;  // do not modify, modify model scale instead
	
	static constexpr uint32_t const WORLD_GRID_SIZE = 288,
																	WORLD_GRID_HALFSIZE = (WORLD_GRID_SIZE>>1);
	
	static constexpr int32_t const	MIN_VOXEL_COORD = -(WORLD_GRID_HALFSIZE),
																	MAX_VOXEL_COORD = (WORLD_GRID_HALFSIZE);
	
	static constexpr float const GRID_FRADII = (float)GRID_RADII,
															 GRID_FRADII_X2 = (float)GRID_RADII_X2,
															 TINY_GRID_FRADII = (float)TINY_GRID_RADII,
															 TINY_GRID_FRADII_X2 = (float)TINY_GRID_RADII_X2,
															 TINY_GRID_SCALE = (float)TINY_GRID_FRADII/GRID_FRADII,
															 TINY_GRID_INVSCALE = 1.0f / TINY_GRID_SCALE,	
															 VERY_TINY_GRID_FRADII = (float)VERY_TINY_GRID_RADII,
															 VERY_TINY_GRID_FRADII_HALF = VERY_TINY_GRID_FRADII/2.0f,
															 VERY_TINY_GRID_HALF_SCALE = (float)VERY_TINY_GRID_FRADII_HALF/GRID_FRADII,	
															 WORLD_GRID_FSIZE = (float)WORLD_GRID_SIZE,
															 WORLD_GRID_FHALFSIZE = (float)WORLD_GRID_HALFSIZE,
															 INVERSE_WORLD_GRID_FSIZE = 1.0f / WORLD_GRID_FSIZE;
	
	static constexpr float const	MIN_VOXEL_FCOORD = (float)MIN_VOXEL_COORD,
																MAX_VOXEL_FCOORD = (float)MAX_VOXEL_COORD;
// Desc bits:
	
	// 0000 00 11 ### TYPE ###
static constexpr uint8_t const MASK_TYPE_BITS 		= 0b00000011;
static constexpr uint8_t const TYPE_GROUND 				= 0, // Exclusive bits....
															 TYPE_STRUCTURE 		= (1 << 0),
														   TYPE_DESTRUCTION 	= (1 << 1),
															 TYPE_EFFECT 				= (1 << 1) | (1 << 0);

	// 0001 11 00 ### HEIGHT ###
static constexpr uint8_t const MASK_HEIGHT_BITS 	= 0b00011100;
static constexpr uint8_t const DESC_HEIGHT_STEP_0 = 0;	// 8 Values (0-7 3bits)
static constexpr uint8_t const MAX_HEIGHT_STEP = 7;	// 8 Values (0-7 3bits)

static constexpr uint8_t const MASK_DESC_FLAGS		= 0b11100000;		// Misc Flags used (no implemented yet)


// Hash (SubType) bits:
	
	// ### TYPE_GROUND ###
static constexpr uint8_t const MASK_OCCLUSION_BITS  = 0b00111111;
static constexpr uint8_t const NOT_VISIBLE = 0, 											// Completely Occluded
															 OCCLUSION_SHADING_NONE = (1 << 0),			// (nORMAL sHADING)
															 OCCLUSION_SHADING_TOP_LEFT = (1 << 1),	// Combinable bits....
															 OCCLUSION_SHADING_TOP_RIGHT = (1 << 2),
															 OCCLUSION_SHADING_SIDE_LEFT = (1 << 3),
															 OCCLUSION_SHADING_SIDE_RIGHT = (1 << 4),
															 OCCLUSION_SIDES_NOT_VISIBILE = (1 << 5),
															 ROAD = (1 << 6),
															 SIGN = (1 << 7);
	
	// ### TYPE_STRUCTURE ###
static constexpr uint8_t const MASK_PLOT_TYPE_BITS  = 0b00000001;
static constexpr uint8_t const MASK_PLOT_SIZE_BITS  = 0b00000110;
static constexpr uint8_t const ROOT_PLOT_PART = 0, // Combinable bits....
														   SIBLING_PLOT_PART = (1 << 0),
															 PLOT_SIZE_BITS0 = (1 << 1),
															 PLOT_SIZE_BITS1 = (1 << 2);
															// CARVED_SUBDIVISIONS_1 = (1 << 3),
															// SYMMETRY_X = (1 << 4),
															// SYMMETRY_Z = (1 << 5);
															 //(1 << 6) thri (1 << 7) free for future use (2bits);
															 

// ### TYPE_DESTRUCTION ###
static constexpr uint8_t const // = 0, // Combinable bits....
															 // (1 << 0) thru (1 << 3) free for future use (4bits)
														   COLLAPSING = (1 << 4),
															 COLLAPSED = (1 << 5),
															 STACKED_1 = (1 << 6),
															 STACKED_2 = (1 << 7);

// ### TYPE_EFFECT ###
static constexpr uint8_t const // = 0, // Combinable bits....
															 // (1 << 0) thru (1 << 3) free for future use (4bits)
															 FIRE = (1 << 4),
															 WATER = (1 << 5),
															 CRACKED_EARTH = (1 << 6);

typedef struct __attribute__((packed)) sVoxel
{	
	uint8_t Desc;								// Type of Voxel + Attributes
	uint8_t Hash;								// Deterministic SubType
	
} Voxel;


	// Special case leveraging more of the beginning bits
	__attribute__((always_inline)) STATIC_INLINE_PURE bool const isGround( Voxel const oVoxel ) { return( TYPE_GROUND == (MASK_TYPE_BITS & oVoxel.Desc ) ); }
	__attribute__((always_inline)) STATIC_INLINE_PURE bool const isStructure( Voxel const oVoxel ) { return( TYPE_STRUCTURE == (MASK_TYPE_BITS & oVoxel.Desc) ); }
	__attribute__((always_inline)) STATIC_INLINE_PURE bool const isDestruction( Voxel const oVoxel ) { return( TYPE_DESTRUCTION == (MASK_TYPE_BITS & oVoxel.Desc) ); }
	__attribute__((always_inline)) STATIC_INLINE_PURE bool const isEffect( Voxel const oVoxel ) { return( TYPE_EFFECT == (MASK_TYPE_BITS & oVoxel.Desc) ); }
	
	STATIC_INLINE void setType( Voxel& oVoxel, uint8_t const Type ) 
	{
		uint32_t const CurType = (MASK_TYPE_BITS & oVoxel.Desc);
		
		// if different only...
		if ( Type != CurType ) {
			// Clear type bits
			oVoxel.Desc &= (~MASK_TYPE_BITS);
			// Set new type bits safetly
			oVoxel.Desc |= ( MASK_TYPE_BITS & (Type) );
			
			// Reset Hash Bits on type change
			oVoxel.Hash = 0;
		}
	}
	
	STATIC_INLINE_PURE uint32_t const getHeightStep( Voxel const oVoxel ) 
	{ 
		// Mask off height bits
		// Shift to realize number
		return( (MASK_HEIGHT_BITS & oVoxel.Desc) >> 2 ); 
	}
	STATIC_INLINE void setHeightStep( Voxel& oVoxel, uint8_t const HeightStep ) 
	{ 
		// Clear height bits
		oVoxel.Desc &= (~MASK_HEIGHT_BITS);
		// Set new height bits safetly truncating passed parameter HeightStep
		oVoxel.Desc |= ( MASK_HEIGHT_BITS & (HeightStep << 2) );
	}
	
	// #defines to ensure one comparison only and compared with zero always (performance)
	
	// #### Ground Only
	#define isVoxelVisible(oVoxel) ( Iso::NOT_VISIBLE != (Iso::MASK_OCCLUSION_BITS & oVoxel.Hash) )
	
	#define isRoad(oVoxel) ( 0 != (Iso::ROAD & oVoxel.Hash) )		// Combinable with Flat/Sloped Bits
	#define isSign(oVoxel) ( 0 != (Iso::SIGN & oVoxel.Hash) )		// Combinable with Flat/Sloped Bits
	
	STATIC_INLINE_PURE uint8_t const getOcclusion(Voxel const oVoxel) { 
		return( MASK_OCCLUSION_BITS & oVoxel.Hash );
	}
	STATIC_INLINE void setNotVisible(Voxel& oVoxel) { oVoxel.Hash &= (~MASK_OCCLUSION_BITS); }
	STATIC_INLINE void setOcclusion(Voxel& oVoxel, uint8_t const occlusionshading) { 
		// Clear occlusion bits
		oVoxel.Hash &= (~MASK_OCCLUSION_BITS);
		// Set new occlusion bits
		oVoxel.Hash |= ( MASK_OCCLUSION_BITS & occlusionshading );
	}
		
	// #### Structure Only
	#define isRootPlotPart(oVoxel) ( (Iso::ROOT_PLOT_PART == (Iso::MASK_PLOT_TYPE_BITS & oVoxel.Hash)) )		
	#define isSiblingPlotPart(oVoxel) ( (Iso::ROOT_PLOT_PART != (Iso::MASK_PLOT_TYPE_BITS & oVoxel.Hash)) )	
	
	STATIC_INLINE void setPlotPartType(Voxel& oVoxel, uint8_t const bSiblingOrRoot) { 
		// Clear bits
		oVoxel.Hash &= (~MASK_PLOT_TYPE_BITS);
		// Set new bits
		oVoxel.Hash |= bSiblingOrRoot;
	}
	
	// matches same values in constexpr of BuildingGen
	STATIC_INLINE_PURE uint32_t const getPlotSize( Voxel const oVoxel ) { return( ((MASK_PLOT_SIZE_BITS & oVoxel.Hash) >> 1) + 1 ); }
	// use the BuildingGen constexpr, PLOT_SIZE_1x1, PLOT_SIZE_2x2, PLOT_SIZE_3x3, PLOT_SIZE_4x4
	STATIC_INLINE void setPlotSize(Voxel& oVoxel, uint32_t const uiPlotSize) { 
		// Clear bits
		oVoxel.Hash &= (~MASK_PLOT_SIZE_BITS);
		// Set new bits
		oVoxel.Hash |= ( MASK_PLOT_SIZE_BITS & ((uiPlotSize - 1) << 1) );
	}
	
	//STATIC_INLINE_PURE uint32_t const getCarvedSubdivisions( Voxel const oVoxel ) { return( (0b00001100 & oVoxel.Hash) >> 2 ); }
	
	//#define isSymmetrical_X(oVoxel) ( 0 != (Iso::SYMMETRY_X & oVoxel.Hash) )		// Can be all combinations
	//#define isSymmetrical_Z(oVoxel) ( 0 != (Iso::SYMMETRY_Z & oVoxel.Hash) )		// ie. fully symmetric both would be true
	
	// #### Destruction Only
	#define isCollapsing(oVoxel) ( 0 != (Iso::COLLAPSING & oVoxel.Hash) )
	#define isCollapsed(oVoxel) ( 0 != (Iso::COLLAPSED & oVoxel.Hash) )
	
	STATIC_INLINE void setDestruction_Collapsing(Voxel& oVoxel) { 
		// Set new bits
		oVoxel.Hash |= Iso::COLLAPSING;
	}
	STATIC_INLINE void setDestruction_Collapsed(Voxel& oVoxel) { 
		// Clear bits
		oVoxel.Hash &= (~Iso::COLLAPSING);
		// Set new bits
		oVoxel.Hash |= Iso::COLLAPSED;
	}
	
	// #### Effects Only
	#define isFire(oVoxel) ( 0 != (Iso::FIRE & oVoxel.Hash) )		// THESE are exclusive
	#define isFog(oVoxel) ( 0 != (Iso::FOG & oVoxel.Hash) )			// however are combinable with flat/sloped bits for ground background
	#define isWater(oVoxel) ( 0 != (Iso::WATER & oVoxel.Hash) )
	#define isCrackedEarth(oVoxel) ( 0 != (Iso::CRACKED_EARTH & oVoxel.Hash) )
	
	
	// iSOMETRIC hELPER fUNCTIONS
	
	// Grid Space to World Isometric Coordinates Only, Grid Spae should be pushed from
	// range (-x,-y)=>(X,Y) to (0, 0)=>(X, Y) units first if in that coordinate system
	// **** this is also Isometric to screenspace
	STATIC_INLINE_PURE point2D_t const p2D_GridToIso( point2D_t const gridSpace )
	{
		static constexpr int32_t const GRID_X = (Iso::GRID_RADII_X2 >> 1),
																	 GRID_Y = (Iso::GRID_RADII >> 1);
			/*
				_x = (_col * tile_width * .5) + (_row * tile_width * .5);
				_y = (_row * tile_hieght * .5) - ( _col * tile_hieght * .5);
			*/
		return( point2D_t( (gridSpace.pt.x + gridSpace.pt.y) * GRID_X,
											 (gridSpace.pt.x - gridSpace.pt.y) * GRID_Y ));
	}
	#define p2D_IsoToScreen p2D_GridToIso
	
	// Grid Space to World Isometric Coordinates Only, Grid Spae should be pushed from
	// range (-x,-y)=>(X,Y) to (0, 0)=>(X, Y) units first if in that coordinate system
	// **** this is also Isometric to screenspace (barebones, see world.h v2_gridtoscreen)
	 STATIC_INLINE_PURE vec2_t const v2_GridToIso( vec2_t const gridSpace )
	{
		static constexpr float const GRID_FX = (Iso::GRID_FRADII_X2 / 2.0f),
																 GRID_FY = (Iso::GRID_FRADII / 2.0f);
			/*
				_x = (_col * tile_width * .5) + (_row * tile_width * .5);
				_y = (_row * tile_hieght * .5) - ( _col * tile_hieght * .5);
			*/
		return( vec2_t( (gridSpace.x + gridSpace.y) * GRID_FX,
										(gridSpace.x - gridSpace.y) * GRID_FY ));
	}
	#define v2_IsoToScreen v2_GridToIso
	
	STATIC_INLINE_PURE vec2_t const v2_ScreenToIso( vec2_t const screenSpace )
	{
		static constexpr float const INV_GRID_FX = 1.0f / (Iso::GRID_FRADII_X2),
																 INV_GRID_FY = 1.0f / (Iso::GRID_FRADII);
			/*
			// factored out the constant divide-by-two
			// only if we're doing floating-point division!
			map.x = screen.y / TILE_HEIGHT - screen.x / TILE_WIDTH;
			map.y = screen.y / TILE_HEIGHT + screen.x / TILE_WIDTH;
		*/
		float const fXScreen(screenSpace.x * INV_GRID_FX),
								fYScreen(screenSpace.y * INV_GRID_FY);
		return( vec2_t( fYScreen - fXScreen, fYScreen + fXScreen ));
	}
	
	 STATIC_INLINE_PURE point2D_t const p2D_ScreenToIso( point2D_t const screenSpace )
	{
		// avoid division, leverage other function for floats
		// 4 conversions = 4 cycles
		// integer division = 14 cycles minimum, and there would be 4 of them (ouch)
		return( v2_to_p2D( v2_ScreenToIso(p2D_to_v2(screenSpace)) ) );
	}
	
	// not sure if these functions are compatible with the screwed up isometric layout
	STATIC_INLINE_PURE point2D_t const CartToISO(point2D_t const ptCart)
	{
		return(point2D_t(ptCart.pt.x - ptCart.pt.y, (ptCart.pt.x + ptCart.pt.y) >> 1));
	}
	STATIC_INLINE_PURE vec2_t const CartToISO(vec2_t const ptCart)
	{
		return(vec2_t(ptCart.x - ptCart.y, (ptCart.x + ptCart.y) * 0.5f));
	}
	STATIC_INLINE_PURE point2D_t const ISOToCart(point2D_t const ptIso)
	{
		return(point2D_t(((ptIso.pt.y << 1) + ptIso.pt.x) >> 1, ((ptIso.pt.y << 1) - ptIso.pt.x) >> 1));
	}
	
} // endnamespace

#endif

