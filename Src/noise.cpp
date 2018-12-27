/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

Perlin Noise, Simplex Noise, Value Noise, and Spiral Noise are all inherently Copyright of their respectful creators.
 */

#include "globals.h"
#include "noise.h"



static uint8_t PerlinPermutationsDefaultNativeData[Noise::PERMUTATION_STORAGE_SIZE]			// 512 bytes
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".dtcm")));

static uint8_t const* __restrict PerlinPermutations															// pointerto current dataset of permutations
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".dtcm")))( PerlinPermutationsDefaultNativeData );

#define noise_lerp(t, a, b) lerp(a,b,t) // reorders parameters correctly

STATIC_INLINE_PURE float const grad( int const hash, float const x, float const y ) {
  int const h(hash & 7);      
	// Convert low 3 bits of hash code
  float const u = h < 4 ? x : y,  // into 8 simple gradient directions,
						  v = h < 4 ? y : x;  // and compute the dot product with (x,y).
	return( (0 == (h&1) ? u : -u) + (0 == (h&2) ? 2.0f*v : -2.0f*v) );
}

STATIC_INLINE_PURE float const grad(int const hash, float const x, float const y, float const z) {
	int const h(hash & 15);
	// Convert lower 4 bits of hash into 12 gradient directions
	float const u = h < 8 ? x : y,
		          v = h < 4 ? y : 12== h || 14 == h ? x : z;
	return( (0 == (h&1) ? u : -u) + (0 == (h&2) ? v : -v) );
}
namespace Noise
{		
	void SetPermutationDataStorage(uint8_t* const __restrict& __restrict PermutationDataStorage)
	{
		PerlinPermutations = PermutationDataStorage;
	}
	void SetDefaultNativeDataStorage()
	{
		PerlinPermutations = PerlinPermutationsDefaultNativeData;
	}
	void NewNoisePermutation(uint8_t* const __restrict& __restrict PermutationDataStorage)
	{
		for ( int32_t iDx = NUM_PERMUTATIONS - 1 ; iDx >= 0 ; --iDx ) {
			PermutationDataStorage[iDx] = getRandomBlueNoiseSample();
		}
	
		SetPermutationDataStorage(PermutationDataStorage);
		
		// duplicate initial 256 PerlinPermutationsutations
		memcpy8(const_cast<uint8_t* __restrict>(PerlinPermutations + (NUM_PERMUTATIONS - 1)), PerlinPermutations, NUM_PERMUTATIONS);
	}
	
	void NewNoisePermutation()
	{
		NewNoisePermutation(PerlinPermutationsDefaultNativeData);
	}
	
	// Value noise is the simplest and fastest, I find it poroduces better results in some cases too than both
	// perline and simplex. Depends on the application
	__ramfunc float const getValueNoise( float x, float y, interpolator::functor const& interpFunctor )
	{
		int AA, AB, BA, BB;
		
		{
			// Find the unit cube that contains the point
			int const X = int32::__floorf(x) & 255;
			int const Y = int32::__floorf(y) & 255;

			// Hash coordinates of the 8 cube corners
			int const A = PerlinPermutations[X] + Y;		
			AA = PerlinPermutations[A];		// X0, Y0
			AB = PerlinPermutations[A + 1];	// X0, Y1
			int const B = PerlinPermutations[(X) + 1] + (Y);
			BA = PerlinPermutations[B];		// X1, Y0
			BB = PerlinPermutations[B + 1];	// X1, Y1
		}
		// Add blended results from 8 corners of cube

		// Compute fade/smoothstep/any function curves for each of x, y
		// Find relative x, y of point in cube
		x -= float32::__floorf(x);
		y -= float32::__floorf(y);
		float const u = interpFunctor(x);
		float const v = interpFunctor(y);
		
		// lerp y (v) axis
		float const res = noise_lerp(v,	 // lerp x (u) axis
									 noise_lerp(u, PerlinPermutations[AA], PerlinPermutations[BA]),
									 noise_lerp(u, PerlinPermutations[AB], PerlinPermutations[BB])
									 );

		return(res * Constants::inverseUINT8); // [0...1] range
	}
	
