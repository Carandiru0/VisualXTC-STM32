/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef COMMONMATH_H
#define COMMONMATH_H

#include "PreprocessorCore.h"
#include "Globals.h"

/*
void layouttemplate(int *src, int *dst, int len) {
	assert((len % 4) == 0);
	int tmp;
	// This uses the "%=" template string to create a label which can be used
	// elsewhere inside the assembly block, but which will not conflict with
	// inlined copies of it.
	// R is a C++11 raw string literal. See the example in 10.2 File-scope inline assembly.
	__asm(R"(
		.Lloop%=:
			ldr %[tmp], %[src], #4
			str %[tmp], %[dst], #4
			subs %[len], #4
			bne .Lloop%=)"
		: [dst] "=&m" (*dst),
			[tmp] "=&r" (tmp),
			[len] "+r" (len)
		: [src] "m" (*src));
}
*/
#ifdef __cplusplus
namespace MathConstants
{
	constexpr double const kPI_D = 3.14159265358979323846;
	constexpr float const kPI = (float const)kPI_D;
	constexpr float const kPIOVER180 = (float const)(kPI_D / 180.0);
	constexpr float const k180OVERPI = (float const)(180.0 / kPI_D);
}

constexpr float const TO_DEGREES( float const angleInRadians ) noexcept {

	return ( angleInRadians * MathConstants::kPIOVER180 );
}
constexpr float const TO_RADIANS( float const angleInDegrees ) noexcept {

	return ( angleInDegrees * MathConstants::k180OVERPI );
}

#endif ///__cplusplus

#define __SIMDU32_TYPE uint32_t

typedef union __attribute__((__aligned__(4))) uPixels
{
	struct sPixels
	{	
		int16_t const		p0,
										p1;
		
	} const pixels;
	
	struct sComponent
	{	
		int16_t const		Alpha,
										Luma;
		
	} const pixel;
	
	struct sDimension
	{	
		int16_t const		width,
										height;
		
	} const area;
	
	__SIMD32_TYPE v;
	
	uPixels() : v(0) {}
	__inline explicit uPixels(int16_t const inp0, int16_t const inp1) : v( __PKHBT(inp0, inp1, Constants::n16) )
	{	}
	__inline explicit uPixels(int16_t const inp01) : v( __PKHBT(inp01, inp01, Constants::n16) )
	{	}
	
	__inline void Set(int16_t const inp0, int16_t const inp1) 
	{
		v = __PKHBT(inp0, inp1, Constants::n16);
	}
	
	template <uint32_t SATBITS = Constants::SATBIT_256>
	__inline void Saturate()
	{
		v = __USAT16(v, SATBITS);
	}

} Pixels;

typedef union __attribute__((__aligned__(4))) uPixelsQuad
{
	struct sPixelsQuad
	{	
		uint8_t const 	p0,
										p1,
										p2,
										p3;
		
	} pixels;
	
	__SIMDU32_TYPE v;
	
	__inline uPixelsQuad() : v(0) {}
	__inline explicit uPixelsQuad(uint8_t const inp0, uint8_t const inp1, uint8_t const inp2, uint8_t const inp3) : v( __PKHBT( (inp1 << 8) | inp0, (inp3 << 8) | inp2, Constants::n16) )
	{	}
	__inline explicit uPixelsQuad(uint8_t const inp0123) : v( __PKHBT( (inp0123 << 8) | inp0123, (inp0123 << 8) | inp0123, Constants::n16) )
	{	}
	__inline void Set(uint8_t const inp0, uint8_t const inp1, uint8_t const inp2, uint8_t const inp3)
	{	
		v = __PKHBT( (inp1 << 8) | inp0, (inp3 << 8) | inp2, Constants::n16);
	}
	__inline void SetAndSaturate(uint8_t const inp0, uint8_t const inp1, uint8_t const inp2, uint8_t const inp3)
	{
		Pixels p01(inp0,inp1),
					 p23(inp2,inp3);
		
		p01.Saturate();
		p23.Saturate();
		
		v = __PKHBT( (p01.pixels.p1 << 8) | p01.pixels.p0, (p23.pixels.p1 << 8) | p23.pixels.p0, Constants::n16);
	}
	__inline void Invert()
	{
		// Load 255
		__SIMDU32_TYPE const v255 = __PKHBT( (Constants::n255 << 8) | Constants::n255, (Constants::n255 << 8) | Constants::n255, Constants::n16);
		v = __UQSUB8(v255, v);
	}
} PixelsQuad;

