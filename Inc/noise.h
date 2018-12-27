/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef NOISE_H
#define NOISE_H
#include "math_3d.h"
#include "rng.h"
#include "oled.h"
#include "commonmath.h"

#include "FRAM\FRAM_SDF_MemoryLayout.h"

static constexpr uint32_t const NOISE_TEXTURE_SATBITS = Constants::SATBIT_256,
																NOISE_TEXTURE_DIMENSION = (1 << NOISE_TEXTURE_SATBITS),
																NOISE_TEXTURE_NBBYTES = NOISE_TEXTURE_DIMENSION*NOISE_TEXTURE_DIMENSION;

namespace Noise
{
	static constexpr uint32_t const NUM_PERMUTATIONS = 256,
																	PERMUTATION_STORAGE_SIZE = NUM_PERMUTATIONS << 1;
	
	// Noise interpolators
	namespace interpolator
	{
		// no solution to avoid virtual call overhead... need to profile real-time impact
		typedef struct __attribute__((const)) sFunctor
		{
			float const kCoefficient;		// optional "constant" can be used for interpolators that require it

			sFunctor() : kCoefficient(0.0f) {}
			sFunctor(float const kCo) : kCoefficient(kCo) {}
		
			__attribute__((const)) inline virtual float const operator()(float const t) const = 0;
		} functor;
		
		typedef struct __attribute__((const)) sSmoothStep : functor		// smmother value or perlin noise
		{
				__attribute__((const)) inline virtual float const operator()(float const t) const
				{
						return( t * t * (3.0f - 2.0f * t) );
				}
		} SmoothStep;
		
		typedef struct __attribute__((const)) sFade : functor					// sharper edjes of value/perlin noise
		{
				__attribute__((const)) inline virtual float const operator()(float const t) const
				{
						return( t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f) );
				}
		} Fade;
		
		typedef struct sImpulse : functor					// Configurable one sided fast ramp up, slow ramp down
		{																					// eg.) value noise smoothing area and mask
			__attribute__((const)) inline virtual float const operator()(float const t) const
			{
				float const h = kCoefficient * t;
				return(h * expf(1.0f - h));
			}
			explicit sImpulse(float const kCo) : functor(kCo) {}
		} Impulse;
		
		typedef struct sParabola : functor					// Configurable dualsided fast ramp up/down
		{																						// eg.) value noise edges, creates nice tiles
			__attribute__((const)) inline virtual float const operator()(float const t) const
			{
				return( powf(4.0f * t * (1.0f - t), kCoefficient) );
			}
			explicit sParabola(float const kCo) : functor(kCo) {}
		} Parabola;
		
	} // endnamespace

	template<typename T = uint8_t>
	STATIC_INLINE T const getBlueNoiseSample(vec2_t const uv)
	{
		return( OLED::Texture::textureSampleBilinear_1D<T, NOISE_TEXTURE_SATBITS, OLED::Texture::WRAP, uint8_t>(BlueNoise_x256, uv) );
	}
	template<typename T = uint8_t>
	STATIC_INLINE T const getRandomBlueNoiseSample()
	{
		vec2_t uv;
		
		// Precise random uv coordinates
		uv.x = RandomFloat();
		uv.y = RandomFloat();
		
		return( getBlueNoiseSample(uv) );
	}
	
	// alllow changing pointer on the fly to different unique permutations
	void SetPermutationDataStorage(uint8_t* const __restrict& __restrict PermutationDataStorage);
	void SetDefaultNativeDataStorage();
	
	// Both of these function update the internal pointer to permutation storage
	void NewNoisePermutation(uint8_t* const __restrict& __restrict PermutationDataStorage);  // user provided storage for unique permutations must be 512bytes in size
	void NewNoisePermutation(); // will overwrite native permutation storage
	
	// Value noise is the simplest and fastest, I find it poroduces better results in some cases too than both
	// perline and simplex. Depends on the application small enough to be a ramfunc
	__ramfunc float const getValueNoise( float x, float y, interpolator::functor const& interpFunctor );
	
	// Perlin Simplex Noise supersedes Perlin "Classic" noise. Higher quality at equal or better performance
	// however they are very different from eachother, perlin noise for example is "smoother" over a greyscale gradient
	// if the best realtime noise generation is required, use simplex noise instead
	NOINLINE float const getPerlinNoise( float x, float y, float z, interpolator::functor const& interpFunctor );
	
	NOINLINE float const getSimplexNoise2D( float x, float y);
	
	NOINLINE float const getSimplexNoise3D( float x, float y, float z );
	
	/*
	//==============================================================
	// otaviogood's noise from https://www.shadertoy.com/view/ld2SzK
	//--------------------------------------------------------------
	// This spiral noise works by successively adding and rotating sin waves while increasing frequency.
	// It should work the same on all computers since it's not based on a hash function like some other noises.
	// It can be much faster than other noise functions if you're ok with some repetition.
	*/
	STATIC_INLINE_PURE float const getSpiralNoise3D( float x, float y, float z, float t ) // small enough to be inlined
	{
		static constexpr float const nudge = 4.0f;	// size of perpendicular vector
		static constexpr float const normalizer = 1.0f / 4.1231056256176605498214098559741f; /*sqrt(1.0 + nudge*nudge)*/	// pythagorean theorem on that perpendicular to maintain scale
		static constexpr float const iterincrementalscale = 1.733733f;
		
		{
		// x - y * floor(x/y) = m
		//t = -fmodf(t * 0.2f,2.0f); // noise amount ***** ACKK!!!
			float const x(t * 0.2f);
			t = -(x - 2.0f * float32::__floorf(x*0.5f)); // avoid 14cycle div cost + function overhead of fmodf c lib
		}
		
		float iter(2.0f);
		for (uint32_t i = 6; i != 0; --i)
		{
				// add sin and cos scaled inverse with the frequency
				t += -__fabsf(__sinf(y*iter) + __cosf(x*iter)) / iter;	// abs for a ridged look
				// rotate by adding perpendicular and scaling down
				//p.xy += vec2(p.y, -p.x) * nudge;
				x = (x + y * nudge) * normalizer;
				y = (y + -x * nudge) * normalizer;
				//p.xy *= normalizer;
			
				// rotate on other axis                                                                                                                                                  /////////////////ther axis
				//p.xz += vec2(p.z, -p.x) * nudge;
				x = (x + z * nudge) * normalizer;
				z = (z + -x * nudge) * normalizer;
				//p.xz *= normalizer;
			
				// increase the frequency
				iter *= iterincrementalscale;
		}
		return(t);
	}
	STATIC_INLINE_PURE float const 	getSpiralNoise3D( vec3_t vP, float t ) {
		return( getSpiralNoise3D( vP.x, vP.y, vP.z, t ) );
	}
} // end namespace

#endif