	// Perlin Simplex Noise supersedes Perlin "Classic" noise. Higher quality at equal or better performance
	// however they are very different from eachother, perlin noise for example is "smoother" over a greyscale gradient
	// not defined as ramfunc, if realtime noise generation is required, use simplex noise instead
	NOINLINE float const getPerlinNoise( float x, float y, float z, interpolator::functor const& interpFunctor)
	{
		int AA, AB, BA, BB;
		
		{
			// Find the unit cube that contains the point
			int const X = int32::__floorf(x) & 255;
			int const Y = int32::__floorf(y) & 255;
			int const Z = int32::__floorf(z) & 255;

			// Hash coordinates of the 8 cube corners
			int const A = PerlinPermutations[X] + Y;
			AA = PerlinPermutations[A] + Z;
			AB = PerlinPermutations[A + 1] + Z;
			int const B = PerlinPermutations[X + 1] + Y;
			BA = PerlinPermutations[B] + Z;
			BB = PerlinPermutations[B + 1] + Z;
		}

		// Find relative x, y,z of point in cube
		x -= float32::__floorf(x);
		y -= float32::__floorf(y);
		z -= float32::__floorf(z);

		// Compute fade/smoothstep/any function curves for each of x, y
		float const u = interpFunctor(x);
		float const v = interpFunctor(y);
		float const w = interpFunctor(z);
		
		// Add blended results from 8 corners of cube
		float const res = noise_lerp(w, noise_lerp(v, noise_lerp(u, grad(PerlinPermutations[AA], x, y, z), grad(PerlinPermutations[BA], x-1.0f, y, z)), 
								 noise_lerp(u, grad(PerlinPermutations[AB], x, y-1.0f, z), grad(PerlinPermutations[BB], x-1.0f, y-1.0f, z))),	
								 noise_lerp(v, noise_lerp(u, grad(PerlinPermutations[AA+1], x, y, z-1.0f), grad(PerlinPermutations[BA+1], x-1.0f, y, z-1.0f)), 
								 noise_lerp(u, grad(PerlinPermutations[AB+1], x, y-1.0f, z-1.0f),	grad(PerlinPermutations[BB+1], x-1.0f, y-1.0f, z-1.0f))));
		
		return( (res + 1.0f) * 0.5f );  // [0...1] range
	}

	
	NOINLINE float const getSimplexNoise2D( float x, float y )
	{
		static constexpr float const F2 = 0.366025403f, // F2 = 0.5*(sqrt(3.0)-1.0)
																 G2 = 0.211324865f; // G2 = (3.0-Math.sqrt(3.0))/6.0
			
		int ii, jj;
		{			
			// Skew the input space to determine which simplex cell we're in
			{
			float const s = (x+y)*F2; // Hairy factor for 2D

			ii = int32::__floorf(x + s);
			jj = int32::__floorf(y + s);
			}

			float const t = (float const)(ii+jj)*G2;

			x = x-((float)ii-t); // The x,y distances from the cell origin
			y = y-((float)jj-t);
			
			// Wrap the integer indices at 256, to avoid indexing PerlinPermutations[] out of bounds
			ii &= 0xFF;
			jj &= 0xFF;
		}

    // For the 2D case, the simplex shape is an equilateral triangle.
    // Determine which simplex we are in.
    int i1, j1; // Offsets for second (middle) corner of simplex in (i,j) coords
    if(x>y) {i1=1; j1=0;} // lower triangle, XY order: (0,0)->(1,0)->(1,1)
    else {i1=0; j1=1;}      // upper triangle, YX order: (0,0)->(0,1)->(1,1)

    // A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
    // a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
    // c = (3-sqrt(3))/6

		float n0(0.0f); // Noise contributions from the three corners
    // Calculate the contribution from the three corners
		{
			float const t0 = 0.5f - x*x-y*y;
			if(t0 >= 0.0f) {
				n0 = t0 * t0 * t0 * t0 * grad(PerlinPermutations[ii+PerlinPermutations[jj]], x, y); 
			}
		}

		float n1(0.0f); // Noise contributions from the three corners
		{
			float const x1 = x - i1 + G2; // Offsets for middle corner in (x,y) unskewed coords
			float const y1 = y - j1 + G2;
			float const t1 = 0.5f - x1*x1-y1*y1;
			if(t1 >= 0.0f) {
				n1 = t1 * t1 * t1 * t1 * grad(PerlinPermutations[ii+i1+PerlinPermutations[jj+j1]], x1, y1);
			}
		}

		float n2(0.0f); // Noise contributions from the three corners
		{
			float const x2 = x - 1.0f + 2.0f * G2; // Offsets for last corner in (x,y) unskewed coords
			float const y2 = y - 1.0f + 2.0f * G2;
			float const t2 = 0.5f - x2*x2-y2*y2;
			if(t2 >= 0.0f) {
				n2 = t2 * t2 * t2 * t2 * grad(PerlinPermutations[ii+1+PerlinPermutations[jj+1]], x2, y2);
			}
		}

    // Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the interval [-1,1].
    return( ((40.0f * (n0 + n1 + n2)) + 1.0f) * 0.5f); // [0...1] range
	}
	
