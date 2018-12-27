/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef JPEG_H
#define JPEG_H
#include <stdint.h>
#include "globals.h"
#include "commonmath.h"
#include "sdf.h"

namespace JPEGDecoder
{
	extern uint8_t _DecompressedBuffer[SDFConstants::SDF_DIMENSION*SDFConstants::SDF_DIMENSION]	 // 8bpp,     16KB
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".bss")));
	
	#define getDecompressionBuffer() _DecompressedBuffer
}

namespace JPEGDecoder
{
	static constexpr int32_t const NOT_READY = 0,
																 READY = 1,
																 TIMED_OUT = -1;
NOINLINE void Init();

uint32_t const Start_Decode(uint8_t const* const srcJPEG_MemoryBuffer, 
														uint32_t const sizeInBytesOfJPEG,
														bool const bOnFRAM = false);

int32_t const IsReady( uint32_t const idJPEG);
uint32_t const getCurrentMCUBlockIndex();
uint32_t const getCurrentMCUTotalNb();
												
//void GetInfo( JPEG_ConfTypeDef const*& pInfo );											 

uint32_t const GetState();
uint32_t const GetError();
											 
}		// end namespace							 
#endif

