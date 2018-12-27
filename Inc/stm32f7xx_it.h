/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#ifndef __STM32F7xx_IT_H
#define __STM32F7xx_IT_H
#include "PreprocessorCore.h"

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "main.h"

__ramfunc void SysTick_Handler(void);
 void NMI_Handler(void);
 void HardFault_Handler(void);
 void MemManage_Handler(void);
 void BusFault_Handler(void);
 void UsageFault_Handler(void);
	 
__ramfunc void DMA2D_IRQHandler(void);
__ramfunc void DMA2_Stream3_IRQHandler(void);	// SPI Double Buffer

void DMA2_Stream0_IRQHandler(void);	// JPEG in
void DMA2_Stream1_IRQHandler(void);	// JPEG out
void DMA2_Stream6_IRQHandler(void);	// M2M 0
void DMA2_Stream7_IRQHandler(void);	// M2M 1
void DMA1_Stream3_IRQHandler(void); // USART3

void QUADSPI_IRQHandler(void);
void JPEG_IRQHandler(void);

//not used (DMA instead) void SPI1_IRQHandler(void);
__ramfunc void HASH_RNG_IRQHandler(void);
__ramfunc void LPTIM1_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F7xx_IT_H */

