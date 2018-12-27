/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#ifndef VECTOR_ROTATION_H
#define VECTOR_ROTATION_H
#include "commonmath.h"
#include "arm_optimized_math.h"
#include "point2D.h"
#include "IsoVoxel.h"

struct vec2_rotation_t {

public:
	union
	{
		struct
		{
		float c, s;		// cos, sin
		};
		
		vec2_t vR;
	};
	float Angle;	// retained in degrees
	
private:
	inline void recalculate_precise( float const fAngle )
	{
		float const fAngleRadians(TO_RADIANS(fAngle));
			
		s = sinf(fAngleRadians);		// must use precise sinf and cosf C99 functions
		c = cosf(fAngleRadians);		// precision must be maintained for the trig identity function optimizations to work properly
																// the trig identity functions do NOT accumulate properly - error also begins to accumulate
																// so they are really only an optimization for the most useful case where a discrete angle needs
		Angle = fAngle;							// to be added/subtracted from and existing angle without the recalculation of sin/cos
	}															// however cannot be used succesfively over many loops / frames / etc. as error begins to accumulate fast
																// kinda stupid if you as me, these are supposed to be identities!! but due to floating point rounding errors, the error accumulates
	
	inline void recalculate_fast( float const fAngle ) // used only during the += -= overloads, luckily the faster __sinf / __cosf can be used
	{																									 // when real-time update of angle is required.
		float const fAngleRadians(TO_RADIANS(fAngle));
			
		s = __sinf(fAngleRadians);		
		c = __cosf(fAngleRadians);													
																
		Angle = fAngle;
	}
	inline explicit vec2_rotation_t(float const fSin, float const fCos, float const fAngle)
		: s(fSin), c(fCos), Angle(fAngle)
	{}
	
public:		
	inline vec2_rotation_t()
		: s(0.0f), c(0.0f), Angle(0.0f)
	{}
	
	inline explicit vec2_rotation_t(float const fAngle) // CTOR always requires (precise) calculation of sin/cos - must be the precise version for loadtime
	{																										// or else the error introduced goes thru the roof on the trig sum / diff identity functions
		recalculate_precise(fAngle);
	}
	
	// #### following functions operate without recalculating sin / cos of angle
	
	inline vec2_rotation_t const operator-() const	// negate
	{
		// only need to negate sin, cos is always positive
		return( vec2_rotation_t(-s, c, -Angle) );
	}
	
	inline vec2_rotation_t const operator+(vec2_rotation_t const& __restrict rhs) const	// add two angles together (only good for one-shot - no accunulation)
	{
		// trig identities for sum 2 angles
		// sin(a + b) = sin(a) cos(b) + cos(a) sin(b)
		// cos(a + b) = cos(a) cos(b) - sin(a) sin(b)
		//return( vec2_rotation(Angle + rhs.Angle) ); // -- recalculates sincos ugggg
		return( vec2_rotation_t(__fma(s,rhs.c , c*rhs.s), __fms(c,rhs.c , s*rhs.s), Angle+rhs.Angle) ); // --- leverages existing sincos computations yes!
	}
	
	inline vec2_rotation_t const operator-(vec2_rotation_t const& __restrict rhs) const // subtract *this angle by another angle (only good for one-shot - no accunulation)
	{
		// trig identities for diff 2 angles
		// sin(a - b) = sin(a) cos(b) - cos(a) sin(b)
		// cos(a - b) = cos(a) cos(b) + sin(a) sin(b)
		//return( vec2_rotation(Angle - rhs.Angle) ); // -- recalculates sincos ugggg
		return( vec2_rotation_t(__fms(s,rhs.c , c*rhs.s), __fma(c,rhs.c , s*rhs.s), Angle-rhs.Angle) );	// --- leverages existing sincos computations yes!
	}
	
	/// ### the following function require recalculation of sin / cos but use the faster variants
	inline void operator+=(float const fAngle)
	{
		recalculate_fast(Angle + fAngle);
	}
	inline void operator-=(float const fAngle)
	{
		recalculate_fast(Angle - fAngle);
	}
};

extern struct sRotationConstants // good for leverage trig identity sum/diff functions. However values lower than 1 degree or if the angle is accumulated over time
{																 // the trig identity functions and these constants should not be used
	vec2_rotation_t const 	v180,	 // a good example:
													v90,	 // vec2_rotation_t vNegatedAndOffset = -vR + RotationConstants.v45
													v45,	 // a bad example:
													v1;		 // vR = vR + RotationConstants.v1
	
	sRotationConstants();
	
} const RotationConstants __attribute__((section (".dtcm_const")));

// #### the following functions operate in any given "space" used, rotating a point explicitly
STATIC_INLINE_PURE point2D_t const p2D_rotate(vec2_rotation_t const angle, point2D_t const p)
{
	// rotate point
	return(point2D_t(int32::__roundf(__fms(p.pt.x, angle.c, (float)p.pt.y * angle.s)), 
									 int32::__roundf(__fma(p.pt.x, angle.s, (float)p.pt.y * angle.c))));
}
STATIC_INLINE_PURE vec2_t const v2_rotate(vec2_rotation_t const angle, vec2_t const p)
{
	// rotate point
	return(vec2_t(__fms(p.x, angle.c, p.y * angle.s),
					      __fma(p.x, angle.s, p.y * angle.c)));
}

/// #### the following functions operate in any given "space" used, rotating with respect to an origin
STATIC_INLINE_PURE point2D_t const p2D_rotate(vec2_rotation_t const angle, point2D_t p, point2D_t const origin)
{
	// translate point back to origin:
	p = p2D_sub(p, origin);

	// rotate point
	p = p2D_rotate(angle, p);

	// translate point back:
	return(p2D_add(p, origin));
}

STATIC_INLINE_PURE vec2_t const v2_rotate(vec2_rotation_t const angle, vec2_t p, vec2_t const origin)
{
	// translate point back to origin:
	p = v2_sub(p, origin);

	// rotate point
	p = v2_rotate(angle, p);

	// translate point back:
	return(v2_add(p, origin));
}

/// #### following rotation functions operate in screenspace, rotation is done isometrically
STATIC_INLINE_PURE point2D_t const p2D_rotate_screenspace(vec2_rotation_t const angle, point2D_t const p)
{
	// rotate point
	return(v2_to_p2D_rounded(Iso::CartToISO(vec2_t(__fms(p.pt.x, angle.c, (float)p.pt.y * angle.s), 
												                         __fma(p.pt.x, angle.s, (float)p.pt.y * angle.c)))));
}
STATIC_INLINE_PURE vec2_t const v2_rotate_screenspace(vec2_rotation_t const angle, vec2_t const p)
{
	// rotate point
	return(Iso::CartToISO(vec2_t(__fms(p.x, angle.c, p.y * angle.s),
					                     __fma(p.x, angle.s, p.y * angle.c))));
}

STATIC_INLINE_PURE point2D_t const p2D_rotate_screenspace(vec2_rotation_t const angle, point2D_t p, point2D_t const origin)
{
	// translate point back to origin:
	p = p2D_sub(p, origin);

	// rotate point
	p = p2D_rotate_screenspace(angle, p);

	// translate point back:
	return(p2D_add(p, origin));
}

STATIC_INLINE_PURE vec2_t const v2_rotate_screenspace(vec2_rotation_t const angle, vec2_t p, vec2_t const origin)
{
	// translate point back to origin:
	p = v2_sub(p, origin);

	// rotate point
	p = v2_rotate_screenspace(angle, p);

	// translate point back:
	return(v2_add(p, origin));
}

#endif

