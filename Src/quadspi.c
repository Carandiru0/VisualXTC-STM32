/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

/* Includes ------------------------------------------------------------------*/
#include "quadspi.h"
#include "globals.h"

#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_gpio.h"

#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_qspi.h"

#include "commonmath.h"
#include "debug.cpp"

#include "dma.h"

namespace QuadSPI_FRAM
{
uint8_t const* const QSPI_Address __attribute__((section (".dtcm_const"))) = (uint8_t const* const)(QUADSPI_ADDRESS);
}

QSPI_HandleTypeDef hqspi;
// **** DMA Limited to 64K in a single transfer, using IT instead ********
/*DMA_HandleTypeDef hdma_quadspi;*/

namespace QuadSPI_FRAM
{
	
static struct sQuadSPI
{
	static constexpr bool const QSPI_OP_COMPLETE = false,
															QSPI_OP_PENDING = true;
	
	
	static constexpr uint16_t const STATUSBITS_WEL = 0x0202;
	static constexpr uint32_t const OPCODE_WRITEENABLE = 0x06,
																	OPCODE_RESETWRITEENABLE = 0x04,
																	OPCODE_READSTATUS = 0x05,
																	OPCODE_READMEM = 0x03,
																	OPCODE_WRITEMEM = 0x02,
																	OPCODE_FASTREADMEM = 0x0B,
																	OPCODE_SLEEP = 0xB9;
	
	static constexpr uint32_t const SLEEP_TIME = 5000; // 5s
	static constexpr uint32_t const MAX_BYTES_SINGLE_TRANSMIT = 96384;
	
	uint32_t 					NbBytesRemaining;
	
	uint32_t 					ProgrammingState,
										ProgrammingError,
										VerifyProgrammingState;

	uint32_t					tAlive;
	
	bool							IsSleeping,
										IsWritesDisabled;
	
	volatile uint32_t tLastTimeout;
	
	volatile bool			txComplete,
										rxComplete,
										bMemoryMappedMode;
	
	sQuadSPI()
	: ProgrammingState(QSPI_PROGRAMMINGSTATE_IDLE), ProgrammingError(QSPI_PROGRAMMINGERROR_NONE), 
	  VerifyProgrammingState(QSPI_VERIFY_PROGRAMMINGSTATE_IDLE), tAlive(0),
		NbBytesRemaining(0), IsWritesDisabled(false), IsSleeping(false)
	{
		tLastTimeout = 0;
		txComplete = QSPI_OP_COMPLETE;
		rxComplete = QSPI_OP_COMPLETE;
		bMemoryMappedMode = false;
	}
} oQuadSPI;

} // end namespace
using namespace QuadSPI_FRAM;

namespace QuadSPI_FRAM
{
// Blocking call //
// WEL bit is cleared on powerup, after any rising CS of a WRITE command, or WRDI command, Wrinting to fram is then disabled
// in these conditions. So a WriteEnable must be sent before any new WRITE command
uint32_t const WriteEnable()
{
	QSPI_CommandTypeDef sCommand;
	QSPI_AutoPollingTypeDef sConfig;

	sCommand.Instruction       = sQuadSPI::OPCODE_WRITEENABLE;
	sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
	sCommand.Address     			 = 0;
	sCommand.AddressMode       = QSPI_ADDRESS_NONE;
	sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
	sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
	sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
	sCommand.DummyCycles       = 0;
	
	sCommand.DataMode = QSPI_DATA_NONE;
	
	if (oQuadSPI.bMemoryMappedMode) {
			Abort();
			LL_mDelay(1);
	}
	
	if ( HAL_OK == HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) )
	{
		/* Configure automatic polling mode to wait for write enabling ---- */  
		sConfig.Match           = sQuadSPI::STATUSBITS_WEL;	// Write Enable Latched (bit 1)
		sConfig.Mask            = sQuadSPI::STATUSBITS_WEL;	// mask bit position 1
		sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
#ifdef DUAL_MODE_ENABLED
		sConfig.StatusBytesSize = 2; // one byte from each fram in dual mode
#else
		sConfig.StatusBytesSize = 2;
#endif
		sConfig.Interval        = 8;
		sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

		// Change command to read status register for autopolling
		sCommand.Instruction    = sQuadSPI::OPCODE_READSTATUS;
		sCommand.DataMode       = QSPI_DATA_1_LINE;

		if (HAL_OK == HAL_QSPI_AutoPolling(&hqspi, &sCommand, &sConfig, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)) { //blocking
			oQuadSPI.IsWritesDisabled = false;
			return( QSPI_OP_OK ); // callback should set only on match
		}
	}

