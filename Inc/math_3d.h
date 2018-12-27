/* Modified by Jason Tully
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

/**

Math 3D v1.0
By Stephan Soller <stephan.soller@helionweb.de> and Tobias Malmsheimer
Licensed under the MIT license

Math 3D is a compact C99 library meant to be used with OpenGL. It provides basic
3D vector and 4x4 matrix operations as well as functions to create transformation
and projection matrices. The OpenGL binary layout is used so you can just upload
vectors and matrices into shaders and work with them without any conversions.

It's an stb style single header file library. Define MATH_3D_IMPLEMENTATION
before you include this file in *one* C file to create the implementation.


QUICK NOTES

- If not explicitly stated by a parameter name all angles are in radians.
- The matrices use column-major indices. This is the same as in OpenGL and GLSL.
  The matrix documentation below for details.
- Matrices are passed by value. This is probably a bit inefficient but
  simplifies code quite a bit. Most operations will be inlined by the compiler
  anyway so the difference shouldn't matter that much. A matrix fits into 4 of
  the 16 SSE2 registers anyway. If profiling shows significant slowdowns the
  matrix type might change but ease of use is more important than every last
  percent of performance.
- When combining matrices with multiplication the effects apply right to left.
  This is the convention used in mathematics and OpenGL. Source:
  https://en.wikipedia.org/wiki/Transformation_matrix#Composing_and_inverting_transformations
  Direct3D does it differently.
- The `m4_mul_pos()` and `m4_mul_dir()` functions do a correct perspective
  divide (division by w) when necessary. This is a bit slower but ensures that
  the functions will properly work with projection matrices. If profiling shows
  this is a bottleneck special functions without perspective division can be
  added. But the normal multiplications should avoid any surprises.
- The library consistently uses a right-handed coordinate system. The old
  `glOrtho()` broke that rule and `m4_ortho()` has be slightly modified so you
  can always think of right-handed cubes that are projected into OpenGLs
  normalized device coordinates.
- Special care has been taken to document all complex operations and important
  sources. Most code is covered by test cases that have been manually calculated
  and checked on the whiteboard. Since indices and math code is prone to be
  confusing we used pair programming to avoid mistakes.


FURTHER IDEARS

These are ideas for future work on the library. They're implemented as soon as
there is a proper use case and we can find good names for them.

- bool v3_is_null(vec3_t v, float epsilon)
  To check if the length of a vector is smaller than `epsilon`.
- vec3_t v3_length_default(vec3_t v, float default_length, float epsilon)
  Returns `default_length` if the length of `v` is smaller than `epsilon`.
  Otherwise same as `v3_length()`.
- vec3_t v3_norm_default(vec3_t v, vec3_t default_vector, float epsilon)
  Returns `default_vector` if the length of `v` is smaller than `epsilon`.
  Otherwise the same as `v3_norm()`.
- mat4_t m4_invert(mat4_t matrix)
  Matrix inversion that works with arbitrary matrices. `m4_invert_affine()` can
  already invert translation, rotation, scaling, mirroring, reflection and
  shearing matrices. So a general inversion might only be useful to invert
  projection matrices for picking. But with orthographic and perspective
  projection it's probably simpler to calculate the ray into the scene directly
  based on the screen coordinates.


VERSION HISTORY

v1.0  2016-02-15  Initial release

**/

#ifndef MATH_3D_HEADER
#define MATH_3D_HEADER
#define MATH_3D_NORMALIZE_USE_RSQRTF 
#include "commonmath.h"
#include "arm_optimized_math.h"

typedef union uVector2 // legacy do not use, use vec2_t, vec3_t instead
{
	struct sVector2
	{	
		float32_t		x,
								y;
	} pt;
	
	float32_t v[2];
	
	uVector2() : v{} {}
	
	__inline explicit uVector2(float32_t const fX, float32_t const fY)
		: v{fX, fY}
	{ }
	
} Vector2;
STATIC_INLINE void SetVector2( Vector2* const __restrict pV, float32_t const x, float32_t const y )
{
	(pV->v[0]) = x; (pV->v[1]) = y;
}
typedef union uVector4 // legacy do not use
{
	struct sVector4
	{	
		float32_t		x,
								y,
								z,
								w;
	} pt;
	
	float32_t v[4];
	
	uVector4() : v{} {}
	__inline explicit uVector4(float32_t const fX, float32_t const fY, float32_t const fZ, float32_t const fW)	
		: v{fX, fY, fZ, fW}
	{ }
} Vector4;


//
// 2D & 3D vectors
// 
// Use the `vec3()` function to create vectors. All other vector functions start
// with the `v3_` prefix.
// 
// The binary layout is the same as in GLSL and everything else (just 3 floats).
// So you can just upload the vectors into shaders as they are.
//
	
typedef union vec3_t { struct { float x, y, z; }; float v[3];	// ensure alignment and memory locality
											 inline vec3_t(float const xx = 0.0f, float const yy = 0.0f, float const& zz = 0.0f) :v{xx,yy,zz}{} } vec3_t;

