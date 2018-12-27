/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#include "Globals.h"
#include "stm32f7xx_ll_gpio.h"
#include "stm32f7xx_ll_utils.h"

#define Red_LED_Pin LL_GPIO_PIN_14
#define Red_LED_GPIO_Port GPIOB
#define Blue_LED_Pin  LL_GPIO_PIN_7
#define Blue_LED_GPIO_Port GPIOB

typedef struct sLED_Address
{
	GPIO_TypeDef * const Port;
	uint32_t const Pin;
	
	explicit sLED_Address( GPIO_TypeDef * thePort, uint32_t thePin ) : Port(thePort), Pin(thePin) 
	{}
	
} LED_Address;

STATIC_INLINE_PURE LED_Address const getLEDAddress(enum eLED const eLEDSelect)
{
	switch(eLEDSelect)
	{
		case Blue_OnBoard:
		{
			return(LED_Address(Blue_LED_GPIO_Port, Blue_LED_Pin));
		}
		default:
		case Red_OnBoard:
		{
			return(LED_Address(Red_LED_GPIO_Port, Red_LED_Pin));
		}
		/*case Red_Screen:
		{
			return(LED_Address(Red_Screen_LED_GPIO_Port, Red_Screen_LED_Pin));
		}*/
	}
}

void InitLED_GPIO(void)
{
	LL_GPIO_InitTypeDef GPIO_InitStruct;
	
	LL_GPIO_ResetOutputPin(Red_LED_GPIO_Port, Red_LED_Pin);
	LL_GPIO_ResetOutputPin(Blue_LED_GPIO_Port, Blue_LED_Pin);
	
	/**/
  GPIO_InitStruct.Pin = Red_LED_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(Red_LED_GPIO_Port, &GPIO_InitStruct);

	
	/**/
  GPIO_InitStruct.Pin = Blue_LED_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(Blue_LED_GPIO_Port, &GPIO_InitStruct);
}

/**
  * @brief  Turn-on LED2.
  * @param  None
  * @retval None
  */
void LED_On(enum eLED const eLEDSelect)
{
	LED_Address const resolvedLED( getLEDAddress(eLEDSelect) );
	
  /* Turn LED2 on */
  LL_GPIO_SetOutputPin(resolvedLED.Port, resolvedLED.Pin);
}

/**
  * @brief  Turn-off LED2.
  * @param  None
  * @retval None
  */
void LED_Off(enum eLED const eLEDSelect)
{
	LED_Address const resolvedLED( getLEDAddress(eLEDSelect) );
	
  /* Turn LED2 off */
  LL_GPIO_ResetOutputPin(resolvedLED.Port, resolvedLED.Pin);
}

void LED_Blink(enum eLED const eLEDSelect)
{
	LED_Address const resolvedLED( getLEDAddress(eLEDSelect) );
	
  /* Toggle IO */
  LL_GPIO_TogglePin(resolvedLED.Port, resolvedLED.Pin);
}

void LED_BlinkWait(enum eLED const eLEDSelect, uint32_t const Period)
{
	LED_Address const resolvedLED( getLEDAddress(eLEDSelect) );
	
  /* Toggle IO */
  LL_GPIO_TogglePin(resolvedLED.Port, resolvedLED.Pin);

  LL_mDelay(Period);
}
/**
  * @brief  Set LED2 to Blinking mode for an infinite loop (toggle period based on value provided as input parameter).
  * @param  Period : Period of time (in ms) between each toggling of LED
  *   This parameter can be user defined values. Pre-defined values used in that example are :
  *     @arg LED_BLINK_FAST : Fast Blinking
  *     @arg LED_BLINK_SLOW : Slow Blinking
  *     @arg LED_BLINK_ERROR : Error specific Blinking
  * @retval None
  */
void LED_Blinking(enum eLED const eLEDSelect, uint32_t const Period)
{
	LED_Address const resolvedLED( getLEDAddress(eLEDSelect) );
	
  /* Turn LED2 on */
  LL_GPIO_SetOutputPin(resolvedLED.Port, resolvedLED.Pin);

  /* Toggle IO in an infinite loop */
  while (1)
  {
    LL_GPIO_TogglePin(resolvedLED.Port, resolvedLED.Pin);

    LL_mDelay(Period);
  }
}

void LED_SignalError()
{
	#define Red_Screen_LED_Pin LL_GPIO_PIN_13			// Only repeated here for errors only 
	#define Red_Screen_LED_GPIO_Port GPIOD
	
	__BKPT(0);
	__DSB();
	__ISB();
	
	LED_Address const resolvedLED( getLEDAddress(Red_OnBoard) );
	
  /* Turn LED2 on */
  LL_GPIO_SetOutputPin(resolvedLED.Port, resolvedLED.Pin);
	LL_GPIO_SetOutputPin(Red_Screen_LED_GPIO_Port, Red_Screen_LED_Pin);
	
  /* Toggle IO in an infinite loop */
  while (1)
  {
    LL_GPIO_TogglePin(resolvedLED.Port, resolvedLED.Pin);
		LL_GPIO_TogglePin(Red_Screen_LED_GPIO_Port, Red_Screen_LED_Pin);
		
    LL_mDelay(LED_BLINK_ERROR);
  }
}

// mirroring verified working correctly
template<typename T>
STATIC_INLINE void memcpy_mirrored(T* const __restrict dst, T const* const __restrict src, uint32_t const stride, uint32_t const numoflines)
{
	int32_t x, y(numoflines - 1);
	
	// mirroriung reference:
	//return( ((uint8_t)*(descMask.Mask + (y * descMask.Stride) + (descMask.Stride - 1 - x))) );
		//else
	//return( ((uint8_t)*(descMask.Mask + (y * descMask.Stride) + x)) );
	
	do 
	{
		x = stride - 1;
		do 
		{
			// compiler optimizes this function pretty good, even moves multiplication outside inner loop
			*(dst + (y * stride + x)) = *(src + ((y * stride) + (stride - 1 - x)));
			
		} while ( --x >= 0 );
		
	} while ( --y >= 0 );
	
}
NOINLINE void memcpy32_mirrored(uint32_t* const __restrict dst, uint32_t const* const __restrict src, uint32_t const stride, uint32_t const numoflines)
{
	memcpy_mirrored<uint32_t>(dst, src, stride, numoflines);
}
NOINLINE void memcpy8_mirrored(uint8_t* const __restrict dst, uint8_t const* const __restrict src, uint32_t const stride, uint32_t const numoflines)
{
	memcpy_mirrored<uint8_t>(dst, src, stride, numoflines);
}