	return(QSPI_OP_ERROR);
}
// Blocking call //
uint32_t const WriteDisable()
{
	if ( !oQuadSPI.IsWritesDisabled ) {
		
		/*if ( !oQuadSPI.IsSleeping &&
		   QSPI_PROGRAMMINGSTATE_IDLE == oQuadSPI.ProgrammingState &&
		   sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.rxComplete &&
			 sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.txComplete )	// removed causes bugs, don't add back
		{*/
			QSPI_CommandTypeDef sCommand;
			QSPI_AutoPollingTypeDef sConfig;
			
			sCommand.Instruction       = sQuadSPI::OPCODE_RESETWRITEENABLE;
			sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
			sCommand.Address     			 = 0;
			sCommand.AddressMode       = QSPI_ADDRESS_NONE;
			sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
			sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
			sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
			sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
			sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
			sCommand.DummyCycles       = 0;
			
			sCommand.DataMode = QSPI_DATA_NONE;
			
			if ( HAL_OK == HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) )
			{
				/* Configure automatic polling mode to wait for write disabling ---- */  
				sConfig.Match           = 0;	// Write Enable Not Latched (bit 1)
				sConfig.Mask            = sQuadSPI::STATUSBITS_WEL;	// mask bit position 1
				sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
#ifdef DUAL_MODE_ENABLED
				sConfig.StatusBytesSize = 2; // one byte from each fram in dual mode
#else
				sConfig.StatusBytesSize = 2; // one byte from each fram in dual mode
#endif
				sConfig.Interval        = 8;
				sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

				// Change command to read status register for autopolling
				sCommand.Instruction    = sQuadSPI::OPCODE_READSTATUS;
				sCommand.DataMode       = QSPI_DATA_1_LINE;

				if (HAL_OK == HAL_QSPI_AutoPolling(&hqspi, &sCommand, &sConfig, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)) { //blocking
					oQuadSPI.IsWritesDisabled = true;
					return( QSPI_OP_OK ); // callback should set only on match
				}
			} // command
		/*} // if busy...
		else
			return(QSPI_OP_BUSY);*/
	}
	else
		return( QSPI_OP_OK ); // already disabled
	
	return(QSPI_OP_ERROR);
}
} // end namespace

// Non-Blocking call //
static uint32_t const Sleep()
{
	QSPI_CommandTypeDef sCommand;
	
	if ( !oQuadSPI.IsSleeping &&
		   QSPI_PROGRAMMINGSTATE_IDLE == oQuadSPI.ProgrammingState &&
		   QSPI_VERIFY_PROGRAMMINGSTATE_IDLE == oQuadSPI.VerifyProgrammingState &&
			 sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.rxComplete &&
			 sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.txComplete )	// Finish any current dma operation first, don't goto sleep if busy!!
	{
		sCommand.Instruction       = sQuadSPI::OPCODE_SLEEP;
		sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
		sCommand.Address     			 = 0;
		sCommand.AddressMode       = QSPI_ADDRESS_NONE;
		sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
		sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
		sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
		sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
		sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
		sCommand.DummyCycles       = 0;
		
		sCommand.DataMode = QSPI_DATA_NONE;
		
		if (oQuadSPI.bMemoryMappedMode)
			Abort();
		
		if ( HAL_OK == HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) ) {
			oQuadSPI.IsSleeping = true;
			return( QSPI_OP_OK ); // callback should set only on match
		}

	}
	else
		return(QSPI_OP_BUSY);
	
	return(QSPI_OP_ERROR);
}