STATIC_INLINE_PURE vec3_t const v3_add   (vec3_t const a, vec3_t const b)          { return(vec3_t( a.x + b.x, a.y + b.y, a.z + b.z )); }
STATIC_INLINE_PURE vec3_t const v3_adds  (vec3_t const a, float const s)           { return(vec3_t( a.x + s,   a.y + s,   a.z + s   )); }
STATIC_INLINE_PURE vec3_t const v3_sub   (vec3_t const a, vec3_t const b)          { return(vec3_t( a.x - b.x, a.y - b.y, a.z - b.z )); }
STATIC_INLINE_PURE vec3_t const v3_subs  (vec3_t const a, float const s)           { return(vec3_t( a.x - s,   a.y - s,   a.z - s   )); }
STATIC_INLINE_PURE vec3_t const v3_mul   (vec3_t const a, vec3_t const b)          { return(vec3_t( a.x * b.x, a.y * b.y, a.z * b.z )); }
STATIC_INLINE_PURE vec3_t const v3_muls  (vec3_t const a, float const s)           { return(vec3_t( a.x * s,   a.y * s,   a.z * s   )); }
STATIC_INLINE_PURE vec3_t const v3_fmas  (float const s, vec3_t const a, vec3_t const b ) { return(vec3_t( __fma(s, a.x, b.x),    __fma(s, a.y, b.y),    __fma(s, a.z, b.z) )); }
STATIC_INLINE_PURE vec3_t const v3_fmas  (float const s, vec3_t const a, float const b ) { return(vec3_t( __fma(s, a.x, b),    __fma(s, a.y, b),    __fma(s, a.z, b) )); }
STATIC_INLINE_PURE vec3_t const v3_div   (vec3_t const a, vec3_t const b)          { return(vec3_t( a.x / b.x, a.y / b.y, a.z / b.z )); }
STATIC_INLINE_PURE vec3_t const v3_divs  (vec3_t const a, float const s)           { return(vec3_t( a.x / s,   a.y / s,   a.z / s   )); }
STATIC_INLINE_PURE vec3_t const v3_inverse  (vec3_t const a)           						 { return(vec3_t( 1.0f/a.x,   1.0f/a.y,   1.0f/a.z   )); }
STATIC_INLINE_PURE vec3_t const v3_negate(vec3_t const a)						   						 { return(vec3_t( -a.x,   -a.y,   -a.z )); }
STATIC_INLINE_PURE vec3_t const v3_floor(vec3_t const a) 						   						 { return(vec3_t( float32::__floorf(a.x), float32::__floorf(a.y), float32::__floorf(a.z) )); }
STATIC_INLINE_PURE vec3_t const v3_ceil(vec3_t const a) 						   						 { return(vec3_t( float32::__ceilf(a.x), float32::__ceilf(a.y), float32::__ceilf(a.z) )); }
STATIC_INLINE_PURE vec3_t const v3_fract(vec3_t const a) 						   						 { return(vec3_t( __fractf(a.x), __fractf(a.y), __fractf(a.z) )); }
STATIC_INLINE_PURE vec3_t const v3_sfract(vec3_t const a) 						   						 { return(vec3_t( __sfractf(a.x), __sfractf(a.y), __sfractf(a.z) )); }
STATIC_INLINE_PURE vec3_t const v3_clamp(vec3_t const a, float const min, float const max) { return(vec3_t( clampf(a.x, min, max), clampf(a.y, min, max), clampf(a.z, min, max) )); }

STATIC_INLINE_PURE float  const v3_dot   (vec3_t const a, vec3_t const b)          { return(__fma(a.x,b.x, __fma(a.y,b.y, (a.z*b.z)))); }
STATIC_INLINE_PURE float const  v3_norm       (vec3_t const v);
STATIC_INLINE_PURE float  const v3_length     (vec3_t const v);
STATIC_INLINE_PURE float const  v3_distanceSquared(vec3_t const vDisplacement);
STATIC_INLINE_PURE vec3_t const v3_normalize_fast  (vec3_t const v);
STATIC_INLINE_PURE vec3_t const v3_normalize  (vec3_t const v);
STATIC_INLINE_PURE vec3_t const v3_normalize  (vec3_t const v, float* const __restrict pLength) __attribute__((__nonnull__(2)));
STATIC_INLINE_PURE vec3_t const v3_lerp(vec3_t const a, vec3_t const b, float const t);

STATIC_INLINE_PURE vec3_t const v3_proj  (vec3_t const v, vec3_t const onto);
STATIC_INLINE_PURE vec3_t const v3_cross (vec3_t const a, vec3_t const b);
STATIC_INLINE_PURE float  const v3_angle_between(vec3_t const a, vec3_t const b);

typedef union vec2_t { struct { float x, y; }; float v[2];	// ensure alignment and memory locality
												inline vec2_t(Vector2 const& rr) : v{rr.v[0], rr.v[1]} {}
												inline vec2_t(vec3_t const vv) : v{vv.x,vv.y}{}
												inline vec2_t(float const xx = 0.0f, float const yy = 0.0f) : v{xx,yy}{} } vec2_t;

STATIC_INLINE_PURE vec2_t const v2_add(vec2_t const a, vec2_t const b) { return(vec2_t( a.x + b.x, a.y + b.y )); }
STATIC_INLINE_PURE vec2_t const v2_adds(vec2_t const a, float const s) { return(vec2_t( a.x + s,   a.y + s )); }
STATIC_INLINE_PURE vec2_t const v2_sub(vec2_t const a, vec2_t const b) { return(vec2_t( a.x - b.x, a.y - b.y )); }
STATIC_INLINE_PURE vec2_t const v2_subs(vec2_t const a, float const s) { return(vec2_t( a.x - s,   a.y - s )); }
STATIC_INLINE_PURE vec2_t const v2_mul(vec2_t const a, vec2_t const b) { return(vec2_t( a.x * b.x, a.y * b.y )); }
STATIC_INLINE_PURE vec2_t const v2_muls(vec2_t const a, float const s) { return(vec2_t( a.x * s,   a.y * s )); }
STATIC_INLINE_PURE vec2_t const v2_fmas(float const s, vec2_t const a, vec2_t const b) { return(vec2_t( __fma(s, a.x, b.x), __fma(s, a.y, b.y) )); }
STATIC_INLINE_PURE vec2_t const v2_fmas(float const s, vec2_t const a, float const b) { return(vec2_t( __fma(s, a.x, b), __fma(s, a.y, b) )); }
STATIC_INLINE_PURE vec2_t const v2_div(vec2_t const a, vec2_t const b) { return(vec2_t( a.x / b.x, a.y / b.y )); }
STATIC_INLINE_PURE vec2_t const v2_divs(vec2_t const a, float const s) { return(vec2_t( a.x / s,   a.y / s )); }
STATIC_INLINE_PURE vec2_t const v2_inverse  (vec3_t const a)           { return(vec2_t( 1.0f/a.x,   1.0f/a.y )); }
STATIC_INLINE_PURE vec2_t const v2_negate(vec2_t const a) 						 { return(vec2_t( -a.x,   -a.y )); }
STATIC_INLINE_PURE vec2_t const v2_floor(vec2_t const a) 						   { return(vec2_t( float32::__floorf(a.x),   float32::__floorf(a.y) )); }
STATIC_INLINE_PURE vec2_t const v2_ceil(vec2_t const a) 						   { return(vec2_t( float32::__ceilf(a.x),   float32::__ceilf(a.y) )); }
STATIC_INLINE_PURE vec2_t const v2_fract(vec2_t const a) 						   { return(vec2_t( __fractf(a.x),   __fractf(a.y) )); }
STATIC_INLINE_PURE vec2_t const v2_sfract(vec2_t const a) 						   { return(vec2_t( __sfractf(a.x),   __sfractf(a.y) )); }
STATIC_INLINE_PURE vec2_t const v2_mod(vec2_t const a, vec2_t& integral) { return( vec2_t( __modf(a.x, integral.x), __modf(a.y, integral.y) ) ); }
STATIC_INLINE_PURE vec2_t const v2_clamp(vec2_t const a, float const min, float const max) { return(vec2_t( clampf(a.x, min, max), clampf(a.y, min, max) )); }
	
