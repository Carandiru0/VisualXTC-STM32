/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#ifndef __MAIN_H
#define __MAIN_H
#include "PreprocessorCore.h"

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_jpeg.h"

#include "stm32f7xx_ll_lptim.h"
#include "stm32f7xx_ll_dma.h"
#include "stm32f7xx_ll_rcc.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_exti.h"
#include "stm32f7xx_ll_utils.h"
#include "stm32f7xx_ll_pwr.h"
#include "stm32f7xx_ll_rng.h"
#include "stm32f7xx_ll_spi.h"
#include "stm32f7xx_ll_gpio.h"


#ifndef NVIC_PRIORITYGROUP_0
#define NVIC_PRIORITYGROUP_0         ((uint32_t)0x00000007U) /*!< 0 bit  for pre-emption priority,
																														4 bits for subpriority */
#endif

#define NVIC_PRIORITYGROUP_1         ((uint32_t)0x00000006U) /*!< 1 bit  for pre-emption priority,
                                                                 3 bits for subpriority */
#define NVIC_PRIORITYGROUP_2         ((uint32_t)0x00000005U) /*!< 2 bits for pre-emption priority,
                                                                 2 bits for subpriority */
#define NVIC_PRIORITYGROUP_3         ((uint32_t)0x00000004U) /*!< 3 bits for pre-emption priority,
                                                                 1 bit  for subpriority */
#define NVIC_PRIORITYGROUP_4         ((uint32_t)0x00000003U) /*!< 4 bits for pre-emption priority,
                                                                 0 bit  for subpriority */
																														

#ifdef __cplusplus
 extern "C" {
#endif
	 
void OnError_Handler(void);
	 
#ifdef __cplusplus
}
#endif


#endif /* __MAIN_H */

