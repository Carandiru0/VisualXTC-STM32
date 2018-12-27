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
#include "rng.h"
#include "globals.h"
#include "commonmath.h"
#include "stm32f7xx_ll_rng.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_utils.h"

#define DETERMINISTIC_KEY_SEED 0xC00C1001
#include "..\..\superrandom\superrandom.h"

/* RNG init function */
NOINLINE void MX_RNG_Init(void)
{
  /* Peripheral clock enable */
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_RNG);

  /* RNG interrupt Init */
  NVIC_SetPriority(HASH_RNG_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_LOW, 0));
  NVIC_EnableIRQ(HASH_RNG_IRQn);

	// very important //
	oRandom.ReadPosition = oRandom.ReadIndex = 0;
	oRandom.WritePosition = oRandom.WriteIndex = 1;
	
  LL_RNG_Enable(RNG);

	/* Enable RNG generation interrupt */
  LL_RNG_EnableIT(RNG);
	
	__DSB();
	__ISB();
	
	// Delay so pre-buffered with good true random numbers before initialization
	// 48Mhz = 20.8ns per random number
	// (SEED_BUFFERPOSITIONS * 2 Sides + 1) * 20.8ns = minimum delay
	
	// this is done in a way that sllows the values to propogate the buffers
	uint32_t Delay = 2;	// 2ms // just to be safe
	__IO uint32_t  tmp = SysTick->CTRL;  // Clear the COUNTFLAG first
  // Add this code to indicate that local variable is not used 
  ((void)tmp);

  // Add a period to guaranty minimum wait
  Delay++;

  while (Delay)
  {
		__WFI();	// wait for RNG interrupt
		
    if((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0U)
    {
      Delay--;
    }
		
		LL_RNG_EnableIT(RNG);	// must re-enable as the interrupt disables the interrupt
													// kinda like lock-step, normal operation is once a random number is "read/used"
													// the interrupt is renabled to replace the used random number with a new
													// random number in the buffer down the line (write side)
  }
	
	InitializeRandomNumberGenerators();
}  