STATIC_INLINE_PURE float  const v2_dot(vec2_t const a, vec2_t const b) { return(__fma(a.x,b.x, a.y*b.y)); }
STATIC_INLINE_PURE float const  v2_norm(vec2_t const v);
STATIC_INLINE_PURE float  const v2_length(vec2_t const v);
STATIC_INLINE_PURE float const  v2_distanceSquared(vec2_t const vDisplacement);
STATIC_INLINE_PURE vec2_t const v2_normalize_fast(vec2_t const v);
STATIC_INLINE_PURE vec2_t const v2_normalize(vec2_t const v);
STATIC_INLINE_PURE vec2_t const v2_normalize(vec2_t const v, float* const __restrict pLength) __attribute__((__nonnull__(2)));
STATIC_INLINE_PURE vec2_t const v2_lerp(vec2_t const a, vec2_t const b, float const t);
	
//
// 4x4 matrices
//
// Use the `mat4()` function to create a matrix. You can write the matrix
// members in the same way as you would write them on paper or on a whiteboard:
// 
// mat4_t m = mat4(
//     1,  0,  0,  7,
//     0,  1,  0,  5,
//     0,  0,  1,  3,
//     0,  0,  0,  1
// )
// 
// This creates a matrix that translates points by vec3(7, 5, 3). All other
// matrix functions start with the `m4_` prefix. Among them functions to create
// identity, translation, rotation, scaling and projection matrices.
// 
// The matrix is stored in column-major order, just as OpenGL expects. Members
// can be accessed by indices or member names. When you write a matrix on paper
// or on the whiteboard the indices and named members correspond to these
// positions:
// 
// | m[0][0]  m[1][0]  m[2][0]  m[3][0] |
// | m[0][1]  m[1][1]  m[2][1]  m[3][1] |
// | m[0][2]  m[1][2]  m[2][2]  m[3][2] |
// | m[0][3]  m[1][3]  m[2][3]  m[3][3] |
// 
// | m00  m10  m20  m30 |
// | m01  m11  m21  m31 |
// | m02  m12  m22  m32 |
// | m03  m13  m23  m33 |
// 
// The first index or number in a name denotes the column, the second the row.
// So m[i][j] denotes the member in the ith column and the jth row. This is the
// same as in GLSL (source: GLSL v1.3 specification, 5.6 Matrix Components).
//

typedef union mat4_t {
	// The first index is the column index, the second the row index. The memory
	// layout of nested arrays in C matches the memory layout expected by OpenGL.
	float m[4][4];
	// OpenGL expects the first 4 floats to be the first column of the matrix.
	// So we need to define the named members column by column for the names to
	// match the memory locations of the array elements.
	struct {
		float m00, m01, m02, m03;
		float m10, m11, m12, m13;
		float m20, m21, m22, m23;
		float m30, m31, m32, m33;
	};
	
	inline mat4_t() = default;
	inline mat4_t(
		float const& mm00, float const& mm10, float const& mm20, float const& mm30,
		float const& mm01, float const& mm11, float const& mm21, float const& mm31,
		float const& mm02, float const& mm12, float const& mm22, float const& mm32,
		float const& mm03, float const& mm13, float const& mm23, float const& mm33) 
			:
		/*.m[0][0] =*/ m00(mm00), /*.m[1][0] =*/ m10(mm10), /*.m[2][0] =*/ m20(mm20), /*.m[3][0] =*/ m30(mm30),
		/*.m[0][1] =*/ m01(mm01), /*.m[1][1] =*/ m11(mm11), /*.m[2][1] =*/ m21(mm21), /*.m[3][1] =*/ m31(mm31),
		/*.m[0][2] =*/ m02(mm02), /*.m[1][2] =*/ m12(mm22), /*.m[2][2] =*/ m22(mm22), /*.m[3][2] =*/ m32(mm32),
		/*.m[0][3] =*/ m03(mm03), /*.m[1][3] =*/ m13(mm33), /*.m[2][3] =*/ m23(mm23), /*.m[3][3] =*/ m33(mm33)
	{}
} mat4_t;

STATIC_INLINE_PURE mat4_t const m4_identity     ();
STATIC_INLINE_PURE mat4_t const m4_translation  (vec3_t const offset);
STATIC_INLINE_PURE mat4_t const m4_scaling      (vec3_t const scale);
STATIC_INLINE_PURE mat4_t const m4_rotation_x   (float const angle_in_deg);
STATIC_INLINE_PURE mat4_t const m4_rotation_y   (float const angle_in_deg);
STATIC_INLINE_PURE mat4_t const m4_rotation_z   (float const angle_in_deg);
STATIC_INLINE_PURE mat4_t const m4_transpose    (mat4_t const matrix);
STATIC_INLINE_PURE mat4_t const m4_mul          (mat4_t const a, mat4_t const b);

mat4_t const m4_rotation     (float const angle_in_deg, vec3_t const axis);
mat4_t const m4_ortho        (float const left, float const right, float const bottom, float const top, float const back, float const front);
mat4_t const m4_perspective  (float const vertical_field_of_view_in_deg, float const aspect_ratio, float const near_view_distance, float const far_view_distance);
mat4_t const m4_look_at      (vec3_t const from, vec3_t const to, vec3_t const up);

