/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

xorshift and hashing algorithms are Copyright belonging to their respectful creators.
 */

#ifndef _SUPER_RANDOM_H
#define _SUPER_RANDOM_H

// Header Only Implementation of STM32 True & Psuedo Random Numbers //
// **** include superrandom.h in .c file rng.c of the project //
// **** include superrandom_exports.h in .h file rng.h of the project //
// rng.c must include LL STM32 drivers for RNG
// rng.c must implement MX_Init_RNG which calls InitializeRandomNumberGenerators at end
// IRQ routine is provided, RNG_DataReady_Callback, already implemented

// define DETERMINISTIC_KEY_SEED as a uint32 value if program key seed can't be random
// good for creating the same numbers across two seperate machines
// otherwise reccomend using random program key seed if isolated to one machine
/*
eg.) in rng.c file:
	#define DETERMINISTIC_KEY_SEED 0xC00C1001
	#include "superrandom.h"
*/

#ifndef DETERMINISTIC_KEY_SEED
#define RANDOM_KEY_SEED 1
#define DETERMINISTIC_KEY_SEED 0
#else
#define RANDOM_KEY_SEED 0
#endif

#ifdef ARMV7M_DCACHE_LINESIZE
#define ALIGN_TO_CACHELINE __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
#else
#define ALIGN_TO_CACHELINE 
#endif

static struct sRandom
{
	#define SEED_BUFFERPOSITIONS 32								// must be power of 2
	
	__IO uint32_t aRandom32bit[2][SEED_BUFFERPOSITIONS];

	__IO uint32_t ReadIndex, WriteIndex;

	__IO uint32_t ReadPosition, WritePosition;

	uint32_t	hashSeed, reservedSeed;			// hashSeed is changeable, do not change reservedSed after initial INIT!!
	
} oRandom __attribute__((section (".dtcm")));

//updated to use new algo: xoshiro128** http://xoshiro.di.unimi.it/xoshiro128starstar.c
//by David Blackman and Sebastiano Vigna (vigna@acm.org)
static uint32_t xorshift_state[4]
	ALIGN_TO_CACHELINE 							// used a lot
	__attribute__((section (".dtcm"))) = {1,1,1,1}; // seeds must be non zero

STATIC_INLINE_PURE uint32_t const rotl(uint32_t const x, int const k) {
	return( (x << k) | (x >> (32 - k)) );
}

STATIC_INLINE uint32_t const xorshift_next(void) {
	uint32_t const result_starstar = rotl(xorshift_state[0] * 5, 7) * 9;

	uint32_t const t = xorshift_state[1] << 9;

	xorshift_state[2] ^= xorshift_state[0];
	xorshift_state[3] ^= xorshift_state[1];
	xorshift_state[1] ^= xorshift_state[2];
	xorshift_state[0] ^= xorshift_state[3];

	xorshift_state[2] ^= t;

	xorshift_state[3] = rotl(xorshift_state[3], 11);

	return(result_starstar - 1);	// avoids a strangle failure case by using the - 1
}

// internally used for distribution on range, 16bit numbers will be faster on ARM, but 32bit numbers on a x64 Intel will be faster
// this avoids using the modulo operator (no division), ref: https://github.com/lemire/fastrange
__attribute__((always_inline)) STATIC_INLINE uint32_t const RandomNumber_Limit_32(uint32_t const xrandx, uint32_t const uiMax) { // 1 to UINT32_MAX
	return( (uint32_t)(((uint64_t)xrandx * (uint64_t)uiMax) >> 32) );
}
__attribute__((always_inline)) STATIC_INLINE uint16_t const RandomNumber_Limit_16(uint16_t const xrandx, uint16_t const uiMax) { // 1 to 65535
	return( (uint16_t)(((uint32_t)xrandx * (uint32_t)uiMax) >> 16) );
}
__attribute__((always_inline)) STATIC_INLINE int32_t const RandomNumber_Limits_32(uint32_t const xrandx, int32_t const iMin, int32_t const iMax)
{
	return( ((int32_t)RandomNumber_Limit_32(xrandx, iMax))+iMin );
}
__attribute__((always_inline)) STATIC_INLINE int32_t const RandomNumber_Limits_16(uint32_t xrandx, int32_t const iMin, int32_t const iMax)
{
	// choose high or low halfword with comparison with zero (fast) to get usuable 16bit random number
	xrandx = ((int32_t)xrandx) >= 0 ? xrandx >> 16 : xrandx;
	return( ((int32_t)RandomNumber_Limit_16(xrandx, iMax))+iMin );
}

