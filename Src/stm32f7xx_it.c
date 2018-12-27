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
  * @file    stm32f7xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
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
#include "stm32f7xx.h"
#include "stm32f7xx_it.h"
#include "globals.h"
#include "stm32f7xx_ll_dma2d.h"

/* USER CODE BEGIN 0 */
extern void TransferComplete_Callback_DMA2D();
extern void TransferError_Callback_DMA2D();
extern void TransferComplete_Callback_DMA2_Stream3();
extern void TransferError_Callback_DMA2_Stream3();
extern void TransferComplete_Callback_DMA2_StreamMem2Mem(uint32_t const Stream);
extern void TransferError_Callback_DMA2_StreamMem2Mem(uint32_t const Stream);
extern void LPTimer_Callback();
extern void RNG_Callback();

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern JPEG_HandleTypeDef    JPEG_Handle;
extern QSPI_HandleTypeDef 	 hqspi;

extern volatile bool g_bToggleRender;

/* Private variables ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M7 Processor Interruption and Exception Handlers         */ 
/******************************************************************************/
static atomic_strict_uint32_t g_Ticks 
	__attribute__((section (".dtcm_atomic")))(0);

extern uint32_t const millis()
{
	return(g_Ticks);
}

/**
* @brief This function handles System tick timer.
*/
__ramfunc void SysTick_Handler(void)
{

  /* USER CODE BEGIN SysTick_IRQn 0 */
	++g_Ticks;
  /* USER CODE END SysTick_IRQn 0 */
  
	 HAL_IncTick();
	
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/**
* @brief This function handles Non maskable interrupt.
*/
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */
	LED_SignalError();
  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */

  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
* @brief This function handles Hard fault interrupt.
*/
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
	LED_SignalError();
  /* USER CODE END HardFault_IRQn 0 */
  
  /* USER CODE BEGIN HardFault_IRQn 1 */

  /* USER CODE END HardFault_IRQn 1 */
}

/**
* @brief This function handles Memory management fault.
*/
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
	LED_SignalError();
  /* USER CODE END MemoryManagement_IRQn 0 */

  /* USER CODE BEGIN MemoryManagement_IRQn 1 */

  /* USER CODE END MemoryManagement_IRQn 1 */
}

/**
* @brief This function handles Prefetch fault, memory access fault.
*/
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
	LED_SignalError();
  /* USER CODE END BusFault_IRQn 0 */
 
  /* USER CODE BEGIN BusFault_IRQn 1 */

  /* USER CODE END BusFault_IRQn 1 */
}

/**
* @brief This function handles Undefined instruction or illegal state.
*/
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
	LED_SignalError();
  /* USER CODE END UsageFault_IRQn 0 */

  /* USER CODE BEGIN UsageFault_IRQn 1 */

  /* USER CODE END UsageFault_IRQn 1 */
}

/******************************************************************************/
/* STM32F7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f7xx.s).                    */
/******************************************************************************/


__ramfunc void DMA2D_IRQHandler(void)
{
	if(0 != LL_DMA2D_IsActiveFlag_TC(DMA2D))
  {
    LL_DMA2D_ClearFlag_TC(DMA2D);
    TransferComplete_Callback_DMA2D();
  }
  else if(0 != LL_DMA2D_IsActiveFlag_TE(DMA2D) || 0 != LL_DMA2D_IsActiveFlag_CE(DMA2D))
  {
    TransferError_Callback_DMA2D();
  }
}


/**
* @brief This function handles DMA2 stream3 global interrupt.
*/
__ramfunc void DMA2_Stream3_IRQHandler(void) 	// SPI Double Buffer
{
  if(0 != LL_DMA_IsActiveFlag_TC3(DMA2))
  {
    LL_DMA_ClearFlag_TC3(DMA2);
    TransferComplete_Callback_DMA2_Stream3();
  }
  else if(0 != LL_DMA_IsActiveFlag_TE3(DMA2))
  {
    TransferError_Callback_DMA2_Stream3();
  }
}