mat4_t const m4_invert_affine(mat4_t const matrix);
vec3_t const m4_mul_pos      (mat4_t const matrix, vec3_t const position);
vec3_t const m4_mul_dir      (mat4_t const matrix, vec3_t const direction);

typedef struct quat_t
{
	vec3_t	v;	// imag
	float	s;	// real
	
	inline quat_t() = default;
	inline quat_t(vec3_t const vv, float const ss) 
		: v(vv), s(ss)
	{}
} quat_t;

STATIC_INLINE_PURE quat_t const quat(float const theta_x, float const theta_y, float const theta_z);
STATIC_INLINE_PURE quat_t const quat(vec3_t const vThetas) { return(quat(vThetas.x, vThetas.y, vThetas.z)); }
STATIC_INLINE_PURE quat_t const quat(vec3_t const vAxis, float const angle) { return(quat_t( v3_muls(vAxis, ARM__sinf(angle*0.5f)), ARM__cosf(angle*0.5f) )); }

STATIC_INLINE_PURE quat_t const quat_add(quat_t const a, quat_t const b) { return(quat_t( v3_add(a.v, b.v), a.s+b.s )); }
STATIC_INLINE_PURE quat_t const quat_sub(quat_t const a, quat_t const b) { return(quat_t( v3_sub(a.v, b.v), a.s - b.s )); }
STATIC_INLINE_PURE quat_t const quat_muls(quat_t const a, float const s) { return(quat_t( v3_muls(a.v, s), a.s * s )); }

STATIC_INLINE_PURE float const  quat_norm(quat_t const q) { return(__fma(q.s, q.s, v3_dot(q.v, q.v))); }
STATIC_INLINE_PURE float const  quat_length(quat_t const q);
STATIC_INLINE_PURE quat_t const quat_normalize(quat_t const q);


//
// 2D & 3D vector functions header implementation
//
STATIC_INLINE_PURE float const v2_norm(vec2_t const v) {

	return(__fma(v.x, v.x, v.y*v.y));
}

STATIC_INLINE_PURE float const v2_length(vec2_t const v) { 
	
	float const fNorm = v2_norm(v);
	
	if (fNorm > 0.0f) {
		return( __sqrtf(fNorm) );
	}
	
	return(0.0f);        
}
STATIC_INLINE_PURE float const v2_distanceSquared(vec2_t const vDisplacement) {
	return(v2_norm(vDisplacement));
}
STATIC_INLINE_PURE vec2_t const v2_normalize_fast(vec2_t const v) {
	
	float const fNorm = v2_norm(v);
	
	if (fNorm > 0.0f) {
#ifdef MATH_3D_NORMALIZE_USE_RSQRTF
		float const fInvLength = __rsqrtf( fNorm );
#else
		float const fInvLength = 1.0f / __sqrtf( fNorm );
#endif		
		return( vec2_t( v.x * fInvLength, v.y * fInvLength ) );
	}
	else
		return( vec2_t() );
}
STATIC_INLINE_PURE vec2_t const v2_normalize(vec2_t const v) {
	
	float const fNorm = v2_norm(v);
	
	if (fNorm > 0.0f) {

		float const fInvLength = 1.0f / __sqrtf( fNorm );
	
		return( vec2_t( v.x * fInvLength, v.y * fInvLength ) );
	}
	else
		return( vec2_t() );
}
__attribute__((__nonnull__(2))) STATIC_INLINE_PURE vec2_t const v2_normalize(vec2_t const v, float* const __restrict pLength) {
	
	float const fNorm = v2_norm(v);
	
	if (fNorm > 0.0f) {
		float fInvLength;
		
		fInvLength = __sqrtf( fNorm );
		// Save/output length if param specified
		*pLength = fInvLength;
		
		fInvLength = 1.0f / fInvLength;
		
		return( vec2_t( v.x * fInvLength, v.y * fInvLength ) );
	}
	else
		return( vec2_t() );
}

STATIC_INLINE_PURE vec2_t const v2_lerp(vec2_t const a, vec2_t const b, float const t)
{
	// (a*(1 - t) + b * t)
	// fma(t, b, fma(-t, a, a))
	return(v2_fmas(t, b, v2_fmas(-t, a, a)));
}

STATIC_INLINE_PURE float const v3_norm(vec3_t const v) {

	return(__fma(v.x, v.x, __fma(v.y, v.y, (v.z*v.z))));
}

STATIC_INLINE_PURE float const v3_length(vec3_t const v) { 
	
	float const fNorm = v3_norm(v);
	
	if (fNorm > 0.0f) {
		return( __sqrtf(fNorm) );
	}
	
	return(0.0f);        
}
STATIC_INLINE_PURE float const v3_distanceSquared(vec3_t const vDisplacement) {
	return(v3_norm(vDisplacement));
}
STATIC_INLINE_PURE vec3_t const v3_normalize_fast(vec3_t const v) {
	
	float const fNorm = v3_norm(v);
	
	if (fNorm > 0.0f) {
#ifdef MATH_3D_NORMALIZE_USE_RSQRTF
		float const fInvLength = __rsqrtf( fNorm );
#else
		float const fInvLength = 1.0f / __sqrtf( fNorm );
#endif		
		return( vec3_t( v.x * fInvLength, v.y * fInvLength, v.z * fInvLength ) );
	}
	else
		return( vec3_t() );
}
STATIC_INLINE_PURE vec3_t const v3_normalize(vec3_t const v) {
	
	float const fNorm = v3_norm(v);
	
	if (fNorm > 0.0f) {

		float const fInvLength = 1.0f / __sqrtf( fNorm );
		
		return( vec3_t( v.x * fInvLength, v.y * fInvLength, v.z * fInvLength ) );
	}
	else
		return( vec3_t() );
}
__attribute__((__nonnull__(2))) STATIC_INLINE_PURE vec3_t const v3_normalize(vec3_t const v, float* const __restrict pLength) {
	
	float const fNorm = v3_norm(v);
	
	if (fNorm > 0.0f) {
		float fInvLength;
	
		fInvLength = __sqrtf( fNorm );
		// Save/output length if param specified
		*pLength = fInvLength;
		
		fInvLength = 1.0f / fInvLength;
		
		return( vec3_t( v.x * fInvLength, v.y * fInvLength, v.z * fInvLength ) );
	}
	else
		return( vec3_t() );
}