__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __fmaxf ( float32_t Sn, float32_t const Sm) // Cortex M7 Extension
{
	// & is output only early clobber
	// = is write only
	// + is read/write
	
	// t VFP floating-point registers s0-s31. Used for 32 bit values.
	// w VFP floating-point registers d0-d31 and the appropriate subset d0-d15 based on command line options. Used for 64 bit values only.
	
	__asm("vmaxnm.f32 %0, %0, %1" : "+t" (Sn) : "t" (Sm) );
	
	return(Sn);
}
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __fminf ( float32_t Sn, float32_t const Sm) // Cortex M7 Extension
{
	// & is output only early clobber
	// = is write only
	// + is read/write
	
	// t VFP floating-point registers s0-s31. Used for 32 bit values.
	// w VFP floating-point registers d0-d31 and the appropriate subset d0-d15 based on command line options. Used for 64 bit values only.
	
	__asm("vminnm.f32 %0, %0, %1" : "+t" (Sn) : "t" (Sm) );
	
	return(Sn);
}
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __fabsf ( float32_t Sn )		// faster than regular C99 fabsf
{
	// & is output only early clobber
	// = is write only
	// + is read/write
	
	// t VFP floating-point registers s0-s31. Used for 32 bit values.
	// w VFP floating-point registers d0-d31 and the appropriate subset d0-d15 based on command line options. Used for 64 bit values only.
	
	__asm("vabs.f32 %0, %0" : "+t" (Sn) );
	
	return(Sn);
}
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __sqrtf ( float32_t Sn )	// fastest sqrt I have ever seen
{
	// & is output only early clobber
	// = is write only
	// + is read/write
	
	// t VFP floating-point registers s0-s31. Used for 32 bit values.
	// w VFP floating-point registers d0-d31 and the appropriate subset d0-d15 based on command line options. Used for 64 bit values only.
	
	__asm("vsqrt.f32 %0, %0" : "+t" (Sn) );
	
	return(Sn);
}

// c math fmaf(() function broken very slow, so asre some other ones like floorf, and arm_dsp_scale
// replacement
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __fma ( float32_t const Sn, float32_t const Sm, float32_t Sd)
{
	// & is output only early clobber
	// = is write only
	// + is read/write
	
	// t VFP floating-point registers s0-s31. Used for 32 bit values.
	// w VFP floating-point registers d0-d31 and the appropriate subset d0-d15 based on command line options. Used for 64 bit values only.
	
	__asm("vfma.f32 %0, %1, %2" : "+t" (Sd) : "t" (Sn), "t" (Sm) );
	
	return(Sd);
}
// c math fmaf(() function broken very slow, so asre some other ones like floorf, and arm_dsp_scale
// replacement
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __fms ( float32_t const Sn, float32_t const Sm, float32_t Sd)
{
	// & is output only early clobber
	// = is write only
	// + is read/write
	
	// t VFP floating-point registers s0-s31. Used for 32 bit values.
	// w VFP floating-point registers d0-d31 and the appropriate subset d0-d15 based on command line options. Used for 64 bit values only.
	
	//bugfix, this has to be vfNms to get correct subtraction order... very odd but tested
	__asm("vfnms.f32 %0, %1, %2" : "+t" (Sd) : "t" (Sn), "t" (Sm) );
	
	return(Sd);
}

namespace int32 // converts to signed int from floating point
{
	// & is output only early clobber
	// = is write only
	// + is read/write
	