STATIC_INLINE uint32_t const GetNextTrueRandomNumber(void)
{
	uint32_t const uiReturn = oRandom.aRandom32bit[oRandom.ReadIndex][oRandom.ReadPosition];
	
	oRandom.ReadPosition = (oRandom.ReadPosition + 1) & (SEED_BUFFERPOSITIONS - 1);
	
	/* ReEnable RNG IT generation */
  LL_RNG_EnableIT(RNG);
	return(uiReturn);
}

__ramfunc float const RandomFloat(/* Range is 0.0f to 1.0f*/void)
{
	union
	{
		uint32_t const i;
		float const f;
	} const pun = { 0x3F800000U | (GetNextTrueRandomNumber() >> 9) };
	
	return(pun.f - 1.0f);
}

__ramfunc int32_t const RandomNumber(int32_t const iMin, int32_t const iMax)
{
  if (iMin == iMax) return(iMin);
  return( RandomNumber_Limits_32(GetNextTrueRandomNumber(), iMin, iMax) );
}
__ramfunc int32_t const RandomNumber16(int32_t const iMin, int32_t const iMax)  // iMax cannot exceed 65535, range will only be valid
{																																								// for 0 to 65535
  if (iMin == iMax) return(iMin);																								// or
  return( RandomNumber_Limits_16(GetNextTrueRandomNumber(), iMin, iMax) );			// -32768 to 32767
}
__ramfunc bool const Random5050(void)
{
	#define LIMIT5050 4095
	return( RandomNumber_Limits_16(GetNextTrueRandomNumber(), -LIMIT5050, LIMIT5050) < 0 );
}

/*public override uint GetHash (int buf) {    // xxHash Function https://blogs.unity3d.com/2015/01/07/a-primer-on-repeatable-random-numbers/
		uint h32 = (uint)seed + PRIME32_5;
		h32 += 4U;
		h32 += (uint)buf * PRIME32_3;
		h32 = RotateLeft (h32, 17) * PRIME32_4;
		h32 ^= h32 >> 15;
		h32 *= PRIME32_2;
		h32 ^= h32 >> 13;
		h32 *= PRIME32_3;
		h32 ^= h32 >> 16;
		return h32;
	}*/
void HashSetSeed( int32_t const Seed )
{
	oRandom.hashSeed = (uint32_t)Seed;
}

__ramfunc uint32_t const Hash( int32_t const data )
{
	static uint32_t const PRIME32_1 __attribute__((section (".dtcm_const"))) = 2654435761U;
	static uint32_t const PRIME32_2 __attribute__((section (".dtcm_const"))) = 2246822519U;
	static uint32_t const PRIME32_3 __attribute__((section (".dtcm_const"))) = 3266489917U;
	static uint32_t const PRIME32_4 __attribute__((section (".dtcm_const"))) = 668265263U;
	static uint32_t const PRIME32_5 __attribute__((section (".dtcm_const"))) = 374761393U;
	
	uint32_t h32 = (uint32_t)oRandom.hashSeed + PRIME32_5;
	h32 += 4U;
	h32 += (uint32_t)data * PRIME32_3;
	h32 = rotl(h32, 17) * PRIME32_4;
	h32 ^= h32 >> 15;
	h32 *= PRIME32_2;
	h32 ^= h32 >> 13;
	h32 *= PRIME32_3;
	h32 ^= h32 >> 16;
	
	return(h32);
}

__ramfunc void PsuedoSetSeed( int32_t const Seed )
{
	// save temp of current hashSeed
	uint32_t const tmpSaveHashSeed = oRandom.hashSeed;
	
	// set hashSeed temporarily to the static reservedSeed (used to maintain deterministic property of psuedo rng)
	oRandom.hashSeed = oRandom.reservedSeed;
	
	// perform high quality hash on user desired seed for psuedo rng before seeding the rng (quality increase++++)
	xorshift_state[0] = xorshift_state[1] = xorshift_state[2] = xorshift_state[3] = ( Hash(Seed) ) + 1; // psuedo rng seeds must be non zero
	xorshift_next(); // discard first value of rng sequence
	
	// restore originally saved  hash seed
	oRandom.hashSeed = tmpSaveHashSeed;
}
__ramfunc void PsuedoResetSeed()
{
	PsuedoSetSeed(oRandom.reservedSeed);
}