STATIC_INLINE_PURE vec3_t const v3_lerp(vec3_t const a, vec3_t const b, float const t)
{
	// (a*(1 - t) + b * t)
	// fma(t, b, fma(-t, a, a))
	return(v3_fmas(t, b, v3_fmas(-t, a, a)));
}
STATIC_INLINE_PURE vec3_t const v3_proj(vec3_t const v, vec3_t const onto) {
	return( v3_muls(onto, v3_dot(v, onto) / v3_dot(onto, onto)) );
}

STATIC_INLINE_PURE vec3_t const v3_cross(vec3_t const a, vec3_t const b) {

	return( vec3_t(
		//a.y * b.z - a.z * b.y,
		//a.z * b.x - a.x * b.z,
		//a.x * b.y - a.y * b.x
		__fms(a.y,b.z, (a.z * b.y)),
		__fms(a.z,b.x, (a.x * b.z)),
		__fms(a.x,b.y, (a.y * b.x))
	) );
}

STATIC_INLINE_PURE float const v3_angle_between(vec3_t const a, vec3_t const b) {
	return( acosf( v3_dot(a, b) / (v3_length(a) * v3_length(b)) ) );
}


//
// Matrix functions header implementation
//

STATIC_INLINE_PURE mat4_t const m4_identity() {
	return( mat4_t(
		 1.0f,  0.0f,  0.0f,  0.0f,
		 0.0f,  1.0f,  0.0f,  0.0f,
		 0.0f,  0.0f,  1.0f,  0.0f,
		 0.0f,  0.0f,  0.0f,  1.0f
	) );
}

STATIC_INLINE_PURE mat4_t const m4_translation(vec3_t const offset) {
	return( mat4_t(
		 1.0f,  0.0f,  0.0f,  offset.x,
		 0.0f,  1.0f,  0.0f,  offset.y,
		 0.0f,  0.0f,  1.0f,  offset.z,
		 0.0f,  0.0f,  0.0f,  1.0f
	) );
}

STATIC_INLINE_PURE mat4_t const m4_scaling(vec3_t const scale) {
	return( mat4_t(
		 scale.x,  0.0f,  0.0f,  	 0.0f,
		 0.0f,  scale.y,  0.0f,  	 0.0f,
		 0.0f,  0.0f,  	 scale.z,  0.0f,
		 0.0f,  0.0f, 			 0.0f, 1.0f
	) );
}

STATIC_INLINE_PURE mat4_t const m4_rotation_x(float const angle_in_deg) {
	float s, c;
	s = ARM__sinf(angle_in_deg);
	c = ARM__cosf(angle_in_deg);
	return( mat4_t(
		1.0f,  0.0f,  0.0f,  0.0f,
		0.0f,  c, 		-s,  	 0.0f,
		0.0f,  s,  		c,  	 0.0f,
		0.0f,  0.0f,  0.0f,  1.0f
	) );
}

STATIC_INLINE_PURE mat4_t const m4_rotation_y(float const angle_in_deg) {
	float s, c;
	s = ARM__sinf(angle_in_deg);
	c = ARM__cosf(angle_in_deg);
	return( mat4_t(
		 c,  		0.0f,  s,  		0.0f,
		 0.0f,  1.0f,  0.0f,  0.0f,
		-s,  		0.0f,  c,  		0.0f,
		 0.0f,  0.0f,  0.0f,  1.0f
	) );
}

STATIC_INLINE_PURE mat4_t const m4_rotation_z(float const angle_in_deg) {
	float s, c;
	s = ARM__sinf(angle_in_deg);
	c = ARM__cosf(angle_in_deg);
	return( mat4_t(
		 c, 		-s,  		0.0f,  0.0f,
		 s,  		 c,  		0.0f,  0.0f,
		 0.0f,   0.0f,  1.0f,  0.0f,
		 0.0f,   0.0f,  0.0f,  1.0f
	) );
}

STATIC_INLINE_PURE mat4_t const m4_transpose(mat4_t const matrix) {
	return( mat4_t(
		matrix.m00, matrix.m01, matrix.m02, matrix.m03,
		matrix.m10, matrix.m11, matrix.m12, matrix.m13,
		matrix.m20, matrix.m21, matrix.m22, matrix.m23,
		matrix.m30, matrix.m31, matrix.m32, matrix.m33
	) );
}

/**
 * Multiplication of two 4x4 matrices.
 * 
 * Implemented by following the row times column rule and illustrating it on a
 * whiteboard with the proper indices in mind.
 * 
 * Further reading: https://en.wikipedia.org/wiki/Matrix_multiplication
 * But note that the article use the first index for rows and the second for
 * columns.
 */
STATIC_INLINE_PURE mat4_t const m4_mul(mat4_t const a, mat4_t const b) {
	mat4_t result;
	
	for(int i = 3; i >= 0; --i) {
		for(int j = 3; j >= 0; --j) {

			//for(int k = 0; k < 4; k++) {
			//	sum += a.m[k][j] * b.m[i][k];
			//}
			// aka:
			//a.m[0][j] * b.m[i][0] + a.m[1][j] * b.m[i][1] + a.m[2][j] * b.m[i][2] + a.m[3][j] * b.m[i][3];
			//
			// unrolled and fma'd:
			result.m[i][j] = __fma(a.m[0][j], b.m[i][0], __fma( a.m[1][j], b.m[i][1], __fma(a.m[2][j], b.m[i][2], (a.m[3][j] * b.m[i][3]))));
		}
	}
	
	return(result);
}