	// t VFP floating-point registers s0-s31. Used for 32 bit values.
	// w VFP floating-point registers d0-d31 and the appropriate subset d0-d15 based on command line options. Used for 64 bit values only.
	// 
__attribute__((always_inline)) STATIC_INLINE_PURE int32_t const __roundf(float32_t const Sn) 
{
	float32_t tmp;
	int32_t Sd;
	// VCVT - R will round to nearest as already set in FPSCR (at startup)
	// by default a (int)_floatvar_ uses VCVT without R which will round to zero
	// syntax is vcvt(r).#to type#.#from type#
	
	__asm("vcvtr.s32.f32 %[tmp], %[Sn] \n\t" 
				"vmov %[Sd], %[tmp]"
				: [Sd] "=r" (Sd), [tmp] "=&t" (tmp) : [Sn] "t" (Sn) );
	
	return(Sd);
}
__attribute__((always_inline)) STATIC_INLINE_PURE int32_t const __floorf(float32_t const Sn) // Cortex M7 Extension
{
	float32_t tmp;
	int32_t Sd;
	// VCVT - M will round to negative infinity (M for minus infinity) Directed Rounding
	// by default a (int)_floatvar_ uses VCVT without R which will round to zero
	// syntax is vcvt(m).#to type#.#from type#
	
	__asm("vcvtm.s32.f32 %[tmp], %[Sn] \n\t" 
				"vmov %[Sd], %[tmp]"
				: [Sd] "=r" (Sd), [tmp] "=&t" (tmp) : [Sn] "t" (Sn) );
	
	return(Sd);
}
__attribute__((always_inline)) STATIC_INLINE_PURE int32_t const __ceilf(float32_t const Sn) // Cortex M7 Extension
{
	float32_t tmp;
	int32_t Sd;
	// VCVT - P will round to positive infinity (P for plus infinity) Directed Rounding
	// by default a (int)_floatvar_ uses VCVT without R which will round to zero
	// syntax is vcvt(p).#to type#.#from type#
	
	__asm("vcvtp.s32.f32 %[tmp], %[Sn] \n\t" 
				"vmov %[Sd], %[tmp]"
				: [Sd] "=r" (Sd), [tmp] "=&t" (tmp) : [Sn] "t" (Sn) );
	
	return(Sd);
}
} // endnamepsace
namespace uint32 // converts to unsigned int from floating point
{
	// & is output only early clobber
	// = is write only
	// + is read/write
	
	// t VFP floating-point registers s0-s31. Used for 32 bit values.
	// w VFP floating-point registers d0-d31 and the appropriate subset d0-d15 based on command line options. Used for 64 bit values only.
__attribute__((always_inline)) STATIC_INLINE_PURE uint32_t const __roundf(float32_t const Sn) 
{
	float32_t tmp;
	uint32_t Sd;
	// VCVT - R will round to nearest as already set in FPSCR (at startup)
	// by default a (int)_floatvar_ uses VCVT without R which will round to zero
	// syntax is vcvt(r).#to type#.#from type#
	
	__asm("vcvtr.u32.f32 %[tmp], %[Sn] \n\t" 
				"vmov %[Sd], %[tmp]"
				: [Sd] "=r" (Sd), [tmp] "=&t" (tmp) : [Sn] "t" (Sn) );
	
	return(Sd);
}
__attribute__((always_inline)) STATIC_INLINE_PURE uint32_t const __floorf(float32_t const Sn) // Cortex M7 Extension
{
	float32_t tmp;
	uint32_t Sd;
	// VCVT - M will round to negative infinity (M for minus infinity) Directed Rounding
	// by default a (int)_floatvar_ uses VCVT without R which will round to zero
	// syntax is vcvt(m).#to type#.#from type#
	
	__asm("vcvtm.u32.f32 %[tmp], %[Sn] \n\t" 
				"vmov %[Sd], %[tmp]"
				: [Sd] "=r" (Sd), [tmp] "=&t" (tmp) : [Sn] "t" (Sn) );
	
	return(Sd);
}
__attribute__((always_inline)) STATIC_INLINE_PURE uint32_t const __ceilf(float32_t const Sn) // Cortex M7 Extension
{
	float32_t tmp;
	uint32_t Sd;
	// VCVT - P will round to positive infinity (P for plus infinity) Directed Rounding
	// by default a (int)_floatvar_ uses VCVT without R which will round to zero
	// syntax is vcvt(p).#to type#.#from type#
	
	__asm("vcvtp.u32.f32 %[tmp], %[Sn] \n\t" 
				"vmov %[Sd], %[tmp]"
				: [Sd] "=r" (Sd), [tmp] "=&t" (tmp) : [Sn] "t" (Sn) );
	
	return(Sd);
}
} // end namespace
namespace float32			// remains floating point in return value
{
	// & is output only early clobber
	// = is write only
	// + is read/write
	