/* CS
	GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_9;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);
*/
// chipselected if low
STATIC_INLINE void CS_L() { LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_10); }
STATIC_INLINE void CS_H() { LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_10); }
static uint32_t const WakeUp(bool const bWriteDisable = true)
{
	// Issue WriteDisable Command to wake Up
	if (oQuadSPI.IsSleeping) {
			
		if ( sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.rxComplete &&
			   sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.txComplete )	// Finish any current dma operation first
		{
			CS_L();
			LL_mDelay(1);	// must be at least 450us before sleep mode is deactivated
			CS_H();
			
			oQuadSPI.IsSleeping = false;
			
			// Commands may now follow
#if defined(_DEBUG_OUT_OLED) && defined(QSPI_AUTOSLEEP_SLEEPWAKE_STATE_OLED) 
			DebugMessage( "FRAM Wake Up %d", millis());
#endif
			if ( bWriteDisable )
			{
				if ( QSPI_OP_OK != WriteDisable() )
				{
#if defined(_DEBUG_OUT_OLED) && defined(QSPI_ERROR_OLED) 
					DebugMessage( "FRAM Wake Up WRDI Fail =%d", GetHALQSPIErrorCode());
#endif
					return(QSPI_OP_ERROR);
				}
			}
		}
		else
			return(QSPI_OP_BUSY);
	}
	
	// Keep alive if there is activity, called from Memorymapped
	// otherwise there is fast toggling between sleeping and wakeup
	oQuadSPI.tAlive = millis();
	
	return(QSPI_OP_OK);
}

// Non-blocking call //
static uint32_t const WriteMemory(uint8_t const* const pDataOut, uint32_t const NbBytes)
{
	QSPI_CommandTypeDef sCommand;
	
	if ( !oQuadSPI.IsWritesDisabled && sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.txComplete )
	{
		sCommand.Instruction       = sQuadSPI::OPCODE_WRITEMEM;
		sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
		sCommand.AddressMode       = QSPI_ADDRESS_1_LINE;
		sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
		//sCommand.AlternateBytes		 = QSPI_ALTERNATE_BYTES_8_BITS;
		//sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_1_LINE;
		sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
		sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
		sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
		sCommand.SIOOMode          = QSPI_SIOO_INST_ONLY_FIRST_CMD;
		sCommand.DummyCycles       = 0;
		
		sCommand.Address     = 0;
		sCommand.DataMode    = QSPI_DATA_1_LINE;
		sCommand.NbData      = NbBytes;
		
		if (oQuadSPI.bMemoryMappedMode)
			Abort();
		
		Abort(); // aborts any ongoing operation
		
		if ( HAL_OK == HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) )
		{
			// Required b4 dma, ensure data is commited to memory from cache
			SCB_CleanDCache_by_Addr((uint32_t*)pDataOut, NbBytes);
			
			oQuadSPI.txComplete = sQuadSPI::QSPI_OP_PENDING;
			return( HAL_OK == HAL_QSPI_Transmit_IT(&hqspi, const_cast<uint8_t* const>(pDataOut)) ? QSPI_OP_OK : QSPI_OP_ERROR  );
		}
	}
	else
		return(QSPI_OP_BUSY);
	
	return(QSPI_OP_ERROR);
}

namespace QuadSPI_FRAM
{	
// Non-blocking call //
uint32_t const WriteMemory(uint8_t const* const pDataOut, uint32_t const Address, uint32_t const NbBytes)
{
	QSPI_CommandTypeDef sCommand;
	
	if ( !oQuadSPI.IsWritesDisabled && sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.txComplete )
	{
		WriteEnable(); // required for every new write memory
		
		sCommand.Instruction       = sQuadSPI::OPCODE_WRITEMEM;
		sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
		sCommand.AddressMode       = QSPI_ADDRESS_1_LINE;
		sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
		//sCommand.AlternateBytes		 = QSPI_ALTERNATE_BYTES_8_BITS;
		//sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_1_LINE;
		sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
		sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
		sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
		sCommand.SIOOMode          = QSPI_SIOO_INST_ONLY_FIRST_CMD;
		sCommand.DummyCycles       = 0;
		
		sCommand.Address     = Address;
		sCommand.DataMode    = QSPI_DATA_1_LINE;
		sCommand.NbData      = NbBytes;
		
		if (oQuadSPI.bMemoryMappedMode)
			Abort();
		
		Abort(); // aborts any ongoing operation
		
		if ( HAL_OK == HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) )
		{
			// Required b4 dma, ensure data is commited to memory from cache
			SCB_CleanDCache_by_Addr((uint32_t*)pDataOut, NbBytes);
			
			oQuadSPI.txComplete = sQuadSPI::QSPI_OP_PENDING;
			return( HAL_OK == HAL_QSPI_Transmit_IT(&hqspi, const_cast<uint8_t* const>(pDataOut)) ? QSPI_OP_OK : QSPI_OP_ERROR  );
		}
	}
	else
		return(QSPI_OP_BUSY);
	
	return(QSPI_OP_ERROR);
}
} // end namespace

