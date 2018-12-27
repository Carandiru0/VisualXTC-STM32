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
#include "ext_sram.h"

#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_gpio.h"

#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_sram.h"

#include "FRAM\FRAM_SDF_MemoryLayout.h"
#include "FLASH\Imports.h"

#include "rng.h"

namespace ExtSRAM
{
uint8_t const* const ExtSRAM_Address __attribute__((section (".dtcm_const"))) = (uint8_t const* const)(EXTSRAM_ADDRESS);
}

SRAM_HandleTypeDef sramHandle;

static void InitGPIO()
{
	LL_GPIO_InitTypeDef GPIO_InitStruct;
	
	/** FMC GPIO Configuration  
  PF0   ------> FMC_A0
  PF1   ------> FMC_A1
  PF2   ------> FMC_A2
  PF3   ------> FMC_A3
  PF4   ------> FMC_A4
  PF5   ------> FMC_A5
  PF12   ------> FMC_A6
  PF13   ------> FMC_A7
  PF14   ------> FMC_A8
  PF15   ------> FMC_A9
  PG0   ------> FMC_A10
  PG1   ------> FMC_A11
  PE7   ------> FMC_D4
  PE8   ------> FMC_D5
  PE9   ------> FMC_D6
  PE10   ------> FMC_D7
  PD11   ------> FMC_A16
  PD12   ------> FMC_A17
  PD13   ------> FMC_A18
  PD14   ------> FMC_D0
  PD15   ------> FMC_D1
  PG2   ------> FMC_A12
  PG3   ------> FMC_A13
  PG4   ------> FMC_A14
  PG5   ------> FMC_A15
  PC7   ------> FMC_NE1
  PD0   ------> FMC_D2
  PD1   ------> FMC_D3
  PD4   ------> FMC_NOE
  PD5   ------> FMC_NWE
  */
	
	/* GPIO Ports Clock Enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOF);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOG);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);
	
	// GPIOF
	GPIO_InitStruct.Pin = LL_GPIO_PIN_0|LL_GPIO_PIN_1|LL_GPIO_PIN_2|LL_GPIO_PIN_3 
                          |LL_GPIO_PIN_4|LL_GPIO_PIN_5|LL_GPIO_PIN_12|LL_GPIO_PIN_13 
                          |LL_GPIO_PIN_14|LL_GPIO_PIN_15;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_12;
  LL_GPIO_Init(GPIOF, &GPIO_InitStruct);
	
	// GPIOG
	GPIO_InitStruct.Pin = LL_GPIO_PIN_0|LL_GPIO_PIN_1|LL_GPIO_PIN_2|LL_GPIO_PIN_3 
                          |LL_GPIO_PIN_4|LL_GPIO_PIN_5;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_12;
  LL_GPIO_Init(GPIOG, &GPIO_InitStruct);
	
	// GPIOE
	GPIO_InitStruct.Pin = LL_GPIO_PIN_7|LL_GPIO_PIN_8|LL_GPIO_PIN_9|LL_GPIO_PIN_10;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_12;
  LL_GPIO_Init(GPIOE, &GPIO_InitStruct);
	
	// GPIOD
	GPIO_InitStruct.Pin = LL_GPIO_PIN_11|LL_GPIO_PIN_12|LL_GPIO_PIN_13|LL_GPIO_PIN_14 
                          |LL_GPIO_PIN_15|LL_GPIO_PIN_0|LL_GPIO_PIN_1|LL_GPIO_PIN_4 
                          |LL_GPIO_PIN_5;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_12;
  LL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	
	// GPIOC -- CE
	GPIO_InitStruct.Pin = LL_GPIO_PIN_7;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_9;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

namespace ExtSRAM
{
	
void Init()
{
	InitGPIO();
	
	FMC_NORSRAM_TimingTypeDef Timing = {0};
	
	LL_AHB3_GRP1_EnableClock( LL_AHB3_GRP1_PERIPH_FMC );
	
	/* SRAM device configuration */
  sramHandle.Instance = FMC_NORSRAM_DEVICE;
  sramHandle.Extended = FMC_NORSRAM_EXTENDED_DEVICE;
  	
  /* Timing configuration derived from system clock (up to 216Mhz)
     for 108Mhz as SRAM clock frequency */
	// 5.0ns = 1 HCLK @ 200Mhz, 4.63ns = 1 HCLK @ 216Mhz, 3.84ns = 1 HCLK @ 260Mhz
	// SRAM Write cycle time : 8ns
	//      Read  cycle time : 8ns
	//      WE low pulse width : 6.5ns - 8ns
	//      Address Access time : 8ns
	
	// Timing values tweaked to levels that do not fail torture test
  Timing.AddressSetupTime      = 3;		// # of HCLK's, n * HCLK
  Timing.AddressHoldTime       = 0;		
  Timing.DataSetupTime         = 3;		

  Timing.AccessMode            = FMC_ACCESS_MODE_A;
  
  sramHandle.Init.NSBank             = FMC_NORSRAM_BANK1;
  sramHandle.Init.DataAddressMux     = FMC_DATA_ADDRESS_MUX_DISABLE;
  sramHandle.Init.MemoryType         = FMC_MEMORY_TYPE_SRAM;
  sramHandle.Init.MemoryDataWidth    = FMC_NORSRAM_MEM_BUS_WIDTH_8;
  sramHandle.Init.BurstAccessMode    = FMC_BURST_ACCESS_MODE_DISABLE;
  sramHandle.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW;
  sramHandle.Init.WaitSignalActive   = FMC_WAIT_TIMING_BEFORE_WS;
  sramHandle.Init.WriteOperation     = FMC_WRITE_OPERATION_ENABLE;
  sramHandle.Init.WaitSignal         = FMC_WAIT_SIGNAL_DISABLE;
  sramHandle.Init.ExtendedMode       = FMC_EXTENDED_MODE_DISABLE;
  sramHandle.Init.AsynchronousWait   = FMC_ASYNCHRONOUS_WAIT_DISABLE;
  sramHandle.Init.WriteBurst         = FMC_WRITE_BURST_DISABLE;
  sramHandle.Init.ContinuousClock    = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;
	sramHandle.Init.WriteFifo 				 = FMC_WRITE_FIFO_ENABLE;
  sramHandle.Init.PageSize 					 = FMC_PAGE_SIZE_NONE;
	
  /* Initialize the SRAM controller */
  if(HAL_SRAM_Init(&sramHandle, &Timing, &Timing) != HAL_OK)
  {
    /* Initialization Error */
    OnError_Handler();
  }
}

