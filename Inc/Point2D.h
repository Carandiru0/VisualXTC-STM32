/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#pragma once
#ifndef POINT2D_H
#define POINT2D_H
#include <stdint.h>
#include "commonmath.h"
#include "math_3d.h"

typedef union point2D 
{
	struct sPoint2D
	{
		int16_t x,
						y;
	} pt;

	__SIMD32_TYPE v;

	inline point2D() = default;
	point2D(point2D const& a) {
		v = a.v;
	}
	point2D(point2D const&& a) {
		v = a.v;
	}
	void operator=(point2D const&) = delete;
	void operator=(point2D const&& a) {
		v = a.v;
	}
	inline explicit point2D(int32_t const x, int32_t const y) : v(__PKHBT(x, y, 16))
	{}
	inline point2D(__SIMD32_TYPE const vSrc) : v(vSrc)
	{}
} point2D_t;

STATIC_INLINE_PURE point2D_t const p2D_add(point2D_t const a, point2D_t const b) {
	return(__QADD16(a.v,b.v));
}
STATIC_INLINE_PURE point2D_t const p2D_addh(point2D_t const a, point2D_t const b) {
	return(__SHADD16(a.v,b.v));
}
STATIC_INLINE_PURE point2D_t const p2D_adds(point2D_t const a, int16_t const s) { 
	return(__QADD16(a.v, point2D_t(s, s).v));
}

STATIC_INLINE_PURE point2D_t const p2D_sub(point2D_t const a, point2D_t const b) { 
	return(__QSUB16(a.v, b.v));
}
STATIC_INLINE_PURE point2D_t const p2D_subh(point2D_t const a, point2D_t const b) { 
	return(__SHSUB16(a.v, b.v));
}
STATIC_INLINE_PURE point2D_t const p2D_subs(point2D_t const a, int16_t const s) { 
	return(__QSUB16(a.v, point2D_t(s,s).v));
}

STATIC_INLINE_PURE point2D_t const p2D_mul(point2D_t const a, point2D_t const b) { 
	return( point2D_t(a.pt.x * b.pt.x, a.pt.y * b.pt.y) ); 
}
STATIC_INLINE_PURE point2D_t const p2D_muls(point2D_t const a, int16_t const s) {
	return( point2D_t(a.pt.x * s, a.pt.y * s) );
}
STATIC_INLINE_PURE point2D_t const p2D_negate(point2D_t const a) { 
	return( point2D_t(-a.pt.x, -a.pt.y) );
}

template<uint32_t const SATBITS>
STATIC_INLINE_PURE point2D_t const p2D_saturate(point2D_t const a) { 
	return(__USAT16(a.v, SATBITS));
}
template<uint32_t const SATBITS>
STATIC_INLINE_PURE point2D_t const p2D_signed_saturate(point2D_t const a) { 
	return(__SSAT16(a.v, SATBITS));
}
/*template<uint32_t const SATBITS_X, uint32_t const SATBITS_Y>
STATIC_INLINE_PURE point2D_t const p2D_saturate(point2D_t const a) { 
	return(__USAT(a.pt.x, SATBITS_X), __USAT(a.pt.y, SATBITS_Y));
}*/
/*
template<uint32_t const SATBITS_X, uint32_t const SATBITS_Y>
STATIC_INLINE_PURE point2D_t const v2_to_p2D_saturate(vec2_t const v) {
	return( point2D_t( __USAT(int32::__floorf(v.x), SATBITS_X), __USAT(int32::__floorf(v.y), SATBITS_Y) ) );
}*/
STATIC_INLINE_PURE point2D_t const v2_to_p2D(vec2_t const v) {
	return(point2D_t(int32::__floorf(v.x), int32::__floorf(v.y)));
}
STATIC_INLINE_PURE point2D_t const v2_to_p2D_rounded(vec2_t const v) {
	return(point2D_t(int32::__roundf(v.x), int32::__roundf(v.y)));
}
STATIC_INLINE_PURE vec2_t const p2D_to_v2(point2D_t const v) {
	return(vec2_t(v.pt.x, v.pt.y));
}

 STATIC_INLINE_PURE point2D_t const p2D_CartToIso( point2D_t const pt2DSpace )
{
		/*
			_x = (_col * tile_width * .5) + (_row * tile_width * .5);
			_y = (_row * tile_hieght * .5) - ( _col * tile_hieght * .5);
		*/
	return( point2D_t( pt2DSpace.pt.y + pt2DSpace.pt.x, (pt2DSpace.pt.x >> 1) - (pt2DSpace.pt.y >> 1) ) );
}
	
 STATIC_INLINE_PURE point2D_t const p2D_IsoToCart( point2D_t const ptIsoSpace )
{
		/*
			_x = (_col * tile_width * .5) + (_row * tile_width * .5);
			_y = (_row * tile_hieght * .5) - ( _col * tile_hieght * .5);
		*/
	return( point2D_t( ((ptIsoSpace.pt.y << 1) - ptIsoSpace.pt.x) >> 1, ((ptIsoSpace.pt.y << 1) + ptIsoSpace.pt.x) >> 1 ) );
}