STATIC_INLINE_PURE quat_t const quat(float const theta_x, float const theta_y, float const theta_z) {
	vec3_t vCos, vSin;
	vCos.x = ARM__cosf(0.5f*theta_x);
	vCos.y = ARM__cosf(0.5f*theta_y);
	vCos.z = ARM__cosf(0.5f*theta_z);

	vSin.x = ARM__sinf(0.5f*theta_x);
	vSin.y = ARM__sinf(0.5f*theta_y);
	vSin.z = ARM__sinf(0.5f*theta_z);

	// and now compute quaternion
	quat_t q;

	q.s = __fma(vCos.z, vCos.y*vCos.x, vSin.z * vSin.y*vSin.x);

	q.v.x = __fms(vCos.z, vCos.y*vSin.x, vSin.z * vSin.y*vCos.x);
	q.v.y = __fma(vCos.z, vSin.y*vCos.x, vSin.z * vCos.y*vSin.x);
	q.v.z = __fms(vSin.z, vCos.y*vCos.x, vCos.z * vSin.y*vSin.x);

	return(q);
}

STATIC_INLINE_PURE float const quat_length(quat_t const q) {
	float length(0.0f);

	float const fNorm = quat_norm(q);

	if (fNorm > 0.0f) {
		length = __sqrtf(fNorm);
	}

	return(length);
}
STATIC_INLINE_PURE quat_t const quat_normalize(quat_t const q) {
	float const fNorm = quat_norm(q);

	if (fNorm > 0.0f) {
		float fInvLength;

		fInvLength = __sqrtf(fNorm);
		fInvLength = 1.0f / fInvLength;

		return(quat_t( vec3_t(q.v.x * fInvLength, q.v.y * fInvLength, q.v.z * fInvLength), q.s ));
	}
	else
		return(quat_t( vec3_t(), q.s ));
}

STATIC_INLINE_PURE quat_t const quat_fmas(float const s, quat_t const a, quat_t const b) {
	return(quat_t( v3_fmas(s, a.v, b.v), __fma(s, a.s, b.s) ));
}

STATIC_INLINE_PURE quat_t const quat_conjugate(quat_t const q) {
	return(quat_t( v3_negate(q.v), q.s ));
}

STATIC_INLINE_PURE float const quat_dot(quat_t const q1, quat_t const q2) {
	return(__fma(q1.s, q2.s, v3_dot(q1.v, q2.v)));
}

STATIC_INLINE_PURE quat_t const quat_lerp(quat_t const a, quat_t const b, float const t) {
	// (a*(1 - t) + b * t)
	// fma(t, b, fma(-t, a, a))
	return( quat_normalize(quat_fmas(t, b, quat_fmas(-t, a, a))) );
}

STATIC_INLINE_PURE mat4_t const quat_to_matrix(quat_t q) {
	q = quat_normalize(q);

	return(mat4_t(1.0f - 2.0f * (__fma(q.v.y, q.v.y, q.v.z*q.v.z)), 2.0f * (__fma(q.v.x, q.v.y, q.s * q.v.z)), 2.0f * (__fms(q.v.x, q.v.z, q.s * q.v.y)), 0.0f,
		2.0f * (__fms(q.v.x, q.v.y, q.s * q.v.z)), 1.0f - 2.0f * (__fma(q.v.x, q.v.x, q.v.z*q.v.z)), 2.0f * (__fma(q.v.y, q.v.z, q.s * q.v.x)), 0.0f,
		2.0f * (__fma(q.v.x, q.v.z, q.s * q.v.y)), 2.0f * (__fms(q.v.y, q.v.z, q.s * q.v.x)), 1.0f - 2.0f * (__fma(q.v.x, q.v.x, q.v.y*q.v.y)), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f));
}
#endif // MATH_3D_HEADER



#ifdef MATH_3D_IMPLEMENTATION

/**
 * Creates a matrix to rotate around an axis by a given angle. The axis doesn't
 * need to be normalized.
 * 
 * Sources:
 * 
 * https://en.wikipedia.org/wiki/Rotation_matrix#Rotation_matrix_from_axis_and_angle
 */
mat4_t const m4_rotation(float const angle_in_deg, vec3_t const axis) {
	vec3_t const normalized_axis = v3_normalize(axis);
	float const x = normalized_axis.x, y = normalized_axis.y, z = normalized_axis.z;
	
	float s, c;
	s = ARM__sinf(angle_in_deg);
	c = ARM__cosf(angle_in_deg);
	
	float const n1minusc = 1.0f - c;
	
	return( mat4_t(
		/*
				c + x*x*(n1minusc),        x*y*(n1minusc) - z*s,      x*z*(n1minusc) + y*s,  0.0f,
		    y*x*(n1minusc) + z*s,  		 c + y*y*(n1minusc),        y*z*(n1minusc) - x*s,  0.0f,
		    z*x*(n1minusc) - y*s,      z*y*(n1minusc) + x*s,  		c + z*z*(n1minusc),    0.0f,
		    0.0f,                 		 0.0f,                 			0.0f,            			 1.0f
		*/		
				__fma(x*x,(n1minusc), c), 		__fms(x*y,(n1minusc), z*s),		__fma(x*z,(n1minusc), y*s),  	0.0f,
		    __fma(y*x,(n1minusc), z*s),  	__fma(y*y,(n1minusc), c),    	__fms(y*z,(n1minusc), x*s),  	0.0f,
		    __fms(z*x,(n1minusc), y*s),   __fma(z*y,(n1minusc), x*s),  	__fma(z*z,(n1minusc), c),    	0.0f,
		    0.0f,                 		 		0.0f,                 				0.0f,            			 				1.0f
	) );
}


/**
 * Creates an orthographic projection matrix. It maps the right handed cube
 * defined by left, right, bottom, top, back and front onto the screen and
 * z-buffer. You can think of it as a cube you move through world or camera
 * space and everything inside is visible.
 * 
 * This is slightly different from the traditional glOrtho() and from the linked
 * sources. These functions require the user to negate the last two arguments
 * (creating a left-handed coordinate system). We avoid that here so you can
 * think of this function as moving a right-handed cube through world space.
 * 
 * The arguments are ordered in a way that for each axis you specify the minimum
 * followed by the maximum. Thats why it's bottom to top and back to front.
 * 
 * Implementation details:
 * 
 * To be more exact the right-handed cube is mapped into normalized device
 * coordinates, a left-handed cube where (-1 -1) is the lower left corner,
 * (1, 1) the upper right corner and a z-value of -1 is the nearest point and
 * 1 the furthest point. OpenGL takes it from there and puts it on the screen
 * and into the z-buffer.
 * 
 * Sources:
 * 
 * https://msdn.microsoft.com/en-us/library/windows/desktop/dd373965(v=vs.85).aspx
 * https://unspecified.wordpress.com/2012/06/21/calculating-the-gluperspective-matrix-and-other-opengl-matrix-maths/
 */