// Non-blocking call //
static uint32_t const ReadMemory(uint32_t const Address, uint8_t* const& pDataInBuffer, uint32_t const NbBytes)
{
	QSPI_CommandTypeDef sCommand;
	
	if ( sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.rxComplete )
	{
		sCommand.Instruction       = sQuadSPI::OPCODE_FASTREADMEM;
		sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
		sCommand.AddressMode       = QSPI_ADDRESS_1_LINE;
		sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
		sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
		sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
		sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
		sCommand.SIOOMode          = QSPI_SIOO_INST_ONLY_FIRST_CMD;
		
		sCommand.DummyCycles       = 8;
		
		sCommand.Address     = Address;
		sCommand.DataMode    = QSPI_DATA_1_LINE;
		sCommand.NbData      = NbBytes;
		
		if (oQuadSPI.bMemoryMappedMode)
			Abort();
		
		if ( HAL_OK == HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) )
		{
			oQuadSPI.rxComplete = sQuadSPI::QSPI_OP_PENDING;
			return( HAL_OK == HAL_QSPI_Receive_IT(&hqspi, pDataInBuffer) ? QSPI_OP_OK : QSPI_OP_ERROR );
		}
	}
	else
		return(QSPI_OP_BUSY);
	
	return(QSPI_OP_ERROR);
}

static uint32_t const MemoryMappedMode_Internal()
{
	QSPI_CommandTypeDef sCommand;
	QSPI_MemoryMappedTypeDef sMemMappedCfg;
	
	if ( QSPI_PROGRAMMINGSTATE_IDLE == oQuadSPI.ProgrammingState &&
			 sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.rxComplete &&
			 sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.txComplete )	// Finish any current dma operation first
	{
		sCommand.Instruction       = sQuadSPI::OPCODE_READMEM; // FASTREADMEM inserts a "delay" of dummy cycles
		sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;		// it's working at max speed with regular READMEM, less overhead
		sCommand.AddressMode       = QSPI_ADDRESS_1_LINE;					// FASTREADMEM IS LEGACY SUPPORT FOR A FLASH COMMAND
		sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;					// FRAM WORKS AT WIRE SPEED, NO DELAY NEEDED
		sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
		sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
		sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
		sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
		
		sCommand.DummyCycles       = 0;
		sCommand.Address     			 = 0;
		sCommand.DataMode    			 = QSPI_DATA_1_LINE;
		
		sMemMappedCfg.TimeOutPeriod = 0;
		sMemMappedCfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
		
		// bugfix, enabling the timeout counter causes a crash shortly after loading and displaying voxel model
		//sMemMappedCfg.TimeOutPeriod = 1024 << 1;	// # of CLK to wait for timeout
		//sMemMappedCfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_ENABLE;
		
		Abort();
		
		if ( HAL_OK == HAL_QSPI_MemoryMapped(&hqspi, &sCommand, &sMemMappedCfg)) {
			oQuadSPI.tLastTimeout = millis();
			oQuadSPI.bMemoryMappedMode = true;
			return(QSPI_OP_OK);
		}
	}
	else
		return(QSPI_OP_BUSY);
	
	return(QSPI_OP_ERROR);
}

__attribute__((pure)) STATIC_INLINE bool const IsReadMemoryComplete()
{
	return(sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.rxComplete);
}
namespace QuadSPI_FRAM
{	
__attribute__((pure)) bool const IsWriteMemoryComplete()
{
	return(sQuadSPI::QSPI_OP_COMPLETE == oQuadSPI.txComplete);
}
} // end namespace

STATIC_INLINE void InvalidateReadMemory(uint8_t* const& pDataInBufferInvalidate, uint32_t const NbBytes)
{
	if (IsReadMemoryComplete()) {
		SCB_InvalidateDCache_by_Addr( (uint32_t*)pDataInBufferInvalidate, NbBytes);
	}
}

namespace QuadSPI_FRAM
{

void Abort()
{
	HAL_QSPI_Abort(&hqspi);
	__WFI();
	oQuadSPI.bMemoryMappedMode = false;
}

void ResetVerifyFRAMState()
{
	oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_IDLE;	// reset is done here for verify
}

NOINLINE uint32_t const MemoryMappedMode()
{
	uint32_t const uiWakeUpResult(WakeUp());
	// Wake up if sleeping in either state of memorymapping being currently enabled or not (keep alive)
	if ( QSPI_OP_OK != uiWakeUpResult )
		return(uiWakeUpResult);
	
	if (!oQuadSPI.bMemoryMappedMode) {	// Not running
		return( ::MemoryMappedMode_Internal() );
	}
	
	return(QSPI_OP_OK);
}

/* QUADSPI init function */
NOINLINE void Init(void)
{
	LL_AHB3_GRP1_EnableClock( LL_AHB3_GRP1_PERIPH_QSPI );
	
	// QSPI interrupt Init - set to normal for IT for programming is done infrequently, else its MemoryMapped Mode
	NVIC_SetPriority(QUADSPI_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_NORMAL, 0));
  NVIC_EnableIRQ(QUADSPI_IRQn);
	
