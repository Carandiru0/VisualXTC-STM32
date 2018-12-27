/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#include "globals.h"
#include "DTCM_Reserve.h"

/* ############## */
uint8_t const _dithertable[64]																// 64 bytes
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))						// for fast reads
__attribute__((used))
__attribute__((section (".dtcm_const"))) = { // 0-255 dither table
			3,129,34,160,10,136,42,168,192,66,223,97,200,73,231,105,50,
			176,18,144,58,184,26,152,239,113,207,81,247,121,215,89,14,
			140,46,180,7,133,38,164,203,77,235,109,196,70,227,101,62,188,
			30,156,54,180,22,148,251,125,219,93,243,117,211,85 };// 0-255 dither table 

extern "C" uint8_t const globalasm_dithertable[64]
	__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((used))
	__attribute__((alias("_dithertable"))); // for assembler code
			
namespace DTCM
{			
	
uint8_t _sram_top_ao_bothsides[top_ao_stride*top_ao_numoflines],							// 276 bytes
			  _sram_top_ao_oneside[top_ao_stride*top_ao_numoflines],								// 276bytes
				_sram_top_ao_oneside_mirrored[top_ao_stride*top_ao_numoflines],				// ""
				_sram_side_7_both_ao[side_ao_stride*side_ao_numoflines],							// 396 bytes
				_sram_side_7_both_ao_mirrored[side_ao_stride*side_ao_numoflines],			// ""
				_sram_side_7_botom_ao[side_ao_stride*side_ao_numoflines],							// 396 bytes
				_sram_side_7_botom_ao_mirrored[side_ao_stride*side_ao_numoflines]			// ""
__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
__attribute__((section (".ext_sram")));
		
NOINLINE void ScatterLoad()
{	
	// Copy AO mask textures to DTCM section of Internal SRAM
	memcpy8(DTCM::_sram_top_ao_bothsides, _flash_top_ao_bothsides, top_ao_stride*top_ao_numoflines);
	
	memcpy8(DTCM::_sram_top_ao_oneside, _flash_top_ao_oneside, top_ao_stride*top_ao_numoflines);
	memcpy8_mirrored(DTCM::_sram_top_ao_oneside_mirrored, DTCM::_sram_top_ao_oneside, top_ao_stride, top_ao_numoflines);
	
	memcpy8(DTCM::_sram_side_7_both_ao, _flash_side_7_both_ao, side_ao_stride*side_ao_numoflines);
	memcpy8_mirrored(DTCM::_sram_side_7_both_ao_mirrored, DTCM::_sram_side_7_both_ao, side_ao_stride, side_ao_numoflines);
	
	memcpy8(DTCM::_sram_side_7_botom_ao, _flash_side_7_botom_ao, side_ao_stride*side_ao_numoflines);
	memcpy8_mirrored(DTCM::_sram_side_7_botom_ao_mirrored, DTCM::_sram_side_7_botom_ao, side_ao_stride, side_ao_numoflines);
	
}

}//endnamespace
	