mat4_t const m4_ortho(float const left, float const right, float const bottom, float const top, float const back, float const front) {
	float const l = left, r = right, b = bottom, t = top, n = front, f = back;
	float const tx = -(r + l) / (r - l);
	float const ty = -(t + b) / (t - b);
	float const tz = -(f + n) / (f - n);
	return( mat4_t(
		 2.0f / (r - l),  0.0f,            0.0f,            tx,
		 0.0f,            2.0f / (t - b),  0.0f,            ty,
		 0.0f,            0.0f,            2.0f / (f - n),  tz,
		 0.0f,            0.0f,            0.0f,            1.0f
	) );
}

/**
 * Creates a perspective projection matrix for a camera.
 * 
 * The camera is at the origin and looks in the direction of the negative Z axis.
 * `near_view_distance` and `far_view_distance` have to be positive and > 0.
 * They are distances from the camera eye, not values on an axis.
 * 
 * `near_view_distance` can be small but not 0. 0 breaks the projection and
 * everything ends up at the max value (far end) of the z-buffer. Making the
 * z-buffer useless.
 * 
 * The matrix is the same as `gluPerspective()` builds. The view distance is
 * mapped to the z-buffer with a reciprocal function (1/x). Therefore the z-buffer
 * resolution for near objects is very good while resolution for far objects is
 * limited.
 * 
 * Sources:
 * 
 * https://unspecified.wordpress.com/2012/06/21/calculating-the-gluperspective-matrix-and-other-opengl-matrix-maths/
 */
mat4_t const m4_perspective(float const vertical_field_of_view_in_deg, float const aspect_ratio, float const near_view_distance, float const far_view_distance) {
	
	float const fovy_in_rad = TO_RADIANS(vertical_field_of_view_in_deg);
	float const f = 1.0f / tanf(fovy_in_rad * 0.5f);
	float const ar = aspect_ratio;
	float const nd = near_view_distance, fd = far_view_distance;
	
	return( mat4_t(
		 f / ar,           0.0f,                0.0f,                0.0f,
		 0.0f,                f,                0.0f,                0.0f,
		 0.0f,             0.0f,               (fd+nd)/(nd-fd),  		 (2.0f*fd*nd)/(nd-fd),
		 0.0f,             0.0f,                -1.0f,               0.0f
	) );
}

/**
 * Builds a transformation matrix for a camera that looks from `from` towards
 * `to`. `up` defines the direction that's upwards for the camera. All three
 * vectors are given in world space and `up` doesn't need to be normalized.
 * 
 * Sources: Derived on whiteboard.
 * 
 * Implementation details:
 * 
 * x, y and z are the right-handed base vectors of the cameras subspace.
 * x has to be normalized because the cross product only produces a normalized
 *   output vector if both input vectors are orthogonal to each other. And up
 *   probably isn't orthogonal to z.
 * 
 * These vectors are then used to build a 3x3 rotation matrix. This matrix
 * rotates a vector by the same amount the camera is rotated. But instead we
 * need to rotate all incoming vertices backwards by that amount. That's what a
 * camera matrix is for: To move the world so that the camera is in the origin.
 * So we take the inverse of that rotation matrix and in case of an rotation
 * matrix this is just the transposed matrix. That's why the 3x3 part of the
 * matrix are the x, y and z vectors but written horizontally instead of
 * vertically.
 * 
 * The translation is derived by creating a translation matrix to move the world
 * into the origin (thats translate by minus `from`). The complete lookat matrix
 * is then this translation followed by the rotation. Written as matrix
 * multiplication:
 * 
 *   lookat = rotation * translation
 * 
 * Since we're right-handed this equals to first doing the translation and after
 * that doing the rotation. During that multiplication the rotation 3x3 part
 * doesn't change but the translation vector is multiplied with each rotation
 * axis. The dot product is just a more compact way to write the actual
 * multiplications.
 */
mat4_t const m4_look_at(vec3_t const from, vec3_t const to, vec3_t const up) {
	vec3_t const z = v3_muls(v3_normalize(v3_sub(to, from)), -1.0f);
	vec3_t const x = v3_normalize(v3_cross(up, z));
	vec3_t const y = v3_cross(z, x);
	
	return( mat4_t(
		x.x, x.y, x.z, -v3_dot(from, x),
		y.x, y.y, y.z, -v3_dot(from, y),
		z.x, z.y, z.z, -v3_dot(from, z),
		0.0f,   0.0f,   0.0f,    1.0f
	) );
}


/**
 * Inverts an affine transformation matrix. That are translation, scaling,
 * mirroring, reflection, rotation and shearing matrices or any combination of
 * them.
 * 
 * Implementation details:
 * 
 * - Invert the 3x3 part of the 4x4 matrix to handle rotation, scaling, etc.
 *   correctly (see source).
 * - Invert the translation part of the 4x4 matrix by multiplying it with the
 *   inverted rotation matrix and negating it.
 * 
 * When a 3D point is multiplied with a transformation matrix it is first
 * rotated and then translated. The inverted transformation matrix is the
 * inverse translation followed by the inverse rotation. Written as a matrix
 * multiplication (remember, the effect applies right to left):
 * 
 *   inv(matrix) = inv(rotation) * inv(translation)
 * 
 * The inverse translation is a translation into the opposite direction, just
 * the negative translation. The rotation part isn't changed by that
 * multiplication but the translation part is multiplied by the inverse rotation
 * matrix. It's the same situation as with `m4_look_at()`. But since we don't
 * store the rotation matrix as 3D vectors we can't use the dot product and have
 * to write the matrix multiplication operations by hand.
 * 
 * Sources for 3x3 matrix inversion:
 * 
 * https://www.khanacademy.org/math/precalculus/precalc-matrices/determinants-and-inverses-of-large-matrices/v/inverting-3x3-part-2-determinant-and-adjugate-of-a-matrix
 */
