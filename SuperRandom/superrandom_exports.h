/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef _SUPERRANDOM_EXPORTS_H
#define _SUPERRANDOM_EXPORTS_H
#include <stdint.h>
#include <stdbool.h>

#ifndef __ramfunc
#define __ramfunc __attribute__((section (".ramfunc"))) __attribute__((noinline))
#endif

// Header Only Implementation of STM32 True & Psuedo Random Numbers //
// **** include superrandom.h in .c file rng.c of the project //
// **** include superrandom_exports.h in .h file rng.h of the project //
// rng.c must include LL STM32 drivers for RNG
// rng.c must implement MX_Init_NG which calls InitializeRandomNumberGenerators at end
// IRQ routine is provided, RNG_DataReady_Callback, already implemented

__ramfunc float const RandomFloat(/* Range is 0.0f to 1.0f*/void);
__ramfunc int32_t const RandomNumber(int32_t const iMin, int32_t const iMax);
__ramfunc int32_t const RandomNumber16(int32_t const iMin, int32_t const iMax);
__ramfunc bool const Random5050(void);

// These Functions are for the Psuedo RNG - Deterministic by setting seed value
__ramfunc void PsuedoResetSeed();
__ramfunc void PsuedoSetSeed( int32_t const Seed );
__ramfunc float const PsuedoRandomFloat(/* Range is 0.0f to 1.0f*/void);
__ramfunc int32_t const PsuedoRandomNumber(int32_t const iMin, int32_t const iMax);
__ramfunc int32_t const PsuedoRandomNumber16(int32_t const iMin, int32_t const iMax);
__ramfunc bool const PsuedoRandom5050(void);

void HashSetSeed( int32_t const Seed );
__ramfunc uint32_t const Hash( int32_t const data );

__ramfunc extern void RNG_DataReady_Callback(void);

#endif


