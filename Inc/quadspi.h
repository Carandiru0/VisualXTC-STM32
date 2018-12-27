/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#ifndef __quadspi_H
#define __quadspi_H

#include "PreProcessorCore.h"
#include "stm32f7xx_hal_msp.h"

//#define DUAL_MODE_ENABLED

/* Includes ------------------------------------------------------------------*/

static constexpr uint32_t const QUADSPI_ADDRESS = QSPI_BASE;
namespace QuadSPI_FRAM
{	
#ifdef DUAL_MODE_ENABLED
	static constexpr uint32_t const FRAM_SIZE_BITMASK = 19,
#else
	static constexpr uint32_t const FRAM_SIZE_BITMASK = 18,
#endif
																	FRAM_SIZE_BYTES = (1 << FRAM_SIZE_BITMASK);
	
	static constexpr uint32_t const QSPI_OP_OK = 0,
																	QSPI_OP_BUSY = (1<<1),
																	QSPI_OP_ERROR = (1<<2);
		
	static constexpr uint32_t const QSPI_PROGRAMMINGSTATE_IDLE = 0,
																  QSPI_PROGRAMMINGSTATE_PENDING = (1 << 1),
																	QSPI_PROGRAMMINGSTATE_COMPLETE = (1 << 2),
																	QSPI_PROGRAMMINGSTATE_ERROR = (1 << 3);
	
	static constexpr uint32_t const QSPI_VERIFY_PROGRAMMINGSTATE_IDLE = 0,
																  QSPI_VERIFY_PROGRAMMINGSTATE_PENDING = (1 << 1),
																	QSPI_VERIFY_PROGRAMMINGSTATE_COMPLETE = (1 << 2),
																	QSPI_VERIFY_PROGRAMMINGSTATE_ERROR = (1 << 3);
	
	static constexpr uint32_t const QSPI_PROGRAMMINGERROR_NONE = 0,
																  QSPI_PROGRAMMINGERROR_WRITEENABLEFAIL = (1 << 1),
																	QSPI_PROGRAMMINGERROR_WRITEFAIL = (1 << 2),
																	QSPI_PROGRAMMINGERROR_SIZEFAIL = (1 << 3),
																	QSPI_PROGRAMMINGERROR_WRITEDISABLEFAIL = (1 << 4),
																	QSPI_PROGRAMMINGERROR_READFAIL = (1 << 5),
																	QSPI_PROGRAMMINGERROR_NOTEQUAL = (1 << 6),
																	QSPI_PROGRAMMINGERROR_WRITEINCOMPLETE = (1 << 7),
																	QSPI_MEMORYMAPPED_ENABLEFAIL = (1 << 8),
																	QSPI_PROGRAMMINGERROR_VERIFYINCOMPLETE = (1 << 9),
																	QSPI_PROGRAMMINGERROR_SLEEPFAIL = (1 << 10);
	
	typedef struct sProgramMemory
	{
		uint8_t const* const SrcMemory;
		uint32_t const			 SrcNbBytes;
		
		constexpr explicit sProgramMemory(uint8_t const* const srcMemory, uint32_t const srcNbBytes)
			: SrcMemory(srcMemory), SrcNbBytes(srcNbBytes)
		{}
	} const ProgrammingDesc;
		
	extern uint8_t const* const QSPI_Address;
	
	void Abort();
	void ResetVerifyFRAMState();
		
	NOINLINE void Update(uint32_t const tNow);
	
	NOINLINE uint32_t const MemoryMappedMode();
	uint32_t const AckComplete_ResetProgrammingState();
	uint32_t const ProgramFRAM(uint32_t& NbBytesProgrammed, ProgrammingDesc const programmingDesc);
	uint32_t const VerifyFRAM(uint32_t& NbBytesVerified, uint32_t const& NbBytesExpectedProgrammed, uint8_t const* Address, ProgrammingDesc const programmedList);
	
	__attribute__((pure)) bool const IsWriteMemoryComplete();
	uint32_t const WriteEnable();
	uint32_t const WriteMemory(uint8_t const* const pDataOut, uint32_t const Address, uint32_t const NbBytes);
	uint32_t const WriteDisable();
	
	uint32_t const GetProgrammingState();
	uint32_t const GetLastProgrammingError();
	uint32_t const GetHALQSPIErrorCode();
/* USER CODE END Private defines */

NOINLINE void Init(void);
	
template <uint32_t const RegionNumber>
void MPU_Config()
{
	// As per ST AN 4861, to prevent speculative read accesses by Cortex M7, the maximum addressing space
	// for QSPI should all be set to Strongly ordered. Then the "real" address space is set with an overlapped region
	// of normal, cacheable, bufferable attributes. Both execute never.
	LL_MPU_ConfigRegion(RegionNumber, 0x00, QUADSPI_ADDRESS, 
         LL_MPU_REGION_SIZE_256MB | LL_MPU_REGION_NO_ACCESS | LL_MPU_ACCESS_NOT_BUFFERABLE |
         LL_MPU_ACCESS_NOT_CACHEABLE | LL_MPU_ACCESS_NOT_SHAREABLE | LL_MPU_TEX_LEVEL0 |
         LL_MPU_INSTRUCTION_ACCESS_DISABLE);
	
  // Configure the MPU attributes as readonly for FRAM  at QSPI memorymapped address
	// cacheable, bufferable, normal(RAM), not shareable, memory can be read and cached out of order
	// fastest setting if not using DMA, On (read) misses it updates the block in the
	// main memory and brings the block to the cache
  LL_MPU_ConfigRegion(RegionNumber+1, 0x00, QUADSPI_ADDRESS, 
#ifdef DUAL_MODE_ENABLED
         LL_MPU_REGION_SIZE_512KB 
#else
				 LL_MPU_REGION_SIZE_256KB
#endif
				 | LL_MPU_REGION_PRIV_RO_URO | LL_MPU_ACCESS_BUFFERABLE |
         LL_MPU_ACCESS_CACHEABLE | LL_MPU_ACCESS_NOT_SHAREABLE | LL_MPU_TEX_LEVEL1 |
         LL_MPU_INSTRUCTION_ACCESS_DISABLE);
}
} // end namespace

#endif /*__ quadspi_H */


