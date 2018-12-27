/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
/**
  ******************************************************************************
  * File Name          : dma.c
  * Description        : This file provides code for the configuration
  *                      of all the requested memory to memory DMA transfers.
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2017 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "dma.h"
#include "globals.h"
#include "stm32f7xx_ll_dma.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_spi.h"

/* USER CODE BEGIN 0 */
static ZI_BSS Mem2Mem pDMA2_Mem2Mem0;
static ZI_BSS Mem2Mem pDMA2_Mem2Mem1;

void sMem2Mem::operator= ( sMem2Mem && in )
{
	//unconst
	(*((uint8_t const **)(&ConstData))) = in.ConstData;	
	(*((uint8_t const **)(&in.ConstData))) = nullptr;
	(*((uint8_t**)(&SRAMData))) = in.SRAMData;
	(*((uint8_t**)(&in.SRAMData))) = nullptr;
	
	(*((uint32_t*)(&SizeInWords))) = in.SizeInWords; (*((uint32_t*)(&in.SizeInWords))) = 0;
	(*((volatile bool const**)(&bbFinished))) = in.bbFinished; (*((volatile bool const**)(&in.bbFinished))) = nullptr;
}

void InitM2M( Mem2Mem * const Config, 
							volatile bool* const pbFinished,
							uint8_t const * const pConstData,
							uint8_t* const pSRAMData,
							uint32_t const uiSizeInWords )
{
	*Config = Mem2Mem(pbFinished, pConstData, pSRAMData, uiSizeInWords);
}
	
void PrepareM2MStream(uint32_t const Stream)
{
	/* Configure DMA request MEMTOMEM_DMA2_Stream0 */

	/* Select channel */
	LL_DMA_SetChannelSelection(DMA2, Stream, LL_DMA_CHANNEL_0);

	/* Set transfer direction */
	LL_DMA_SetDataTransferDirection(DMA2, Stream, LL_DMA_DIRECTION_MEMORY_TO_MEMORY);

	/* Set priority level */
	LL_DMA_SetStreamPriorityLevel(DMA2, Stream, LL_DMA_PRIORITY_HIGH);

	/* Set DMA mode */
	LL_DMA_SetMode(DMA2, Stream, LL_DMA_MODE_NORMAL);

	/* Set peripheral increment mode */
	LL_DMA_SetPeriphIncMode(DMA2, Stream, LL_DMA_PERIPH_INCREMENT);

	/* Set memory increment mode */
	LL_DMA_SetMemoryIncMode(DMA2, Stream, LL_DMA_MEMORY_INCREMENT);

	/* Set peripheral data width */
	LL_DMA_SetPeriphSize(DMA2, Stream, LL_DMA_PDATAALIGN_BYTE);

	/* Set memory data width */
	LL_DMA_SetMemorySize(DMA2, Stream, LL_DMA_MDATAALIGN_BYTE);

	/* Enable FIFO mode */
	LL_DMA_EnableFifoMode(DMA2, Stream);

	/* Set FIFO threshold */
	LL_DMA_SetFIFOThreshold(DMA2, Stream, LL_DMA_FIFOTHRESHOLD_FULL);

	/* Set memory burst size */
	LL_DMA_SetMemoryBurstxfer(DMA2, Stream, LL_DMA_MBURST_SINGLE);

	/* Set peripheral burst size */
	LL_DMA_SetPeriphBurstxfer(DMA2, Stream, LL_DMA_PBURST_SINGLE);
	
	LL_DMA_EnableIT_TC(DMA1, Stream);
	LL_DMA_EnableIT_TE(DMA1, Stream);
}

void DeferredCopyMemoryToMemory( Mem2Mem & Config )
{
	/* Destination increment mode: Increment mode Enable
   *  - Source data alignment:      Word alignment
   *  - Destination data alignment: Word alignment
   *  - Number of data to transfer: BUFFER_SIZE
   *  - DMA peripheral request:     No request
   *  - Transfer priority level:    High priority
  */
RetryDMA:	
	if ( nullptr == pDMA2_Mem2Mem0.bbFinished || true == *(pDMA2_Mem2Mem0.bbFinished) )
	{
		pDMA2_Mem2Mem0 = (sMem2Mem &&)Config;
		*(pDMA2_Mem2Mem0.bbFinished) = false;
		
		PrepareM2MStream(LL_DMA_STREAM_6);
		LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_6, pDMA2_Mem2Mem0.SizeInWords);
		LL_DMA_SetM2MSrcAddress(DMA2, LL_DMA_STREAM_6, (uint32_t)pDMA2_Mem2Mem0.ConstData);
		LL_DMA_SetM2MDstAddress(DMA2, LL_DMA_STREAM_6, (uint32_t)pDMA2_Mem2Mem0.SRAMData);
		
		/* Start the DMA transfer Flash to Memory */
		LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_6);
	}
	else if ( nullptr == pDMA2_Mem2Mem1.bbFinished || true == *(pDMA2_Mem2Mem1.bbFinished) )
	{
		pDMA2_Mem2Mem1 = (sMem2Mem &&)Config;
		*(pDMA2_Mem2Mem1.bbFinished) = false;
		
		PrepareM2MStream(LL_DMA_STREAM_7);
		LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_7, pDMA2_Mem2Mem1.SizeInWords);
		LL_DMA_SetM2MSrcAddress(DMA2, LL_DMA_STREAM_7, (uint32_t)pDMA2_Mem2Mem1.ConstData);
		LL_DMA_SetM2MDstAddress(DMA2, LL_DMA_STREAM_7, (uint32_t)pDMA2_Mem2Mem1.SRAMData);
		
		/* Start the DMA transfer Flash to Memory */
		LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_7);
	}
	else {
		goto RetryDMA;
		//LED_SignalError();
	}
}

