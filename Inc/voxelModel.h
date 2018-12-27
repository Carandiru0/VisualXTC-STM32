/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef VOXEL_MODEL_H
#define VOXEL_MODEL_H

#include <vector>
#include "vector_rotation.h"

namespace Volumetric
{
namespace voxB
{
	static constexpr bool const DYNAMIC = true,
															STATIC = false;
	static uint32_t const MAX_DIMENSION_X = 128, MAX_DIMENSION_YZ = 64; // supporting 128x64x64 size voxel model (performance)
	static uint32_t const BASE_AMBIENT = 0x0F;
	
	typedef struct voxCoord	
	{
		uint8_t		x,
							y,
							z;
		
		inline void set(uint8_t const xx, uint8_t const yy, uint8_t const zz)
		{
			x = xx;
			y = yy;
			z = zz;
		}
		inline voxCoord(uint8_t const xx, uint8_t const yy, uint8_t const zz)
			: x(xx), y(yy), z(zz)
		{}
			
	} voxCoord;
	
	// Back, Right Face Adjacency only usefull for dynamic voxel models...
	static constexpr uint32_t const BIT_ADJ_ABOVE = (1<<0),				// USED DURING RUNTIME TO CULL FACES
																	BIT_ADJ_BACK = (1<<1),				// ""														""
																	BIT_ADJ_FRONT = (1<<2),				// ""														""
																	BIT_ADJ_RIGHT = (1<<3),				// ""														""
																	BIT_ADJ_LEFT = (1<<4),				// ""														""
																	BIT_ADJ_ABOVEFRONT = (1<<5),	// USED in combination with above flags		for loading culling whole voxels
																	BIT_ADJ_ABOVELEFT = (1<<6),		// ""																""
																	BIT_ADJ_FRONTLEFT = (1<<7);		// ""																															""
																	// BIT_ADJ_ABOVEFRONTLEFT = case where voxel would be completely occulded
	STATIC_INLINE_PURE bool const	testAdj(uint16_t const AdjAndShade, uint8_t const bitMASK) { return( bitMASK & (AdjAndShade >> 8)); }
	
	typedef struct voxAdjacency	// for visibility testing only, can skip whole faces of a voxel
	{														// only adjaceny for neighbours that we care about
		bool  		Left,				// if " 1 " then left face can be skipped for *this
							Right,			// if " 1 " then right face can be skipped for *this
							Front,			// if " 1 " then front face can be skipped for *this
							Back,				// if " 1 " then back face can be skipped for *this
							Above;  		// if " 1 " then top face can be skipped for *this

		inline voxAdjacency( uint8_t const pendingAdjacency )
			: Above( BIT_ADJ_ABOVE & pendingAdjacency ), 
				Back( BIT_ADJ_BACK & pendingAdjacency ), Front( BIT_ADJ_FRONT & pendingAdjacency ), 
				Right( BIT_ADJ_RIGHT & pendingAdjacency), Left( BIT_ADJ_LEFT & pendingAdjacency )
		{}
	} voxAdjacency;
	