	// t VFP floating-point registers s0-s31. Used for 32 bit values.
	// w VFP floating-point registers d0-d31 and the appropriate subset d0-d15 based on command line options. Used for 64 bit values only.
	
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __roundf(float32_t Sn) 
{
	// VRINT - R will round to nearest as already set in FPSCR (at startup), remains as float format
	// by default a (int)_floatvar_ uses VCVT without R which will round to zero
	
	__asm("vrintr.f32 %0, %0"
				: "+t" (Sn) );
	
	return(Sn);
}
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __floorf(float32_t Sn) // Cortex M7 Extension
{
	// VRINT - M will round to negative infinity (M for minus infinity) Directed Rounding, remains as float format
	// by default a (int)_floatvar_ uses VCVT without R which will round to zero
	
	__asm("vrintm.f32 %0, %0"
				          : "+t" (Sn) );
	
	return(Sn);
}
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __ceilf(float32_t Sn) // Cortex M7 Extension
{
	// VRINT - M will round to positive infinity (P for plus infinity) Directed Rounding, remains as float format
	// by default a (int)_floatvar_ uses VCVT without R which will round to zero
	
	__asm("vrintp.f32 %0, %0"
				: "+t" (Sn) );
	
	return(Sn);
}
} // end namespace
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __fractf(float32_t const Sn) 
{
	return( __fabsf(Sn - float32::__floorf(Sn)) );	// faster unsigned fractional part
}
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __sfractf(float32_t const Sn) 
{
	return( Sn - float32::__floorf(Sn) );	// faster signed fractional part
}
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const __modf(float32_t const Sn, float32_t& fIntegral) 
{
	// returns unsigned fractional part, store integral part in fIntegral
	
	float const fFractional = __fractf(Sn);
	fIntegral = Sn - fFractional;
	return(fFractional); // faster than C99 modff
}
__attribute__((always_inline)) STATIC_INLINE_PURE float const clampf(float Sn)	// 0.0f => 1.0f clamp, // Cortex M7 Extension
{
	__asm("vmaxnm.f32 %0, %0, %1 \n\t" 
				"vminnm.f32 %0, %0, %2"
				: "+t" (Sn) : "t" (0.0f), "t" (1.0f) );
	
	//return( Sn >= 0.0f && Sn <= 1.0f ? Sn : (float32_t)(Sn > 0.0f) );
	return( Sn );
}
__attribute__((always_inline)) STATIC_INLINE_PURE float const clampf(float Sn, float const Sm)	// 0.0f => B clamp, // Cortex M7 Extension
{
	__asm("vmaxnm.f32 %0, %0, %1 \n\t" 
				"vminnm.f32 %0, %0, %2"
				: "+t" (Sn) : "t" (0.0f), "t" (Sm) );
	
	//return( Sn >= 0.0f && Sn <= Sm ? Sn : ((float32_t)(Sn > 0.0f))*Sm );
	return( Sn );
}
__attribute__((always_inline)) STATIC_INLINE_PURE float const clampf(float Sn, float const Smin, float const Smax)	// min => max clamp, // Cortex M7 Extension
{
	__asm("vmaxnm.f32 %0, %0, %1 \n\t" 
				"vminnm.f32 %0, %0, %2"
				: "+t" (Sn) : "t" (Smin), "t" (Smax) );
	//Sn = __fmaxf( Sn, Smin );
	//Sn = __fminf( Sn, Smax );
	return( Sn );
}
__attribute__((always_inline)) STATIC_INLINE_PURE int32_t const clamp(int32_t const A, int32_t const B) // 0 => B clamp
{
	//return n >= T(0) && n <= b ? n : T(n > T(0))*b;
	return( A >= 0 && A <= B ? A : ((int32_t)(A > 0))*B );
}

