/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef SHADE_OPS_H
#define SHADE_OPS_H

#include "math_3d.h"
#include "point2d.h"

namespace Shading
{
	
// op CAN READ BUT NOT MODIFY GLOBAL MEMORY, 
// IF global memory is not used/touched underlying op can be :
// STATIC_ONLINE_PURE ( __attribute__((const)) __STATIC_INLINE)
// WHICH MEANS NO global MEMORY CAN BE ACCESSED READ OR WRITE AT ALL
	
// Template for custom shade function to be used with world::RenderTinyVoxel
// this will fully inline at compile time avoiding use of function pointer and expensive function call
using shade_op_min = uint32_t const (* const)(vec3_t const, point2D_t const); // signature for all valid template params
template<shade_op_min op>
__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE uint32_t const do_shading_op_min(vec3_t const vPoint, point2D_t const vOrigin)	// just fits into 4 registers
{
	return( op(vPoint, vOrigin) );
}
// inline uint32_t const shade_op_example(vec3_t const vVoxelCoord, point2D_t vObjectOrigin) { ... return( theCalculatedShade ); }

using shade_op_default = uint32_t const (* const)(vec3_t const, uint32_t const, float const); // signature for all valid template params
template<shade_op_default op>
__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE uint32_t const do_shading_op_default(vec3_t const vPoint, uint32_t const uiNormalID, float const ShadeMaterial)
{
	return( op(vPoint, uiNormalID, ShadeMaterial) );
}
// inline uint32_t const shade_op_example(vec3_t const vScreenSpaceCoord, uint32_t const uiNormalID, float const DiffuseShade) { ... return( theCalculatedShade ); }

using shade_op_default_normal = uint32_t const (* const)(vec3_t const, vec3_t const, float const); // signature for all valid template params
template<shade_op_default_normal op>
__attribute__((always_inline)) __attribute__((pure)) STATIC_INLINE uint32_t const do_shading_op_default_normal(vec3_t const vPoint, vec3_t const vNormal, float const ShadeMaterial)
{
	return( op(vPoint, vNormal, ShadeMaterial) );
}
// inline uint32_t const shade_op_example(vec3_t const vScreenSpaceCoord, vec3_t const vNormal, float const DiffuseShade) { ... return( theCalculatedShade ); }

} // endnamespace

#endif