	typedef struct __attribute__((packed)) voxelDescPacked	// Final 128x64x64 Structure, 4 bytes in size
	{
		union
		{
			struct __attribute__((packed))
			{
				uint8_t 			 		x : 7,						// bits 1 : 19, Position (0 - 127) 128 Values X Component & (0 - 63) 64 Values each component (y,z)
													y : 6,
													z : 6,
													Left : 1,					// bits 20 : 24, Adjacency (0 / 1) Binary boolean
													Right : 1,
													Front : 1,
													Back	: 1,
													Above : 1,
													Shade : 8;				// bits 25 : 32, Diffuse Shade (0 - 255) 256 Values
			};
			
			uint32_t					Data;
		};
		
		inline vec3_t const 		getPosition() const { return( vec3_t(x, y, z) ); }
		inline uint8_t const 		getShade() const { return( Shade ); }
		inline uint16_t const   getAdjAndShade() const { return( (((uint16_t)Left) << 12) | (((uint16_t)Right) << 11) | (((uint16_t)Front) << 10) | (((uint16_t)Back) << 9) | (((uint16_t)Above) << 8) | getShade() ); }
		
		__attribute__((pure)) inline bool const operator<(voxelDescPacked const& rhs) const														
		{																								
				return( z > rhs.z );		// for sorting, z-buffer can be leveraged for regular 2d isometric coordinates (the first .x.y)
																						// howver for .z each "slice" representing height above ground must be rendered
																						// in bottom to top order. ie.) solves the case where a voxel of a greater height offset (.z)
																						// shares the exact same space (.x,.y)
		}
			
		__attribute__((pure)) inline bool const operator!=(voxelDescPacked const& rhs) const														
		{	
			return(Data != rhs.Data);
		}
		inline voxelDescPacked(voxCoord const Coord, voxAdjacency const Adj, uint8_t const inShade)
			: x(Coord.x), y(Coord.y), z(Coord.z),
				Above(Adj.Above), Front(Adj.Front), Left(Adj.Left),
				Shade( inShade )
		{}
	} voxelDescPacked;
		
	typedef struct __attribute__((packed)) voxelModelDescHeader
	{
		uint32_t numVoxels;
		uint8_t  dimensionX, dimensionY, dimensionZ;
		
	} voxelModelDescHeader;
	
	typedef struct voxelModelBase
	{		
		std::vector<voxelDescPacked>	VoxelsTemp;
		voxelDescPacked const* __restrict   VoxelsFRAM;	// Address to FRAM Location containg voxels
		uint32_t 		numVoxels;					// # of voxels activated
		vec3_t 			maxDimensionsInv;
		vec3_t 			maxDimensions;			// actual used size of voxel model
		float const	Scalar;
		bool const	isDynamic_;
		
		inline voxelModelBase(bool const isDynamic, float const Scale = 1.0f) //  #!#!#! minimum allowed scale is 1.0f #!#!#!  //
				: numVoxels(0), Scalar(Scale), isDynamic_(isDynamic)
		{}
		
	} voxelModelBase; // voxelModelBase
	
	template< bool const Dynamic >
	class voxelModel : public voxelModelBase
	{
	public:
		constexpr bool const isDynamic() const { return(Dynamic); }
		
		inline voxelModel(float const Scale = 1.0f)		//  #!#!#! minimum allowed scale is 1.0f #!#!#!  //
		: voxelModelBase(Dynamic, Scale)
		{}
	};
	
	typedef struct voxelModelInstance_Dynamic
	{
		voxelModel<DYNAMIC> const& __restrict 		model;
		vec2_t																		vLoc;
		vec2_rotation_t														vR;
		float const																Radius;
		
		// static model must be loaded b4 any instance creation!
		inline explicit voxelModelInstance_Dynamic(voxelModel<DYNAMIC> const& __restrict refModel, vec2_t const worldCoord)
		: model(refModel), vLoc(worldCoord), vR(0.0f),
			Radius( v3_length( v3_muls( model.maxDimensions, Iso::VERY_TINY_GRID_RADII ) ) )
		{}
		~voxelModelInstance_Dynamic() = default;
			
			
	} voxelModelInstance_Dynamic;
	
	typedef struct voxelModelInstance_Static
	{
		voxelModel<STATIC> const& __restrict 			model;
		vec2_t																		vLoc;
		float const																Radius;
		
		// static model must be loaded b4 any instance creation!
		inline explicit voxelModelInstance_Static(voxelModel<STATIC> const& __restrict refModel, vec2_t const worldCoord)
		: model(refModel), vLoc(worldCoord),
			Radius( v3_length( v3_muls( model.maxDimensions, Iso::VERY_TINY_GRID_RADII ) ) )
		{}
		~voxelModelInstance_Static() = default;
			
			
	} voxelModelInstance_Static;
	
} // end namespace voxB
} // end namespace Volumetric


#endif