// tNorm = current time / total duration = [0.0f... 1.0f] //
// Precise method which guarantees v = v1 when t = 1.
//  lerp_3(t, a, b) = fma(t, b, fma(-t, a, a))
__attribute__((always_inline)) STATIC_INLINE_PURE float32_t const lerp ( float32_t A, float32_t const B, float32_t const tNorm)			// fmaf broken, really slow, this compiles to vmla.f32
{
	float32_t tmp;
	// & is output only early clobber
	// = is write only
	// + is read/write
	
	__asm("vneg.f32 %[tmp], %[tNorm] \n\t"				// s3 = -tNorm
				"vfma.f32 %[A], %[tmp], %[A] \n\t"		// A = A + (-tNorm * A)
				"vfma.f32 %[A], %[tNorm], %[B]"					// A = A + (tNorm * b)
				: [A] "+t" (A), [tmp] "=&t" (tmp)
				: [tNorm] "t" (tNorm), [B] "t" (B) );
	
	//A = __fma(tNorm, B, __fma(-tNorm, A, A));
	return(A);
}

__attribute__((always_inline)) STATIC_INLINE_PURE float const mix(float const A, float const B, float const weight)
{
	//return T((S(1)-weight)*a+weight*b);
	// lerp_1(t, a, b) = fmaf(t, b, (1 - t)*a) ==  fma(t, v1, fma(-t, v0, v0));
	return( __fma( (1.0f - weight), A , (weight * B) ) ); // a little bit faster...
	//return( lerp(A, B, weight) );
}

// tNorm = current time / total duration = [0.0f... 1.0f] //
__attribute__((always_inline)) STATIC_INLINE_PURE float const smoothstep(float const A, float const B, float const tNorm)
{
  float const t = tNorm*tNorm*(3.0f - 2.0f*tNorm);
  
  return( lerp(A, B, t) );
}

// circular interpolation
// recommend ease_in on one component(x|y)
// and ease_inout on the other component (x,y)
// tNorm = current time / total duration = [0.0f... 1.0f] //
// accelerate from A
__attribute__((always_inline)) STATIC_INLINE_PURE float const ease_in_circular(float const A, float const B, float const tNorm)
{
	#define Delta (B-A)
  
  return( -Delta * (__sqrtf( 1.0f - tNorm*tNorm ) - 1.0f) + A);
#undef Delta
}
// decelerate to B
__attribute__((always_inline)) STATIC_INLINE_PURE float const ease_out_circular(float const A, float const B, float tNorm)
{
	#define Delta (B-A)
	--tNorm;
  return( Delta * __sqrtf( 1.0f - tNorm*tNorm ) + A);
#undef Delta
}
// accel to 0.5 , decel from 0.5 (of A to B)
__attribute__((always_inline)) STATIC_INLINE_PURE float const ease_inout_circular(float const A, float const B, float tNorm)
{
	#define Delta ((B-A)*0.5f)
	tNorm *= 2.0f;
	if ( tNorm < 1.0f )
		return( -Delta * (__sqrtf( 1.0f - tNorm*tNorm ) - 1.0f) + A);
  tNorm -= 2.0f;
	return( Delta * (__sqrtf( 1.0f - tNorm*tNorm ) + 1.0f) + A);
#undef Delta
}