mat4_t const m4_invert_affine(mat4_t const matrix) {
	// Create shorthands to access matrix members
	float const m00 = matrix.m00,  m10 = matrix.m10,  m20 = matrix.m20,  m30 = matrix.m30;
	float const m01 = matrix.m01,  m11 = matrix.m11,  m21 = matrix.m21,  m31 = matrix.m31;
	float const m02 = matrix.m02,  m12 = matrix.m12,  m22 = matrix.m22,  m32 = matrix.m32;
	
	// Invert 3x3 part of the 4x4 matrix that contains the rotation, etc.
	// That part is called R from here on.
		
		// Calculate cofactor matrix of R
		float const c00 =   m11*m22 - m12*m21,   c10 = -(m01*m22 - m02*m21),  c20 =   m01*m12 - m02*m11;
		float const c01 = -(m10*m22 - m12*m20),  c11 =   m00*m22 - m02*m20,   c21 = -(m00*m12 - m02*m10);
		float const c02 =   m10*m21 - m11*m20,   c12 = -(m00*m21 - m01*m20),  c22 =   m00*m11 - m01*m10;
		
		// Caclculate the determinant by using the already calculated determinants
		// in the cofactor matrix.
		// Second sign is already minus from the cofactor matrix.
		float const det = m00*c00 + m10*c10 + m20 * c20;
		if (__fabsf(det) < 0.00001f)
			return( m4_identity() );
		
		// Calcuate inverse of R by dividing the transposed cofactor matrix by the
		// determinant.
		float const fInvDet = 1.0f / det;
		float const i00 = c00 * fInvDet,  i10 = c01 * fInvDet,  i20 = c02 * fInvDet;
		float const i01 = c10 * fInvDet,  i11 = c11 * fInvDet,  i21 = c12 * fInvDet;
		float const i02 = c20 * fInvDet,  i12 = c21 * fInvDet,  i22 = c22 * fInvDet;
	
	// Combine the inverted R with the inverted translation
	return( mat4_t(
		i00, i10, i20,  -(i00*m30 + i10*m31 + i20*m32),
		i01, i11, i21,  -(i01*m30 + i11*m31 + i21*m32),
		i02, i12, i22,  -(i02*m30 + i12*m31 + i22*m32),
		0.0f,   0.0f,   0.0f,      1.0f
	) );
}

/**
 * Multiplies a 4x4 matrix with a 3D vector representing a point in 3D space.
 * 
 * Before the matrix multiplication the vector is first expanded to a 4D vector
 * (x, y, z, 1). After the multiplication the vector is reduced to 3D again by
 * dividing through the 4th component (if it's not 0 or 1).
 */
vec3_t const m4_mul_pos(mat4_t const matrix, vec3_t const position) {
	vec3_t const result(
		//matrix.m00 * position.x + matrix.m10 * position.y + matrix.m20 * position.z + matrix.m30,
		//matrix.m01 * position.x + matrix.m11 * position.y + matrix.m21 * position.z + matrix.m31,
		//matrix.m02 * position.x + matrix.m12 * position.y + matrix.m22 * position.z + matrix.m32
		__fma(matrix.m00, position.x, __fma(matrix.m10,position.y, __fma(matrix.m20,position.z, matrix.m30))),
		__fma(matrix.m01, position.x, __fma(matrix.m11,position.y, __fma(matrix.m21,position.z, matrix.m31))),
		__fma(matrix.m02, position.x, __fma(matrix.m12,position.y, __fma(matrix.m22,position.z, matrix.m32)))
	);
	
	//float const w = matrix.m03 * position.x + matrix.m13 * position.y + matrix.m23 * position.z + matrix.m33;
	float const w = __fma(matrix.m03,position.x, __fma(matrix.m13,position.y, __fma(matrix.m23,position.z, matrix.m33)));
	if ((w != 0.0f && w != 1.0f)) {
		float const fInvW = 1.0f / w;
		return( vec3_t(result.x * fInvW, result.y * fInvW, result.z * fInvW) ); // does correct perspective divide here
	}
	
	return(result);
}

/**
 * Multiplies a 4x4 matrix with a 3D vector representing a direction in 3D space.
 * 
 * Before the matrix multiplication the vector is first expanded to a 4D vector
 * (x, y, z, 0). For directions the 4th component is set to 0 because directions
 * are only rotated, not translated. After the multiplication the vector is
 * reduced to 3D again by dividing through the 4th component (if it's not 0 or
 * 1). This is necessary because the matrix might contains something other than
 * (0, 0, 0, 1) in the bottom row which might set w to something other than 0
 * or 1.
 */
vec3_t const m4_mul_dir(mat4_t const matrix, vec3_t const direction) {
	vec3_t const result = vec3_t(
		//matrix.m00 * direction.x + matrix.m10 * direction.y + matrix.m20 * direction.z,
		//matrix.m01 * direction.x + matrix.m11 * direction.y + matrix.m21 * direction.z,
		//matrix.m02 * direction.x + matrix.m12 * direction.y + matrix.m22 * direction.z
		__fma(matrix.m00,direction.x, __fma(matrix.m10,direction.y, (matrix.m20 * direction.z))),
		__fma(matrix.m01,direction.x, __fma(matrix.m11,direction.y, (matrix.m21 * direction.z))),
		__fma(matrix.m02,direction.x, __fma(matrix.m12,direction.y, (matrix.m22 * direction.z)))
	);
	
	//float const w = matrix.m03 * direction.x + matrix.m13 * direction.y + matrix.m23 * direction.z;
	//float const w = __fma(matrix.m03,direction.x, __fma(matrix.m13,direction.y, (matrix.m23 * direction.z)));
	//if ((w != 0.0f && w != 1.0f)) {
	//	float const fInvW = 1.0f / w;
	//	return( vec3(result.x * fInvW, result.y * fInvW, result.z * fInvW) ); // does correct perspective divide here
	//}
	
	return(result);
}

#endif // MATH_3D_IMPLEMENTATION