  hqspi.Instance = QUADSPI;
  hqspi.Init.ClockPrescaler = 5;	// AHB3 Clock = 216Mhz,Configure Clock Prescaler = 4 => QSPI_CLK = (HCLK / 5) = 43.2 MHz (working!)
  hqspi.Init.FifoThreshold = 4;
  hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;// default setting
  hqspi.Init.FlashSize = FRAM_SIZE_BITMASK - 1;		// Flash size + 1 = bits addressable (should be set as full capacity of the 2 fram chips in dual mode)
  hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE; // delay between commands
  hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
	
#ifdef DUAL_MODE_ENABLED
	hqspi.Init.FlashID = QSPI_FLASH_ID_1; // doesn't matter for dual mode
  hqspi.Init.DualFlash = QSPI_DUALFLASH_ENABLE;//QSPI_DUALFLASH_ENABLE; FMC SRAM has data lines on same pin as qspi bank 2
#else
	hqspi.Init.FlashID = QSPI_FLASH_ID_1;
	hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;//QSPI_DUALFLASH_ENABLE; FMC SRAM has data lines on same pin as qspi bank 2
#endif

	LL_mDelay(1); // Must be 1ms before first access and power on of FRAM Init
	HAL_QSPI_Init(&hqspi);
	
	LL_mDelay(1); // Must be 1ms before first access and power on of FRAM Init

	if ( QSPI_OP_OK != WriteDisable() )
	{
#if defined(_DEBUG_OUT_OLED) && defined(QSPI_ERROR_OLED) 
		DebugMessage( "FRAM WriteDisable Init Fail =%d", GetHALQSPIErrorCode());
#endif
	}
}

NOINLINE void Update(uint32_t const tNow)
{
#if QSPI_AUTOSLEEP_FRAM
	static constexpr uint32_t const UPDATE_INTERVAL = sQuadSPI::SLEEP_TIME >> 1; // ms
	static uint32_t tLastUpdate;
	
	static bool bInitTime(true);

	if ( tNow - tLastUpdate > UPDATE_INTERVAL )  // only poll sleep infrequently
	{
		if ( likely(!bInitTime) )
		{
			if ( !oQuadSPI.IsSleeping ) // is not sleeping only
			{
				// can't use math max, operates on int, to maintain uint range here
				uint32_t const tLastTimeout(oQuadSPI.tLastTimeout); // copy out atomic for more than one opration
				uint32_t const tLastActivity = (oQuadSPI.tAlive > tLastTimeout ? oQuadSPI.tAlive : tLastTimeout);
				// Using millis here for accuracy instead of tNow, if an interrupt happens
				// and upates the aomic variable to be greater than what is currently in tNow
				// then we have a negative result. Causing overflow on a uint, no negative!
				if ( millis() - tLastActivity > sQuadSPI::SLEEP_TIME )
				{
					uint32_t const State = Sleep();
					if ( QSPI_OP_OK == State )
					{ 
#if defined(_DEBUG_OUT_OLED) && defined(QSPI_AUTOSLEEP_SLEEPWAKE_STATE_OLED) 
						DebugMessage( "FRAM Sleeping %d", tLastActivity);
#endif			
					}
					else if ( QSPI_OP_ERROR == State )
					{
#if defined(_DEBUG_OUT_OLED) && defined(QSPI_ERROR_OLED) 
						DebugMessage( "FRAM Cant Sleep Fail =%d", GetHALQSPIErrorCode());
#endif						
					}
					// else QuadSPI - FRAM is busy ignore, not going to sleep
				}
			} // !isSleeping
		} // inittime
		else {
			oQuadSPI.tLastTimeout = tNow;
			oQuadSPI.tAlive = tNow;
			
			bInitTime = false;
		}
		
		tLastUpdate = tNow;
	} // t Update Interval
#else
	UNUSED(tNow);
#endif

}