	NOINLINE float const getSimplexNoise3D( float x, float y, float z )
	{
		static constexpr float const F3 = 1.0f/3.0f, // Simple skewing for 3D noise
																 G3 = 1.0f/6.0f; 
		
		int ii, jj, kk;
		{			
			// Skew the input space to determine which simplex cell we're in
			{
			float const s = (x+y+z)*F3; // Hairy factor for 2D

			ii = int32::__floorf(x + s);
			jj = int32::__floorf(y + s);
			kk = int32::__floorf(z + s);
			}

			float const t = (float const)(ii+jj+kk)*G3;

			x = x-((float)ii-t); // The x,y distances from the cell origin
			y = y-((float)jj-t);
			z = z-((float)kk-t);
			
			// Wrap the integer indices at 256, to avoid indexing PerlinPermutations[] out of bounds
			ii &= 0xFF;
			jj &= 0xFF;
			kk &= 0xFF;
			
		}

    // For the 3D case, the simplex shape is a slightly irregular tetrahedron.
    // Determine which simplex we are in.
    int i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords
    int i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords

    if(x>=y) {
      if(y>=z)
        { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; } // X Y Z order
        else if(x>=z) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; } // X Z Y order
        else { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; } // Z X Y order
      }
    else { // x0<y0
      if(y<z) { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; } // Z Y X order
      else if(x<z) { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; } // Y Z X order
      else { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; } // Y X Z order
    }

    // A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
    // a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
    // a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
    // c = 1/6.
		
		float n0(0.0f); // Noise contributions from the three corners
    // Calculate the contribution from the three corners
		{
			float const t0 = 0.6f - x*x-y*y-z*z;
			if(t0 >= 0.0f) {
				n0 = t0 * t0 * t0 * t0 * grad(PerlinPermutations[ii+PerlinPermutations[jj+PerlinPermutations[kk]]], x, y, z); 
			}
		}

		float n1(0.0f); // Noise contributions from the three corners
		{
			float const x1 = x - i1 + G3; // Offsets for second corner in (x,y,z) coords
			float const y1 = y - j1 + G3;
			float const z1 = z - k1 + G3;
			float const t1 = 0.6f - x1*x1-y1*y1-z1*z1;
			if(t1 >= 0.0f) {
				n1 = t1 * t1 * t1 * t1 * grad(PerlinPermutations[ii+i1+PerlinPermutations[jj+j1+PerlinPermutations[kk+k1]]], x1, y1, z1);
			}
		}

		float n2(0.0f); // Noise contributions from the three corners
		{
			float const x2 = x - i2 + 2.0f*G3; // Offsets for third corner in (x,y,z) coords
			float const y2 = y - j2 + 2.0f*G3;
			float const z2 = z - k2 + 2.0f*G3;
			float const t2 = 0.6f - x2*x2-y2*y2-z2*z2;
			if(t2 >= 0.0f) {
				n2 = t2 * t2 * t2 * t2 * grad(PerlinPermutations[ii+i2+PerlinPermutations[jj+j2+PerlinPermutations[kk+k2]]], x2, y2, z2);
			}
		}

		float n3(0.0f); // Noise contributions from the three corners
		{
			float const x3 = x - 1.0f + 3.0f*G3; // Offsets for last corner in (x,y,z) coords
			float const y3 = y - 1.0f + 3.0f*G3;
			float const z3 = z - 1.0f + 3.0f*G3;
			float const t3 = 0.6f - x3*x3-y3*y3-z3*z3;
			if(t3 >= 0.0f) {
				n3 = t3 * t3 * t3 * t3 * grad(PerlinPermutations[ii+1+PerlinPermutations[jj+1+PerlinPermutations[kk+1]]], x3, y3, z3);
			}
		}
		
    // Add contributions from each corner to get the final noise value.
    // The result is scaled to stay just inside [-1,1]
    return( ((32.0f * (n0 + n1 + n2 + n3)) + 1.0f) * 0.5f ); // [0...1] range
	}
	
} // end namespace