/*----------------------------------------------------------------------------*/
/* Configure DMA                                                              */
/*----------------------------------------------------------------------------*/

/** 
  * Enable DMA controller clock
  */
NOINLINE void MX_DMA_Init(void) 
{
  /* Init with LL driver */
  /* DMA controller clock enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);  ///// ***** ONLY enables DMA2, no DMA1 ********* ////

  /* DMA interrupt init */
	
	// Double Buffer DMA to SPI Periphral
  /* DMA2_Stream3_IRQn interrupt configuration */
  NVIC_SetPriority(DMA2_Stream3_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_VERY_HIGH, 0));
  NVIC_EnableIRQ(DMA2_Stream3_IRQn);

	 /* DMA interrupt init */
	
	// **** DMA Limited to 64K in a single transfer, using IT instead ********
	/*
	// QUADSPI
	NVIC_SetPriority(DMA2_Stream2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_NORMAL, 0));
  NVIC_EnableIRQ(DMA2_Stream2_IRQn);
	*/
	
	// JPEG Streams
  /* DMA2_Stream0_IRQn interrupt configuration */
  NVIC_SetPriority(DMA2_Stream0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_NORMAL, 0));
  NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  NVIC_SetPriority(DMA2_Stream1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_NORMAL, 0));
  NVIC_EnableIRQ(DMA2_Stream1_IRQn);
	
	// Mem2Mem Streams
	/* DMA2_Stream0_IRQn interrupt configuration */
  NVIC_SetPriority(DMA2_Stream6_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_NORMAL, 0));
  NVIC_EnableIRQ(DMA2_Stream6_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  NVIC_SetPriority(DMA2_Stream7_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_NORMAL, 0));
  NVIC_EnableIRQ(DMA2_Stream7_IRQn);
	
	/* SPI3 DMA Init */

	/* SPI3_TX Init */
	LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_3, LL_DMA_CHANNEL_3);
	LL_DMA_ConfigTransfer(DMA2, LL_DMA_STREAM_3,  LL_DMA_DIRECTION_MEMORY_TO_PERIPH |
																								LL_DMA_PRIORITY_VERYHIGH              |  // highest priority for dma
																								LL_DMA_MODE_CIRCULAR     		      |
																								LL_DMA_DOUBLEBUFFER_MODE_ENABLE   |
																								LL_DMA_PERIPH_NOINCREMENT         |
																								LL_DMA_MEMORY_INCREMENT         |
																								LL_DMA_PDATAALIGN_BYTE        |
																								LL_DMA_MDATAALIGN_WORD);
	LL_DMA_SetPeriphAddress(DMA2, LL_DMA_STREAM_3, LL_SPI_DMA_GetRegAddr(SPI1));
	
	LL_DMA_DisableFifoMode(DMA2, LL_DMA_STREAM_3); // Uses Direct Mode, which does not cross the AHB matrix,
																								 // instead uses direct path to perphial with no contention
	/*
	LL_DMA_EnableFifoMode(DMA2, LL_DMA_STREAM_3);

  LL_DMA_SetFIFOThreshold(DMA2, LL_DMA_STREAM_3, LL_DMA_FIFOTHRESHOLD_3_4);

  LL_DMA_SetMemoryBurstxfer(DMA2, LL_DMA_STREAM_3, LL_DMA_MBURST_SINGLE);

  LL_DMA_SetPeriphBurstxfer(DMA2, LL_DMA_STREAM_3, LL_DMA_PBURST_SINGLE);
	*/
	LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_3); // Enable Transfer complete interrupt.
	//LL_DMA_EnableIT_TE(DMA2, LL_DMA_STREAM_3); // Enable Transfer error interrupt.
}

void DMAMem2MemComplete_DMA2_Stream(uint32_t const Stream)
{
	switch(Stream)
	{
		case LL_DMA_STREAM_6:
			if (nullptr != pDMA2_Mem2Mem0.bbFinished) {
				/* Disable DMA2 Tx Stream */
				LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_6);
				*(pDMA2_Mem2Mem0.bbFinished) = true;
			}
			break;
		case LL_DMA_STREAM_7:
			if (nullptr != pDMA2_Mem2Mem1.bbFinished) {
				/* Disable DMA2 Tx Stream */
				LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_7);
				*(pDMA2_Mem2Mem1.bbFinished) = true;
			}
			break;
	}
}
/* USER CODE END 0 */


