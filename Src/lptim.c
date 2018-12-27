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
  * File Name          : LPTIM.c
  * Description        : This file provides code for the configuration
  *                      of the LPTIM instances.
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
#include "lptim.h"
#include "globals.h"
#include "stm32f7xx_ll_lptim.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_gpio.h"

/* USER CODE BEGIN 0 */

/* LPTIM1 init function */
NOINLINE void MX_LPTIM1_Init(void)
{	
  LL_GPIO_InitTypeDef GPIO_InitStruct;
	
  /* Peripheral clock enable */
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPTIM1); 
  
	// DMA2 Stream3 (OLED SPI) FrameBuffer Send Complete is VERYHIGH
	// Render Interrupt is HIGH can only be pre-empted by OLED SPI DMA Interrupt, which it may be waiting for in ToggleBuffers
	// Systick (millisecond timer) is NORMAL Don't nbed this to pre-empt render interrupt or OLED SPI DMA interrupts, only really matters is Update Loop in main() relies on accurate timing.
	// Anything else is either NORMAL or LOW in priority except the major system interrupts
	NVIC_SetPriority(LPTIM1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_HIGH, 0));
  NVIC_EnableIRQ(LPTIM1_IRQn);
		
	LL_LPTIM_SetClockSource(LPTIM1, LL_LPTIM_CLK_SOURCE_INTERNAL);

  LL_LPTIM_SetPrescaler(LPTIM1, LL_LPTIM_PRESCALER_DIV4);

  LL_LPTIM_SetPolarity(LPTIM1, LL_LPTIM_OUTPUT_POLARITY_REGULAR);

  LL_LPTIM_SetUpdateMode(LPTIM1, LL_LPTIM_UPDATE_MODE_IMMEDIATE);

  LL_LPTIM_SetCounterMode(LPTIM1, LL_LPTIM_COUNTER_MODE_INTERNAL);

	//LL_LPTIM_TrigSw(LPTIM1);
	
	LL_LPTIM_Enable(LPTIM1);
	
	// FrequencyOutput = 32768 / Prescaler / (AutoReload + 1)
	// 60.235Hz = 32768 / 16 / (33 + 1) ~off by 0.2352
	// 59.795Hz = 32768 / 4 / (136 + 1) ~off by 0.2044
	// AutoReload = 136, Frequency = 59.79Hz | 16.72ms for display timer rerndering interrupt
	// AutoReload = 67, Frequency = 120.47Hz | 8.30ms for display timer rerndering interrupt - Nyquist Sampling Therom states sampling should be 2x the desired frequency
	//																																												 otherwise a skipped interrupt may be visible
	//																																												 working, the realtime render loop still limits the actual rendering time to 16 ms
	//																																												 but this will prevent "drift" and missed rendering frames
	//																																												 that would push the actual visible rendering time poosibly past 16ms
	static constexpr uint32_t const AUTORELOAD_LPTIMER = 67;
	LL_LPTIM_SetAutoReload(LPTIM1, AUTORELOAD_LPTIMER);
	LL_LPTIM_SetCompare(LPTIM1, 0);	
	
		
	// Greater than 160Hz reccommended for LED NO Flicker (Healthy LED)
/*
	The counter clock is LSE (32.768 KHz), Autoreload equal to 99 so the output
	frequency (FrequencyOutput) will be equal to 327.680.

  FrequencyOutput = Counter Clock Frequency / Prescaler / / (Autoreload + 1)
                  = 32768 / 2 / 100
                  = 163.840 Hz | 6.1 ms
									
	Pulse value equal to 49 and the duty cycle (DutyCycle) is computed as follow:

  DutyCycle = 1 - ((PulseValue + 1) / (Autoreload + 1))
  DutyCycle = 50%
*/
	//LL_LPTIM_SetAutoReload(LPTIM1, 100 - 1);
	//LL_LPTIM_SetCompare(LPTIM1, 50);	// PWM Initially OFF
}

/* USER CODE END 0 */
/* USER CODE BEGIN 1 */
void StartLPTimer(void)
{
	/* Start the LPTIM counter in continuous mode */
	LL_LPTIM_StartCounter(LPTIM1, LL_LPTIM_OPERATING_MODE_CONTINUOUS);
	
	/* Clear LPTIM1 compare match flag */
  LL_LPTIM_ClearFLAG_CMPM(LPTIM1);
  
  /* Enable the compare match Interrupt */
	LL_LPTIM_EnableIT_CMPM(LPTIM1);
	
	/*LL_LPTIM_Disable(LPTIM1);
	
	LL_LPTIM_TrigSw(LPTIM1);  // Start counter, must be called while disabled
	
	LL_LPTIM_Enable(LPTIM1);
	*/
}

/* USER CODE END 1 */