void QUADSPI_IRQHandler(void)
{
  /* USER CODE BEGIN QUADSPI_IRQn 0 */

  /* USER CODE END QUADSPI_IRQn 0 */
  HAL_QSPI_IRQHandler(&hqspi);
  /* USER CODE BEGIN QUADSPI_IRQn 1 */

  /* USER CODE END QUADSPI_IRQn 1 */
}
// **** DMA Limited to 64K in a single transfer, using IT instead ********
/*
void DMA2_Stream2_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_quadspi);
}
*/

void JPEG_IRQHandler(void)
{
  HAL_JPEG_IRQHandler(&JPEG_Handle);
}

void DMA2_Stream0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(JPEG_Handle.hdmain);
}

void DMA2_Stream1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(JPEG_Handle.hdmaout);
}

void DMA2_Stream6_IRQHandler(void)
{
	if(0 != LL_DMA_IsActiveFlag_TC6(DMA2))
  {
    LL_DMA_ClearFlag_TC6(DMA2);
    TransferComplete_Callback_DMA2_StreamMem2Mem(LL_DMA_STREAM_6);
  }
  else if(0 != LL_DMA_IsActiveFlag_TE6(DMA2))
  {
    TransferError_Callback_DMA2_StreamMem2Mem(LL_DMA_STREAM_6);
  }
}

/**
* @brief This function handles DMA2 stream1 global interrupt.
*/
void DMA2_Stream7_IRQHandler(void)
{
	if(0 != LL_DMA_IsActiveFlag_TC7(DMA2))
  {
    LL_DMA_ClearFlag_TC7(DMA2);
    TransferComplete_Callback_DMA2_StreamMem2Mem(LL_DMA_STREAM_7);
  }
  else if(0 != LL_DMA_IsActiveFlag_TE7(DMA2))
  {
    TransferError_Callback_DMA2_StreamMem2Mem(LL_DMA_STREAM_7);
  }
}

#if (0 != USART_ENABLE)

extern void USARTComplete_DMA1(void);
void DMA1_Stream3_IRQHandler(void)
{
  if(0 != LL_DMA_IsActiveFlag_TC3(DMA1))
  {
    LL_DMA_ClearFlag_TC3(DMA1);
		USARTComplete_DMA1();
  }
  else if(0 != LL_DMA_IsActiveFlag_TE3(DMA1))
  {
   LED_Blinking(Red_OnBoard, 500);
  }
}

#endif
/**
* @brief This function handles EXTI line[15:10] interrupts.
*/
/*
void EXTI15_10_IRQHandler(void)
{
  if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_13) != RESET)
  {
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_13);
    // USER CODE BEGIN LL_EXTI_LINE_13
#ifdef ENABLE_SKULL
    g_bToggleRender = !g_bToggleRender;
#endif
  }
}
*/
/*
void SPI1_IRQHandler(void)
{
	LED_SignalError();
}
*/
/**
* @brief This function handles HASH and RNG global interrupts.
*/
__ramfunc void HASH_RNG_IRQHandler(void)
{
  /* USER CODE BEGIN HASH_RNG_IRQn 0 */
if ( LL_RNG_IsActiveFlag_CEIS(RNG) )
  {
		LL_RNG_ClearFlag_CEIS(RNG);
  }
	if ( LL_RNG_IsActiveFlag_SEIS(RNG) )
  {
		LL_RNG_ClearFlag_SEIS(RNG);
  }
	
  if(LL_RNG_IsActiveFlag_DRDY(RNG))
  {
    /* DRDY flag will be automatically cleared when reading 
       newly generated random number in DR register */

    /* Call function in charge of handling DR reading */
    RNG_Callback();
  }
  /* USER CODE END HASH_RNG_IRQn 0 */
  
  /* USER CODE BEGIN HASH_RNG_IRQn 1 */

  /* USER CODE END HASH_RNG_IRQn 1 */
}

/**
* @brief  This function handles LPTIM1 interrupts.
* @param  None
* @retval None
*/

__ramfunc void LPTIM1_IRQHandler(void)
{
  // Check whether compare match interrupt is pending 
  if(LL_LPTIM_IsActiveFlag_CMPM(LPTIM1))
  {
    // Clear the compare match interrupt flag 
    LL_LPTIM_ClearFLAG_CMPM(LPTIM1);
    
    // LPTIM1 Autoreload match interrupt processing
    LPTimer_Callback();
  }
}


/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