STATIC_INLINE_PURE point2D_t const v3_to_p2D_iso(vec3_t const v) {
	
	point2D_t a = p2D_CartToIso(point2D_t(int32::__floorf(v.x), int32::__floorf(v.z)));
	// convert to isometric point
	//point2D_t a(v.x, v.z);

	// increase zposition by height
	//a.pt.y += (v.y * 0.5f);

	// half result, as x is always 2*y
	//a.pt.y >>= 1;

	//a.pt.x >>= 1;
	
	return(a);
}

typedef union rect2D
{
	struct sRect2D
	{
		point2D_t   bl,
					br,
					tr,
					tl;
	} pts;

	point2D_t vs[4];

} rect2D_t;

typedef union diamond2D
{
	static constexpr uint32_t const BOTTOM = 0,
																	RIGHT = 1,
																	TOP = 2,
																	LEFT = 3;
	struct sDiamond2D
	{
		point2D_t   b,			// same winding order matching rect2D_t
					r,
					t,
					l;
	} pts;

	point2D_t vs[4];

	diamond2D() = default;

	diamond2D( diamond2D const& rhs )
	{
		vs[0].v = rhs.vs[0].v;
		vs[1].v = rhs.vs[1].v;
		vs[2].v = rhs.vs[2].v;
		vs[3].v = rhs.vs[3].v;
	}
	void operator=( diamond2D const& rhs )
	{
		vs[0].v = rhs.vs[0].v;
		vs[1].v = rhs.vs[1].v;
		vs[2].v = rhs.vs[2].v;
		vs[3].v = rhs.vs[3].v;
	}
} diamond2D_t;

typedef union halfdiamond2D
{
	struct sHalfDiamond2D
	{
		point2D_t   b,			// same winding order matching rect2D_t
								t;
	} pts;

	point2D_t vs[2];

	halfdiamond2D() = default;

	halfdiamond2D( halfdiamond2D const& rhs )
	{
		vs[0].v = rhs.vs[0].v;
		vs[1].v = rhs.vs[1].v;
	}
	void operator=( halfdiamond2D const& rhs )
	{
		vs[0].v = rhs.vs[0].v;
		vs[1].v = rhs.vs[1].v;
	}
	halfdiamond2D( diamond2D_t const& rhs )
	{
		vs[0].v = rhs.vs[0].v;
		vs[1].v = rhs.vs[2].v;
	}
	void operator=( diamond2D_t const& rhs )
	{
		vs[0].v = rhs.vs[0].v;
		vs[1].v = rhs.vs[2].v;
	}
} halfdiamond2D_t;

STATIC_INLINE_PURE int32_t const p2D_distanceSquared(point2D_t const vStart, point2D_t const vEnd) 
{
	point2D_t deltaSeperation = p2D_sub(vEnd, vStart);
	deltaSeperation = p2D_mul(deltaSeperation, deltaSeperation);
	
	return(deltaSeperation.pt.x + deltaSeperation.pt.y);
}

#endif


