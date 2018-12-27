/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef PREPROCESSOR_CORE
//#define NDEBUG defined globally in target c/c++ settings
#define ARM_MATH_CM7 1
#define __DSP_PRESENT 1
#define __ARM_PCS_VFP 1
#define __ARM_FP_FAST 1
#define __ARM_FEATURE_DSP 1
#define __ARM_FEATURE_SIMD32 1
#define __ARM_FEATURE_UNALIGNED 1
#define __ARM_FEATURE_QBIT 1
#define __ARM_FEATURE_SAT 1
#define __ARM_FEATURE_FMA 1
#define __ARM_FEATURE_CLZ 1
#define __USE_C99_MATH 1
#define __FP_FAST_FMAF 1

#include <stdint.h>
#define UINT32_MAX           4294967295u
#define UINT8_MAX            255

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION <= 6010050)
// a pragma disable warning that actually disables the warning!
#pragma diag_suppress=815
#pragma diag_suppress=191
#pragma diag_suppress=1299
#pragma diag_suppress=3108

#endif

#ifdef __cplusplus
 extern "C" {
#endif 

#define SHOW_FRAMETIMING
#define _DEBUG_OUT_OLED
//#define RENDER_BLOOMHDRSOURCE
//#define RENDER_DEPTH
//#define MEASURE_FRAMEBUFFER_UTILIZATION
//#define _DEBUG_OUT_SWO		// Serial Wire Output in parallel with OLED ouput
//#define FIRST_MOVE_ONLY
//#define DEBUG_FLAT_GROUND
//#define DEBUG_NORENDER_STRUCTURES
#define VOX_DEBUG_ENABLED
//#define VOX_FRAM_FORCE_REPROGRAMMING
#define USART_ENABLE 1 // too fucking noisy
	 
// **** The below defines include the entire FRAM INCBIN file when needed
// defined only if currently programming FRAM, FRAM Loading
//#define QSPI_PROGRAM_SDF_TO_FRAM

#ifdef QSPI_PROGRAM_SDF_TO_FRAM
	 
	 #define QSPI_AUTOSLEEP_FRAM_ENABLED 0
	 // defined only if verifying and not programming	 
	 #define QSPI_VERIFY_SDF_FRAM_ONLY
#else
	 
	 #define QSPI_AUTOSLEEP_FRAM_ENABLED 0
#endif
	 
#define QSPI_AUTOSLEEP_FRAM QSPI_AUTOSLEEP_FRAM_ENABLED

#if QSPI_AUTOSLEEP_FRAM

// defined to show transitions 

///#define QSPI_AUTOSLEEP_SLEEPWAKE_STATE_OLED

#endif

// defined to show/enable any debug QSPI ERROR OLED messages
#define QSPI_ERROR_OLED

// ****
	 
#include "stm32f7xx.h"
#include "stm32f7xx_ll_cortex.h"
#include "stm32f7xx_ll_system.h"
#include "stm32f767xx.h"
#include <math.h>
#ifdef fmaf
#undef fmaf
#define fmaf __fma
#endif
#include <arm_math.h>
#include <arm_acle.h>
#define __DO_NOT_LINK_PROMISE_WITH_ASSERT
#include <assert.h>
	
	/// ##### __ramfunc's if defined in header file should never be static or inline!!!! ##### ///
#define STATIC_INLINE __STATIC_INLINE
#define STATIC_INLINE_PURE __attribute__((const)) __STATIC_INLINE
#define __ramfunc __attribute__((noinline, section(".ramfunc")))
#define NOINLINE __attribute__((noinline))
#define ZI_BSS  __attribute__((section(".bss")))

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define Heap_Size 0x8000		// Must match startup.s Heap_Size !!!!

//#include <stdint.h>
//#include <arm_acle.h>

#ifdef __cplusplus
}
#endif

#endif /*PREPROCESSOR_CORE*/


