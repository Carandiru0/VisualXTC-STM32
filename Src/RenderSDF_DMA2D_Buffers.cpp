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
#include "oled.h"

namespace SDF
{
namespace SDFPrivate
{

uint8_t _AlphaMaskBuffer[OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT]	 // 8bpp,     16KB
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".bss")));	// bss places it a lower priority (higher address) which
																											// saves DTCM Memory for data that is defined in a section w/o .bss
	
}//end namespace
}//end namespace