uint32_t const AckComplete_ResetProgrammingState()
{
	if ( QSPI_PROGRAMMINGSTATE_COMPLETE == oQuadSPI.ProgrammingState )
	{
		if ( QSPI_OP_OK == WriteDisable() ) {
			oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_IDLE;
			oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_NONE;
		}
		else {
			oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_ERROR;
			oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_WRITEDISABLEFAIL;
		}
	}
	
	return(oQuadSPI.ProgrammingState);
}

uint32_t const ProgramFRAM(uint32_t& NbBytesProgrammed, ProgrammingDesc const programmingDesc)
{
	static uint32_t PendingNbBytes(0);
	
	if ( QSPI_PROGRAMMINGSTATE_IDLE == oQuadSPI.ProgrammingState )
	{
		// Start programming if currently idle	
		oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_IDLE;	// reset is also done here for verif
		oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_PENDING;
		 
		NbBytesProgrammed = 0;
		PendingNbBytes = 0;

		goto Programming;

	}
	else if ( QSPI_PROGRAMMINGSTATE_PENDING == oQuadSPI.ProgrammingState )
	{
	
Programming:
		switch( WakeUp(false) )  // Prevent sleeping, keep alive
		{
			case QSPI_OP_BUSY:
				__WFI(); // beware: puts mcu in lowe power-mode
				return(oQuadSPI.ProgrammingState);
			case QSPI_OP_ERROR:
				oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_ERROR;
				oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_SLEEPFAIL;
				return(oQuadSPI.ProgrammingState);
			case QSPI_OP_OK:
			default:
				break;
		}
		if ( IsWriteMemoryComplete() )
		{
			NbBytesProgrammed += PendingNbBytes;
			PendingNbBytes = 0;
			
			if ( 0 != NbBytesProgrammed ) {

				if (HAL_QSPI_ERROR_NONE == GetHALQSPIErrorCode() && programmingDesc.SrcNbBytes == NbBytesProgrammed) {
					oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_COMPLETE;
					return(oQuadSPI.ProgrammingState);
				}
				else {
					oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_ERROR;
					oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_WRITEINCOMPLETE;
					return(oQuadSPI.ProgrammingState);
				}
			}

			if ((programmingDesc.SrcNbBytes) >= FRAM_SIZE_BYTES) {
				oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_ERROR;
				oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_SIZEFAIL;
				return(oQuadSPI.ProgrammingState);
			}
			
			// Start programming if currently idle			
			if ( QSPI_OP_OK == WriteEnable() ) // there must be a writeenable before any new write operation
			{
				switch ( ::WriteMemory(programmingDesc.SrcMemory, programmingDesc.SrcNbBytes) )
				{
					case QSPI_OP_OK:
						PendingNbBytes = programmingDesc.SrcNbBytes;
						__WFI(); // beware: puts mcu in lowe power-mode
						break;
					case QSPI_OP_ERROR:
						oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_ERROR;
						oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_WRITEFAIL;
						return(oQuadSPI.ProgrammingState);
					case QSPI_OP_BUSY:
					default:
						__WFI(); // beware: puts mcu in lowe power-mode
						// ignore, try on next pass
						break;
				}
			} 
			else {
				oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_ERROR;
				oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_WRITEENABLEFAIL;
				return(oQuadSPI.ProgrammingState);
			}
		} // if isCompleted
		else
		{		
			if ( 0 != PendingNbBytes )
			{
				// Check State
				switch( HAL_QSPI_GetState(&hqspi) )
				{
					case HAL_QSPI_STATE_BUSY_INDIRECT_TX:
					case HAL_QSPI_STATE_BUSY:
						if (HAL_QSPI_ERROR_NONE == GetHALQSPIErrorCode())
							break;	// Should still be PENDING
						else {
							oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_ERROR;
							oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_WRITEFAIL;
							return(oQuadSPI.ProgrammingState);
						}
					case HAL_QSPI_STATE_ERROR:
						oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_ERROR;
						oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_WRITEFAIL;
						return(oQuadSPI.ProgrammingState);
					default:
						oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_ERROR;
						oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_NONE;			// This is an error, but an unknown state still want to catch it
						return(oQuadSPI.ProgrammingState);
				}
			}
			else {
				oQuadSPI.ProgrammingState = QSPI_PROGRAMMINGSTATE_ERROR;
				oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_WRITEFAIL;
				return(oQuadSPI.ProgrammingState);
			}
		} // busy...
	} // if QSPI_PROGRAMMINGSTATE_PENDING
	return(oQuadSPI.ProgrammingState);
}