// quadratic interpolation
// recommend ease_in on one component(x|y)
// and ease_inout on the other component (x,y)
// tNorm = current time / total duration = [0.0f... 1.0f] //
// accelerate from A
__attribute__((always_inline)) STATIC_INLINE_PURE float const ease_in_quadratic(float const A, float const B, float const tNorm)
{
	#define Delta (B-A)
  
  return( Delta * tNorm*tNorm + A);
#undef Delta
}
// decelerate to B
__attribute__((always_inline)) STATIC_INLINE_PURE float const ease_out_quadratic(float const A, float const B, float tNorm)
{
	#define Delta (B-A)
  return( -Delta * tNorm*(tNorm-2.0f) + A);
#undef Delta
}
// accel to 0.5 , decel from 0.5 (of A to B)
__attribute__((always_inline)) STATIC_INLINE_PURE float const ease_inout_quadratic(float const A, float const B, float tNorm)
{
	#define Delta ((B-A)*0.5f)
	tNorm *= 2.0f;
	if ( tNorm < 1.0f )
		return( Delta * tNorm*tNorm + A);
  --tNorm;
	return( -Delta * (tNorm*(tNorm-2.0f) - 1.0f) + A);
#undef Delta
}

__attribute__((always_inline)) STATIC_INLINE_PURE uint32_t const blend(uint32_t const A, uint32_t const B, uint32_t const weight)
{	
	return( __USAT(A + (weight * ( (B - A) + 127 ) / 255), 8) );
}
__attribute__((always_inline)) STATIC_INLINE_PURE int const min(int const x, int const y)
{
  return( y ^ ((x ^ y) & -(x < y)) );
}
__attribute__((always_inline)) STATIC_INLINE_PURE int const max(int const x, int const y)
{
  return( x ^ ((x ^ y) & -(x < y)) );
}
__attribute__((always_inline)) STATIC_INLINE_PURE int const absolute(int const src)
{
	return( __QSUB(src^src >> 31, src >> 31) );
}

// ### __powf resides in arm optimized math to mitigate circular header include issue ### //

__attribute__((always_inline)) STATIC_INLINE_PURE int const RoundToMultiple(int const n, int const multiple)
{
	return( n + multiple - 1 - (n - 1) % multiple );
	//return( ((n + (multiple>>1))/multiple) * multiple );
}

constexpr uint64_t const sqrt_helper(uint64_t x, uint64_t lo, uint64_t hi)
{
	#define MID ((lo + hi + 1) / 2)
  return lo == hi ? lo : ((x / MID < MID)
      ? sqrt_helper(x, lo, MID - 1) : sqrt_helper(x, MID, hi));
	#undef MID
}
// compile time 
constexpr uint64_t const ct_sqrt(uint64_t x)
{
  return sqrt_helper(x, 0, x / 2 + 1);
}

constexpr double const sqrt_helperf(double x, double lo, double hi)
{
	#define MID ((lo + hi + 1.0) / 2.0)
  return lo == hi ? lo : ((x / MID < MID)
      ? sqrt_helper(x, lo, MID - 1.0) : sqrt_helper(x, MID, hi));
	#undef MID
}
constexpr double const ct_sqrtf(double x)
{
  return sqrt_helperf(x, 0.0, x / 2.0 + 1.0);
}

//fast approximation, with very good accuracy (quake 3 fast reciprocal square root ( 1.0 / sqrt(x) ))
__attribute__((always_inline)) STATIC_INLINE_PURE float const __rsqrtf( float const number )
{
	static constexpr uint32_t const MNUM = 0x5F375A86;
	static constexpr float const threehalfs = 1.5f,
															 half = 0.5f;
	union {
		float f;
		uint32_t i;
	} conv;
	
	float const x2(number * half);

	conv.f  = number;
	conv.i  = MNUM - ( conv.i >> 1 );
	
	return( conv.f * ( threehalfs - ( x2 * conv.f * conv.f ) ) );
}
#endif /*COMMONMATH_H*/

