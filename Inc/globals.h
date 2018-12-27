/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef __globals_H
#define __globals_H

#include "PreprocessorCore.h"
#include <stdint.h>
#include "atomic_ops.h"

#define LED_BLINK_FAST  100
#define LED_BLINK_SLOW  500
#define LED_BLINK_ERROR 2000

#define NULL 0
#define TRUE 1
#define FALSE 0

#define ARMV7M_DCACHE_LINESIZE 32  /* 32 bytes (8 words) */
#define ARMV7M_ICACHE_LINESIZE 32  /* 32 bytes (8 words) */
	
#define NVIC_PRIORITY_VERY_HIGH 0
#define NVIC_PRIORITY_HIGH 1
#define NVIC_PRIORITY_NORMAL 2
#define NVIC_PRIORITY_LOW 3

#define UserButton_Blue_Pin LL_GPIO_PIN_13
#define UserButton_Blue_GPIO_Port GPIOC

//#define ENABLE_SKULL

enum eLED
{
	Red_OnBoard = 0,
	Blue_OnBoard
};

void InitLED_GPIO(void);

#ifdef __cplusplus
NOINLINE extern "C" void memcpy32(uint32_t* __restrict dst, uint32_t const* __restrict src, uint32_t const length); // not size, only #of elements
NOINLINE extern "C" void memcpy32_mirrored(uint32_t* const __restrict dst, uint32_t const* const __restrict src, uint32_t const stride, uint32_t const numoflines);
NOINLINE extern "C" void memcpy8(uint8_t* __restrict dst, uint8_t const* __restrict src, uint32_t const length);
NOINLINE extern "C" void memcpy8_mirrored(uint8_t* const __restrict dst, uint8_t const* const __restrict src, uint32_t const stride, uint32_t const numoflines);
#else
NOINLINE extern void memcpy32(uint32_t* __restrict dst, uint32_t const* __restrict src, uint32_t const length); // not size, only #of elements
NOINLINE extern void memcpy32_mirrored(uint32_t* const __restrict dst, uint32_t const* const __restrict src, uint32_t const stride, uint32_t const numoflines);
NOINLINE extern void memcpy8(uint8_t* __restrict dst, uint8_t const* __restrict src, uint32_t const length);
NOINLINE extern void memcpy8_mirrored(uint8_t* const __restrict dst, uint8_t const* const __restrict src, uint32_t const stride, uint32_t const numoflines);
#endif

#ifdef __cplusplus
 extern "C" {
#endif 
extern uint32_t const millis(void);
#ifdef __cplusplus
}
#endif

void LED_On(enum eLED const eLEDSelect);
void LED_Off(enum eLED const eLEDSelect);
void LED_Blink(enum eLED const eLEDSelect);
void LED_BlinkWait(enum eLED const eLEDSelect, uint32_t const Period);
void LED_Blinking(enum eLED const eLEDSelect, uint32_t const Period);
void LED_SignalError();

#ifdef __cplusplus
static constexpr uint32_t const CLOCKSPEED = 260000000;	// default 216MHz, overclocked to 260Mhz (maximum stable working w/OLED, SRAM, USB etc at corresponding higher clocks aswell)

static constexpr uint32_t const UNLOADED = 0,
																PENDING = 1,
																LOADED = 2;

namespace Constants
{
	static constexpr float32_t const inverseUINT8 = 1.0f / 255.0f,
																	 inverseUINT16 = 1.0f / (float)(UINT16_MAX),
																	 inverse4BPP = 1.0f / 16.0f,
																	 nf255 = 255.0f,
																	 nf127 = 127.0f,
																	 nf16 = 16.0f,
																	 nfNegativePoint5 = -0.5f,
																	 nfPoint5 = 0.5f;
													
	static constexpr uint32_t const n16 = 16, n8 = 8, n255 = 255,
																	n33 = 33,
																	n1 = 1;
	
	static constexpr uint32_t const SATBIT_UINT32 = 32,
																	SATBIT_512 = 9,
																  SATBIT_256 = 8,
														      SATBIT_128 = 7,
														      SATBIT_64 = 6,
																  SATBIT_32 = 5,
																	SATBIT_16 = 4,
																	SATBIT_8 = 3,
																	SATBIT_4 = 2;
	
	static constexpr uint8_t const OPACITY_0 = 0,
																 OPACITY_12 = 0x1f,
																 OPACITY_25 = 0X3f,
																 OPACITY_50 = 0X7f,
																 OPACITY_75 = 0Xbf,
																 OPACITY_100 = 0xff;
	
	static constexpr float ConvTimeToFloat = 0.001f;
}
namespace OLED
{
	static constexpr uint32_t const SCREEN_WIDTH = 256, SCREEN_HEIGHT = 64, HALF_WIDTH = SCREEN_WIDTH >> 1, HALF_HEIGHT = SCREEN_HEIGHT >> 1;
	static constexpr uint32_t const Width_SATBITS = Constants::SATBIT_256, Height_SATBITS = Constants::SATBIT_64,
																	HalfWidth_SATBITS = Constants::SATBIT_128, HalfHeight_SATBITS = Constants::SATBIT_32;
	static constexpr float const INV_SCREEN_WIDTH = 1.0f / (float)SCREEN_WIDTH,
															 INV_SCREEN_HEIGHT = 1.0f / (float)SCREEN_HEIGHT,
															 INV_HALF_WIDTH = 1.0f / (float)HALF_WIDTH,
															 INV_HALF_HEIGHT = 1.0f / (float)HALF_HEIGHT,
															 INV_SCREEN_WIDTH_MINUS1 = 1.0f / (float)(SCREEN_WIDTH-1),
															 INV_SCREEN_HEIGHT_MINUS1 = 1.0f / (float)(SCREEN_HEIGHT-1),
															 INV_HALF_WIDTH_MINUS1 = 1.0f / (float)(HALF_WIDTH-1),
															 INV_HALF_HEIGHT_MINUS1 = 1.0f / (float)(HALF_HEIGHT-1);

}

template <typename T, std::size_t const N>
constexpr std::size_t const countof(T const (&)[N]) noexcept { return(N); }

#define SAFE_DELETE(p) if(p){delete p; p = nullptr;}
#define SAFE_DELETE_ARRAY(p) if(p){delete [] p; p = nullptr;}

template <typename T>
__attribute__((always_inline)) STATIC_INLINE void swap_pointer(T __restrict & __restrict pA, T __restrict & __restrict pB)
{ 
	T const __restrict pTemp(pA);
	pA = pB;
	pB = pTemp;
}
/*template <typename T> // conflicts with std::swap, using std::swap instead
__attribute__((always_inline)) STATIC_INLINE void swap(T& __restrict pA, T& __restrict pB)
{ 
	T const pTemp(pA);
	pA = pB;
	pB = pTemp;
}
*/
 __attribute__((always_inline)) STATIC_INLINE uint32_t const micros()
{
	static constexpr uint32_t const CLOCKSPEED_SCALED = CLOCKSPEED / 1e6;
	
	return( millis() * 1000 + 1000 - SysTick->VAL / CLOCKSPEED_SCALED );
}

#endif  // __cplusplus

#endif