/*
uint32_t const VerifyFRAM(uint32_t& NbBytesVerified, uint8_t* const& DstVerifyMemory, std::initializer_list<ProgrammingDesc> const programmedList)
{
	static uint32_t Address(0),
									listIndex(0);
	
	static int32_t listIndexPendingVerifyBytes(-1);
	
	// want acknowledgement first of completion
	// resets to idle state and disables writing to the device memory
	if ( QSPI_PROGRAMMINGSTATE_IDLE == oQuadSPI.ProgrammingState )
	{
		if (QSPI_VERIFY_PROGRAMMINGSTATE_IDLE == oQuadSPI.VerifyProgrammingState)
		{
			oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_PENDING;
			oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_NONE;
			Address = 0;
			listIndex = 0;
			listIndexPendingVerifyBytes = -1;
			goto Verify;
		}
		else if (QSPI_VERIFY_PROGRAMMINGSTATE_PENDING == oQuadSPI.VerifyProgrammingState)
		{
			
Verify:
			if ( IsReadMemoryComplete() )
			{
				if ( listIndexPendingVerifyBytes >= 0 )
				{
					// Verify last Read Completion
					auto iterVerifyProgramDesc = programmedList.begin();
			
					{
						int32_t gotoIndex(listIndexPendingVerifyBytes);
						while( 0 != gotoIndex) {
							
							++iterVerifyProgramDesc;
							--gotoIndex;
						}
						
						InvalidateReadMemory(DstVerifyMemory, iterVerifyProgramDesc->SrcNbBytes);
						
						int32_t bytePos(iterVerifyProgramDesc->SrcNbBytes - 1);
						while ( bytePos >= 0 )
						{
							if ( iterVerifyProgramDesc->SrcMemory[bytePos] != DstVerifyMemory[bytePos] ) {
								oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_ERROR;
								oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_NOTEQUAL;
								return(oQuadSPI.VerifyProgrammingState);
							}
							
							--bytePos;
							++NbBytesVerified;
						}
					}
				}
				
				auto i = programmedList.begin();
			
				{
					int32_t gotoIndex(listIndex);
					while( 0 != gotoIndex) {
						
						if ( programmedList.end() == ++i) {
							oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_COMPLETE;
							return(oQuadSPI.VerifyProgrammingState);
						}
						--gotoIndex;
					}
				}
				
				if ((Address + i->SrcNbBytes) >= FRAM_SIZE_BYTES) {
					oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_ERROR;
					oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_SIZEFAIL;
					return(oQuadSPI.VerifyProgrammingState);
				}
			
				switch ( ReadMemory(Address, DstVerifyMemory, i->SrcNbBytes) )
				{
					case sQuadSPI::QSPI_OP_OK:
						listIndexPendingVerifyBytes = listIndex;
						Address += i->SrcNbBytes;
						++listIndex;
						break;
					case sQuadSPI::QSPI_OP_ERROR:
						oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_ERROR;
						oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_READFAIL;
						return(oQuadSPI.VerifyProgrammingState);
					case sQuadSPI::QSPI_OP_BUSY:
					default:
						// ignore, try on next pass
						break;
				}
			} // isReady
		} // if QSPI_VERIFY_PROGRAMMINGSTATE_PENDING
	} // if QSPI_PROGRAMMINGSTATE_IDLE
	
	return(oQuadSPI.VerifyProgrammingState);
}*/


