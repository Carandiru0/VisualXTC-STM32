/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#include "stm32f7xx_hal_msp.h"
#include "main.h"
#include "globals.h"

// **** DMA Limited to 64K in a single transfer, using IT instead ********
/* extern DMA_HandleTypeDef 		hdma_quadspi; */
extern DMA_HandleTypeDef   	hdmaIn;
extern DMA_HandleTypeDef   	hdmaOut;
	
/**
  * @brief JPEG MSP Initialization
  *        This function configures the hardware resources used in this example:
  *           - Peripheral's clock enable
  *           - NVIC configuration for JPEG interrupt request enable
  *           - DMA configuration for transmission request by peripheral
  *           - NVIC configuration for DMA interrupt request enable
  * @param hjpeg: JPEG handle pointer
  * @retval None
  */
void HAL_JPEG_MspInit(JPEG_HandleTypeDef *hjpeg)
{  
  /* Input DMA */    
  /* Set the parameters to be configured */
  hdmaIn.Init.Channel = DMA_CHANNEL_9;
  hdmaIn.Init.Direction = DMA_MEMORY_TO_PERIPH;
  hdmaIn.Init.PeriphInc = DMA_PINC_DISABLE;
  hdmaIn.Init.MemInc = DMA_MINC_ENABLE;
  hdmaIn.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdmaIn.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE; // input memory alignment must be a byte to be compatible
  hdmaIn.Init.Mode = DMA_NORMAL;											// with FRAM QSPI Memorymapped space
  hdmaIn.Init.Priority = DMA_PRIORITY_HIGH;
  hdmaIn.Init.FIFOMode = DMA_FIFOMODE_ENABLE;         
  hdmaIn.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdmaIn.Init.MemBurst = DMA_MBURST_SINGLE;						// this must also be set, 1 byte, otherwise strange things happen
  hdmaIn.Init.PeriphBurst = DMA_PBURST_INC4;      		// wakeup/sleep fram fails...
  
  hdmaIn.Instance = DMA2_Stream0;
  
  /* DeInitialize the DMA Stream */
  //HAL_DMA_DeInit(&hdmaIn);  
  /* Initialize the DMA stream */
  HAL_DMA_Init(&hdmaIn);
  
  /* Associate the DMA handle */
  __HAL_LINKDMA(hjpeg, hdmain, hdmaIn);
	
  /* Output DMA */
  /* Set the parameters to be configured */ 
  hdmaOut.Init.Channel = DMA_CHANNEL_9;
  hdmaOut.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdmaOut.Init.PeriphInc = DMA_PINC_DISABLE;
  hdmaOut.Init.MemInc = DMA_MINC_ENABLE;
  hdmaOut.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;	// output can be aligned to 32bit 
  hdmaOut.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  hdmaOut.Init.Mode = DMA_NORMAL;
  hdmaOut.Init.Priority = DMA_PRIORITY_HIGH;
  hdmaOut.Init.FIFOMode = DMA_FIFOMODE_ENABLE;         
  hdmaOut.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdmaOut.Init.MemBurst = DMA_MBURST_INC4;
  hdmaOut.Init.PeriphBurst = DMA_PBURST_INC4;

  hdmaOut.Instance = DMA2_Stream1;
	
  /* DeInitialize the DMA Stream */
  //HAL_DMA_DeInit(&hdmaOut);  
  /* Initialize the DMA stream */
  HAL_DMA_Init(&hdmaOut);

  /* Associate the DMA handle */
  __HAL_LINKDMA(hjpeg, hdmaout, hdmaOut);
}

void HAL_QSPI_MspInit(QSPI_HandleTypeDef* qspiHandle)
{
	LL_GPIO_InitTypeDef GPIO_InitStruct;
	
	/**QUADSPI GPIO Configuration    
	PF8     	------> QUADSPI_BK1_IO0			** MOSI	Red
	PF9     	------> QUADSPI_BK1_IO1			** MISO Blue
	PF10     	------> QUADSPI_CLK					** QUADSPI_CLK (Direct on board pin header)
	PE7     	------> QUADSPI_BK2_IO0			** MOSI Black
	PE8     	------> QUADSPI_BK2_IO1			** MISO White
	PB10    	------> QUADSPI_BK1_NCS 		** QUADSPI_CS Orange
	*/
	
	// BANK1
	GPIO_InitStruct.Pin = LL_GPIO_PIN_8|LL_GPIO_PIN_9;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_10;
  LL_GPIO_Init(GPIOF, &GPIO_InitStruct);
	
	// BANK 2
	
	/*
	GPIO_InitStruct.Pin = LL_GPIO_PIN_7|LL_GPIO_PIN_8;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_10;
  LL_GPIO_Init(GPIOE, &GPIO_InitStruct);
	*/
	
	// CLK
	GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_9;
  LL_GPIO_Init(GPIOF, &GPIO_InitStruct);
	
	// CS
	GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_9;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	
	/* QUADSPI DMA Init */
	/* QUADSPI Init */
	
	// **** DMA Limited to 64K in a single transfer, using IT instead ********
	/*
	hdma_quadspi.Instance = DMA2_Stream2;
	hdma_quadspi.Init.Channel = DMA_CHANNEL_11;
	hdma_quadspi.Init.Direction = DMA_PERIPH_TO_MEMORY;
	hdma_quadspi.Init.PeriphInc = DMA_PINC_DISABLE;
	hdma_quadspi.Init.MemInc = DMA_MINC_ENABLE;
	hdma_quadspi.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
	hdma_quadspi.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
	hdma_quadspi.Init.Mode = DMA_NORMAL;
	hdma_quadspi.Init.Priority = DMA_PRIORITY_HIGH;
	hdma_quadspi.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
	
	HAL_DMA_Init(&hdma_quadspi);

	__HAL_LINKDMA(qspiHandle,hdma,hdma_quadspi);
	*/
}