__ramfunc float const PsuedoRandomFloat(/* Range is 0.0f to 1.0f*/void)
{
	union
	{
		uint32_t const i;
		float const f;
	} const pun = { 0x3F800000U | (xorshift_next() >> 9) };
	
	return(pun.f - 1.0f);
}

__ramfunc int32_t const PsuedoRandomNumber(int32_t const iMin, int32_t const iMax)
{
  if (iMin == iMax) return(iMin);
  return( RandomNumber_Limits_32(xorshift_next(), iMin, iMax) );
}
__ramfunc int32_t const PsuedoRandomNumber16(int32_t const iMin, int32_t const iMax)  // iMax cannot exceed 65535, range will only be valid
{																																											// for 0 to 65535
  if (iMin == iMax) return(iMin);																											// or
  return( RandomNumber_Limits_16(xorshift_next(), iMin, iMax) );											// -32768 to 32767
}
__ramfunc bool const PsuedoRandom5050(void)
{
	#define LIMIT5050 4095
	return( RandomNumber_Limits_16(xorshift_next(), -LIMIT5050, LIMIT5050) < 0 );
}


static void InitializeRandomNumberGenerators(void)
{
	#define DUMP_ITERATIONS ((SEED_BUFFERPOSITIONS>>1) - 1)		// go thru both sides of buffer and fill values n/2 - 1 times
	
	// Seed 16 numbers to randomize startup a little
	uint32_t uiDump = DUMP_ITERATIONS;
	while (0 != uiDump) {
		
		volatile uint32_t const CurReadIndex = oRandom.ReadIndex;

		do {

			oRandom.reservedSeed = GetNextTrueRandomNumber();			// dumping into reservedSeed, initting reserveSeed with a true random number
			__WFI();
			
		} while (CurReadIndex == oRandom.ReadIndex);
		
		--uiDump;
	}
	
	// Reset positions and swap sides to overwrite and read brand new values from now on
	LL_RNG_DisableIT(RNG);
	oRandom.ReadIndex = 1;
	oRandom.WriteIndex = 0;
	oRandom.ReadPosition = 0;
	oRandom.WritePosition = 1;
	
	oRandom.reservedSeed = oRandom.reservedSeed + 1; // seeds must be nonzero
	
#if ((0 == RANDOM_KEY_SEED) && (0 != DETERMINISTIC_KEY_SEED))
	oRandom.reservedSeed = DETERMINISTIC_KEY_SEED; // overwrite
#endif
	LL_RNG_EnableIT(RNG);
	
	oRandom.hashSeed = oRandom.reservedSeed;	// reserved seed is used for the hashing of seeds pushed into the psuedo rng, initial hashSeed 
																						// is set to this aswell but can be changed, whereas reserved seed cannot to maintain deterministic psuedo rngs
	PsuedoResetSeed();
}

extern __ramfunc void RNG_DataReady_Callback(void)
{
	/* Disable RNG IT generation */
  LL_RNG_DisableIT(RNG);
	
	/* Value of generated random number could be retrieved and stored in dedicated array */
  oRandom.aRandom32bit[oRandom.WriteIndex][oRandom.WritePosition] = LL_RNG_ReadRandData32(RNG);
	
	oRandom.WritePosition = (oRandom.WritePosition + 1) & (SEED_BUFFERPOSITIONS - 1);
	
	// overflowed / wrapped back to zero? (SEED_BUFFERPOSITIONS filled up)
	if ( 0 == oRandom.WritePosition ) {
		uint32_t const tmpIndex = oRandom.ReadIndex;
		oRandom.ReadIndex  = oRandom.WriteIndex;
		oRandom.WriteIndex = tmpIndex;
		// swap sides, so the writes goto the old potentially used values side
	}	// new reads use the just finished writes side
}















#endif

