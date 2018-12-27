/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef EXT_SRAM_H
#define EXT_SRAM_H

//#define DO_RAMTEST_AT_STARTUP  // will NOT clobber any scatterloaded data, as test is done right before Scatterload routine in DTCM cpp file

static constexpr uint32_t const EXTSRAM_ADDRESS = 0x60000000;
static constexpr uint32_t const EXTSRAM_ADDRESS_BITS = 19;
static constexpr uint32_t const EXTSRAM_NBYTES = (1 << EXTSRAM_ADDRESS_BITS);
namespace ExtSRAM
{
	extern uint8_t const* const ExtSRAM_Address;
	
void Init();
uint32_t const RamTest(uint32_t& NbBytes, float& tWrite, float& tRead);

template <uint32_t const RegionNumber>
void MPU_Config()
{
  // Configure the MPU attributes as readwrite for external SRAM 
	// cacheable, bufferable, normal(RAM), not shareable, memory can be read and cached out of order
	// fastest setting if not using DMA, On (read) misses it updates the block in the
	// main memory and brings the block to the cache -- for external memories only SRAM has WBWA capability! in comparison to SDRAM must be WT.
  LL_MPU_ConfigRegion(RegionNumber, 0x00, EXTSRAM_ADDRESS, 
         LL_MPU_REGION_SIZE_512KB | LL_MPU_REGION_FULL_ACCESS | LL_MPU_ACCESS_BUFFERABLE |
         LL_MPU_ACCESS_CACHEABLE | LL_MPU_ACCESS_NOT_SHAREABLE | LL_MPU_TEX_LEVEL1 |
         LL_MPU_INSTRUCTION_ACCESS_DISABLE);
}

} // endn amespace


#endif