uint32_t const VerifyFRAM(uint32_t& NbBytesVerified, uint32_t const& NbBytesExpectedProgrammed, uint8_t const* Address, ProgrammingDesc const programmedList)
{
	// want acknowledgement first of completion
	// resets to idle state and disables writing to the device memory
	if ( QSPI_PROGRAMMINGSTATE_IDLE == oQuadSPI.ProgrammingState )
	{
		if (QSPI_VERIFY_PROGRAMMINGSTATE_IDLE == oQuadSPI.VerifyProgrammingState)
		{		
			switch( MemoryMappedMode() )
			{
				case QSPI_OP_BUSY:
					return(oQuadSPI.VerifyProgrammingState);
				case QSPI_OP_ERROR:
					oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_ERROR;
					oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_SLEEPFAIL;
					return(oQuadSPI.VerifyProgrammingState);
				case QSPI_OP_OK:
				default:
					break;
			}

			oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_PENDING;
			oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_NONE;
	
			goto Verify;
		}
		else if (QSPI_VERIFY_PROGRAMMINGSTATE_PENDING == oQuadSPI.VerifyProgrammingState)
		{
Verify:
			
			if ( ((((uint32_t const)Address) + programmedList.SrcNbBytes) - ((uint32_t const)QSPI_Address)) >= FRAM_SIZE_BYTES) {
				oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_ERROR;
				oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_SIZEFAIL;
				return(oQuadSPI.VerifyProgrammingState);
			}
			
			uint8_t const* SrcMemory(programmedList.SrcMemory);
			for ( uint32_t iDx = 0 ; iDx < programmedList.SrcNbBytes ; ++iDx )
			{
				if (*Address != *SrcMemory) {
					oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_ERROR;
					oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_NOTEQUAL;
					return(oQuadSPI.VerifyProgrammingState);
				}
				
				++Address;
				++SrcMemory;
				
				++NbBytesVerified;
			}
			
			if (HAL_QSPI_ERROR_NONE == GetHALQSPIErrorCode() && NbBytesExpectedProgrammed == NbBytesVerified) {
				oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_COMPLETE;
			}
			else {
				oQuadSPI.VerifyProgrammingState = QSPI_VERIFY_PROGRAMMINGSTATE_ERROR;
				oQuadSPI.ProgrammingError = QSPI_PROGRAMMINGERROR_VERIFYINCOMPLETE;
			}
		} // if QSPI_VERIFY_PROGRAMMINGSTATE_PENDING
	} // if QSPI_PROGRAMMINGSTATE_IDLE
	
	return(oQuadSPI.VerifyProgrammingState);
}

uint32_t const GetProgrammingState()
{
	return(oQuadSPI.ProgrammingState);
}

uint32_t const GetLastProgrammingError()
{
	return(oQuadSPI.ProgrammingError);
}

//#define HAL_QSPI_ERROR_NONE            ((uint32_t)0x00000000U) /*!< No error           */
//#define HAL_QSPI_ERROR_TIMEOUT         ((uint32_t)0x00000001U) /*!< Timeout error      */
//#define HAL_QSPI_ERROR_TRANSFER        ((uint32_t)0x00000002U) /*!< Transfer error     */
//#define HAL_QSPI_ERROR_DMA             ((uint32_t)0x00000004U) /*!< DMA transfer error */
//#define HAL_QSPI_ERROR_INVALID_PARAM   ((uint32_t)0x00000008U) /*!< Invalid parameters error */
uint32_t const GetHALQSPIErrorCode()
{
	return( HAL_QSPI_GetError(&hqspi) );
}

} // end namespace 
// *** CALLBACKS (overrides of __weak function in HAL QSPI c source) **** //
// **** these are only called if there is an interrupt, if a blocking method is used these are not triggered
extern "C" {

/**
  * @brief  Rx Transfer completed callbacks.
  * @param  hqspi: QSPI handle
  * @retval None
  */
void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
	QuadSPI_FRAM::oQuadSPI.rxComplete = QuadSPI_FRAM::sQuadSPI::QSPI_OP_COMPLETE;
	QuadSPI_FRAM::oQuadSPI.tLastTimeout = millis(); // Sleep uses this to know when to gotoSleep
}

/**
  * @brief  Tx Transfer completed callbacks.
  * @param  hqspi: QSPI handle
  * @retval None
  */
void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
	QuadSPI_FRAM::oQuadSPI.txComplete = QuadSPI_FRAM::sQuadSPI::QSPI_OP_COMPLETE;
	QuadSPI_FRAM::oQuadSPI.tLastTimeout = millis(); // Sleep uses this to know when to gotoSleep
	//CS_H();
}

void HAL_QSPI_TimeOutCallback(QSPI_HandleTypeDef *hqspi)
{
	QuadSPI_FRAM::oQuadSPI.tLastTimeout = millis(); // Sleep uses this to know when to gotoSleep
}

#if defined(_DEBUG_OUT_OLED) && defined(QSPI_ERROR_OLED) && !defined(PROGRAM_SDF_TO_FRAM)
void HAL_QSPI_ErrorCallback(QSPI_HandleTypeDef *hqspi)
{
	DebugMessage( "FRAM MemorxMapped Error =%d", GetHALQSPIErrorCode()); // Heavy stack usage for this call, only used if debugging QSPI
}
#endif

}

