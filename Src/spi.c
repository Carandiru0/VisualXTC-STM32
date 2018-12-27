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
#include "spi.h"
#include "globals.h"
#include "dma.h"
#include "stm32f7xx_ll_dma.h"
#include "stm32f7xx_ll_spi.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_gpio.h"
#include "oled.h"
#include "arm_acle.h"


/* SPI1 init function */
NOINLINE void MX_SPI1_Init(void)
{
  LL_SPI_InitTypeDef SPI_InitStruct;
  LL_GPIO_InitTypeDef GPIO_InitStruct;
	
  /* Peripheral clock enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
  
	//NVIC_SetPriority(SPI1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_HIGH, 0));
	//NVIC_EnableIRQ(SPI1_IRQn);
	
  /**SPI1 GPIO Configuration  
  PA5   ------> SPI1_SCK	--------- Purplish Red
  PA7   ------> SPI1_MOSI 	------- Gray
	PE14  ------> SPI1_OLED_RESET --- Yellow
	PE12  ------> SPI1_OLED_DC  ----- Blue
	PE15  ------> SPI1_OLED_CS ------ Green
  */
	
	// SPI!_SCK
  GPIO_InitStruct.Pin = LL_GPIO_PIN_5;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_5;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// SPI_MOSI
  GPIO_InitStruct.Pin = LL_GPIO_PIN_7;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_5;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/**/
  GPIO_InitStruct.Pin = OLED::GPIO_OLED_RESET_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(OLED::GPIO_OLED_RESET_GPIO_Port, &GPIO_InitStruct);
  LL_GPIO_SetOutputPin(OLED::GPIO_OLED_RESET_GPIO_Port, OLED::GPIO_OLED_RESET_Pin);

  /**/
  GPIO_InitStruct.Pin = OLED::GPIO_OLED_DC_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(OLED::GPIO_OLED_DC_GPIO_Port, &GPIO_InitStruct);
	LL_GPIO_SetOutputPin(OLED::GPIO_OLED_DC_GPIO_Port, OLED::GPIO_OLED_DC_Pin);
	
  /**/
  GPIO_InitStruct.Pin = OLED::GPIO_OLED_CS_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(OLED::GPIO_OLED_CS_GPIO_Port, &GPIO_InitStruct);
	LL_GPIO_SetOutputPin(OLED::GPIO_OLED_CS_GPIO_Port, OLED::GPIO_OLED_CS_Pin);
	
  /* SPI1_TX Init */
	
  SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
  SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
  SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
  SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
  SPI_InitStruct.ClockPhase = LL_SPI_PHASE_1EDGE;
  SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
  SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV16; 				// was too high, program is running faster now
  SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;							 					// so exceeding the OLED's capability happenned
  SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;		// datasheet SPI transfer rate level of 10MHz
  SPI_InitStruct.CRCPoly = 7;																				// 240 x 64 = 15.36Kbit FrameBuffer Packed 4bit 16 Gray Levels (see OLED.cpp and SSD1322 datasheet)
  LL_SPI_Init(SPI1, &SPI_InitStruct);																// 120Hz * 15.36Kbit = 1.85Mhz bandwidth required for refresh rate of 120Hz
																																		// 260MHz/2 = 130MHz AHB / Prescaler (16) = 8.125MHz
  LL_SPI_SetStandard(SPI1, LL_SPI_PROTOCOL_MOTOROLA);								// 1.85MHz <= 8.125MHz <= 10 MHz  @ 16 Divider
																																		// 1.85MHz <= 16.25MHz !! 10 MHz  @ 8 Divider
  LL_SPI_DisableNSSPulseMgt(SPI1);																	// this is theoretical max rate, but in practice the speed is not reached
																																		// so over the max allowable rate is ok until the actual
																																		// speed of transmission exceeds the datasheet spec
																																		// usually some strange artifacts can be seen
																																		// Strange artifacts can be seen at 8 divider, so brought down to 16 divider
																																		// the resulting SPI speed still far exceeds needed bandwidth for 120Hz refresh
																																		// OLED Hardware Target framerate is 60Hz, 120Hz is ideal Nyquist Sampling (2x) (invisible to eyes)
																																		// Actual framerate of application varies from 8ms (125Hz) to 33ms (30Hz)
																																		// 33ms is max allowable frame time, 16ms is ideal software target
																																		
	// DMA Has not started until spi 3 init finished, dma is enabled on first transfer**
	// this is enabled on 1st transfer LL_DMA_EnableChannel
	LL_SPI_EnableDMAReq_TX(SPI1);
	//LL_SPI_EnableIT_ERR(SPI1);
	
	/* Enable SPI1, starts transfer and interuppts*/
  LL_SPI_Enable(SPI1);
}

void SPI1_TransmitByte(uint8_t const dataByte)
{
	// Wait for empty TX Buffer
	WaitForSPI1BufferEmpty(); 
	
	LL_SPI_TransmitData8(SPI1, dataByte);
}
void SPI1_TransmitHalfWord(uint16_t const dataHalfWord)
{
	// Wait for empty TX Buffer
	WaitForSPI1BufferEmpty();

	LL_SPI_TransmitData16(SPI1, dataHalfWord);
}
void SPI1_TransmitHalfWord(uint16_t const dataByteFirst, uint16_t const dataByteSecond)
{
	SPI1_TransmitHalfWord( (dataByteSecond << 8) | (dataByteFirst) );
}

/*
void SPI1_Transmit( uint32_t const MemoryAddress, uint32_t const uiNumBytes )
{
CheckDMABusy:
	if ( false != bOLEDBusy_DMA2 )
	{
		WaitForSPI1FreeDMA();
	}

	// SPI3 DMA Init
	bOLEDBusy_DMA2 = true;
	
	// SPI3_TX Init
	LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_3, MemoryAddress);
	LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_3, uiNumBytes);
	
	WaitForSPI1BufferEmpty();
	
	LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_3);
}
*/

void WaitForSPI1BufferEmpty(void)
{
	/*
	 *  a) flushed from the transmit buffer
	 */
	while ( RESET == LL_SPI_IsActiveFlag_TXE(SPI1) );//// __WFI(); // beware: puts mcu in lowe power-mode & greatly increase size of code 2KB
	
}
void WaitPendingSPI1ChipDeselect(void)
{
	// Only neccessary to WaitForSPI3DmaCompletion for "actual" completion of data being sent from hardware
	// if chip is to be deselected

	/*
	 * There is an unpleasant wait until we are certain the data has been sent.
	 * The need for this has been verified by oscilloscope. The shift register
	 * at this point may still be clocking out data and it is not safe to
	 * release the chip select line until it has finished. It only costs half
	 * a microsecond so better safe than sorry. Is it...
	 */
	
	/*
	 * b) flushed out of the shift register
	 */
	while ( LL_SPI_IsActiveFlag_BSY(SPI1) );//// __WFI();  // beware: puts mcu in lowe power-mode & greatly increase size of code 2KB
}