static bool const RamTest_Simple()
{
	char const szTestSimple[] = "This is a test, a simple test\n";
	
	/* Write data to the SRAM memory */
  HAL_SRAM_Write_8b(&sramHandle, (uint32_t*)ExtSRAM_Address, (uint8_t*)(szTestSimple), countof(szTestSimple));

  /* Read back data from the SRAM memory */
	char szRxBuffer[countof(szTestSimple)] = {0};
	
  HAL_SRAM_Read_8b(&sramHandle, (uint32_t*)ExtSRAM_Address, reinterpret_cast<uint8_t*>(szRxBuffer), countof(szRxBuffer));

	for ( int i = countof(szRxBuffer) - 1 ; i >= 0 ; --i ) {
		
		if ( szTestSimple[i] != szRxBuffer[i] )
			return(false);
	}
	
	return(true);
}
static uint32_t const RamTest_AllBytes()
{
	uint32_t tCounter(1 << EXTSRAM_ADDRESS_BITS);
	uint32_t uiCharCounter(0);
	
	uint8_t const* psramaddress = (uint8_t const*)ExtSRAM_Address;
	
	while ( tCounter != 0 ) {
		
		/* Write data to the SRAM memory */
		HAL_SRAM_Write_32b(&sramHandle, (uint32_t*)psramaddress, (uint32_t*)(&uiCharCounter), 1);
		
		uint32_t uiReadBack(0);
		HAL_SRAM_Read_32b(&sramHandle, (uint32_t*)psramaddress, (uint32_t*)(&uiReadBack), 1);
		
		if ( uiCharCounter != uiReadBack )
			return(tCounter);
		
		++uiCharCounter;
		++psramaddress;
	
		--tCounter;
	}
	
	return(0);
}

