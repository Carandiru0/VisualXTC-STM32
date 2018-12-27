/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef DTCM_RESERVE_H
#define DTCM_RESERVE_H
#include "commonmath.h"
#include "FLASH\Imports.h"

extern uint8_t const _dithertable[64]
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
__attribute__((section (".dtcm_const")));// 0-255 dither table 

/// *** TCM MemoriesTightly coupled memories) 
// DTCM & ITCM are NOT cacheable, they are as fast as the L1 cache 
// ONLY the region for SRAM1 & SRAM2 are cacheable - and require cache coherency maintance, eg.) DTCM does not apply for cache coherency operations!
namespace DTCM
{

NOINLINE void ScatterLoad();		// loads not only DTCM but some external SRAM aswell
	
#define ADDY static constexpr size_t const
	
ADDY DTCMRAM_START_ADDRESS  =      0x20000000;
ADDY DTCMRAM_END_ADDRESS    =      0x2001FFFFU;
ADDY DTCMRAM_AVAILABLE_SIZE =      (DTCMRAM_END_ADDRESS - DTCMRAM_START_ADDRESS + 1);   

								
																																								
// Mirror of settings in scatter loading file
ADDY ITCM_RAMFUNC_ADDRESS 	= 0x00000000;			// 16,384 bytes (16 KB) ITCM Superfast Memory total
ADDY DTCM_STACK_ADDRESS 		= 0x20000000;    	// 131,072 bytes (128KB) DTCM Superfast Memory total																						
ADDY DTCM_RWDATA_ADDRESS 		= 0x20000000;			// overlapping regions set for stack + DTCM+SRAM1+SRAM2
ADDY SRAM1_RWDATA_ADDRESS 	= 0x20020000;			// 376,832 bytes (384KB) non-DTCM (SRAM1) Memory total
ADDY SRAM2_RWDATA_ADDRESS 	= 0x2007C000;			// 16,384 bytes (16KB) non-DTCM (SRAM2) Memory total

// ITCM size is 16KB
// DTCM size is 128KB
// SRAM1 size is 368KB
// SRAM2 size is 16KB
// External SRAM size is 512KB 

// __attribute__((aligned(ARMV7M_DCACHE_LINESIZE))) is only good for arrays,buffers, structures - data that can be cached in increments of 32 bytes

// DTCM external buffers
extern uint8_t _sram_top_ao_bothsides[top_ao_stride*top_ao_numoflines],				// 276 bytes
			  _sram_top_ao_oneside[top_ao_stride*top_ao_numoflines],								// 276bytes
				_sram_top_ao_oneside_mirrored[top_ao_stride*top_ao_numoflines],				// ""
				_sram_side_7_both_ao[side_ao_stride*side_ao_numoflines],							// 396 bytes
				_sram_side_7_both_ao_mirrored[side_ao_stride*side_ao_numoflines],			// ""
				_sram_side_7_botom_ao[side_ao_stride*side_ao_numoflines],							// 396 bytes
				_sram_side_7_botom_ao_mirrored[side_ao_stride*side_ao_numoflines]			// ""
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
__attribute__((section (".ext_sram")));   // seems there is better concurrency while using external sram (AXI Bus), TCM bus may be too busy (ART, DTCM, ITCM, FLASH)

}//emdnamespace
#endif