// BlueNoise_x256 moved to external FRAM on QSPI Interface
// thus this test is now bottlenecked by the QSPI speed and will give
// a much lower result
// this is due to the read required from FRAM over QSPI
// INFACT the write speed would only ever be correct if we did a internal SRAM to external SRAM transfer
// so this has been renamed and is stilll useful for testing max QSPI speed instead
static uint32_t const RamTest_Torture_QSPI_FRAM(uint32_t& NbBytes, float& tWrite, float& tRead)
{
	static constexpr uint32_t const NOISE_TEXTURE_SATBITS = Constants::SATBIT_256,
																  NOISE_TEXTURE_NBBYTES = (1 << NOISE_TEXTURE_SATBITS)*(1 << NOISE_TEXTURE_SATBITS);
	
	uint32_t const NbIterationTotal(EXTSRAM_NBYTES / NOISE_TEXTURE_NBBYTES);
	uint32_t NbBytesVerified(0);
	uint32_t tStart, tSum(0);
	
	NbBytes = 0; tWrite = 1.0f; tRead = 1.0f; // avoid division by zero
			
	/* Write data to the SRAM memory */
	uint8_t const* psramaddress = (uint8_t const*)ExtSRAM_Address;
	for ( uint32_t iIter = 0 ; iIter < NbIterationTotal; ++iIter ) {
		
		tStart = micros();
		
		HAL_SRAM_Write_8b(&sramHandle, (uint32_t*)psramaddress, (uint8_t*)(BlueNoise_x256), NOISE_TEXTURE_NBBYTES);

		tSum += (micros() - tStart);
		
		psramaddress += NOISE_TEXTURE_NBBYTES;
	}

	tWrite = ((float)tSum/(float)NbIterationTotal) * 1e-6f;		// scale so seconds equal 1.0f
	NbBytes = NOISE_TEXTURE_NBBYTES * (EXTSRAM_NBYTES / NOISE_TEXTURE_NBBYTES);
	
	// read back and compare (bytes) simultaneously
	tSum = 0;
	psramaddress = (uint8_t const*)ExtSRAM_Address;
	
	for ( uint32_t iIter = 0 ; iIter < (EXTSRAM_NBYTES / NOISE_TEXTURE_NBBYTES) ; ++iIter ) {
		
		tStart = micros(); 
		
		for(uint32_t iDx = 0; iDx < NOISE_TEXTURE_NBBYTES; ++iDx)
		{
			if (*(BlueNoise_x256 + iDx) != *(psramaddress + iDx) ) {
				return(NbBytesVerified); // error memory does not match
			}
			++NbBytesVerified;
		}
		
		tSum += (micros() - tStart);
		
		psramaddress += NOISE_TEXTURE_NBBYTES;
	}
	tRead = ((float)tSum/(float)NbIterationTotal) * 1e-6f;		// scale so seconds equal 1.0f
	
	return(NbBytesVerified);
}

uint32_t const RamTest_Addressing()
{
	for (uint32_t iDx = 3 ; iDx < 19 ; ++iDx ) // 19 bits A0 --> A18, starting at A3 which will be equal to one byte
	{																					 // Size is (1 << 19), last addressable bit is one bit less the size
																						 // (1 << 3) == 8 to (1 << 18) == 262144 to (1 << 19) == 524288 - 1
		uint8_t const uiRandomCharacter = RandomNumber(0, UINT8_MAX);
		uint32_t const AddressOffset = (1 << iDx);
		
		// Write one byte at the start of every address alignment
		HAL_SRAM_Write_8b(&sramHandle, (uint32_t*)(ExtSRAM_Address + AddressOffset), (uint8_t*)&uiRandomCharacter, 1);
		
		// Readback and verify the character generated for this address alignment
		uint8_t uiReadBack(0);
		HAL_SRAM_Read_8b(&sramHandle, (uint32_t*)(ExtSRAM_Address + AddressOffset), (uint8_t*)&uiReadBack, 1);
		
		if ( uiRandomCharacter != uiReadBack ) {
			// return furthest address checked
			return(iDx);
		}
	}
	
	{	// check far byte
		
		uint8_t const uiRandomCharacter = RandomNumber(0, UINT8_MAX);
		uint32_t const Offset = (1 << EXTSRAM_ADDRESS_BITS) - sizeof(uint8_t) - 1;
		
		HAL_SRAM_Write_8b(&sramHandle, (uint32_t*)(ExtSRAM_Address + Offset), (uint8_t*)&uiRandomCharacter, 1);
		
		uint8_t uiReadBack(0);
		HAL_SRAM_Read_8b(&sramHandle, (uint32_t*)(ExtSRAM_Address + Offset), (uint8_t*)&uiReadBack, 1);
		
		if ( uiRandomCharacter != uiReadBack ) {
			// return furthest address checked
			return(EXTSRAM_ADDRESS_BITS - 1);
		}
	}
			
	// single byte verify at each offset of addressing tested ok
	return(EXTSRAM_ADDRESS_BITS);
}

uint32_t const RamTest(uint32_t& NbBytes, float& tWrite, float& tRead) // time in seconds, where 1 second is 1.0f
{
	
#ifndef PROGRAM_SDF_TO_FRAM
	
	if ( RamTest_Simple() ) {
				
		uint32_t const AddressWalked = RamTest_Addressing();
		
		if ( EXTSRAM_ADDRESS_BITS == AddressWalked ) {
			
			uint32_t const BytesNotWalked = RamTest_AllBytes();
			
			if ( 0 == BytesNotWalked ) {
				
				return( /*NbBytes*/ RamTest_Torture_QSPI_FRAM(NbBytes, tWrite, tRead) );
			}
			else {
				NbBytes = BytesNotWalked;
			// return 0, not verified
			}
			
		}
		else {
			NbBytes = AddressWalked;
			// return 0, not verified
		}
		
	}
#endif
	
	return(0);
}


} // end namespace