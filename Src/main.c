/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#include "PreprocessorCore.h"

#include "stm32f7xx_ll_dma2d.h"

#include "globals.h"
#include "main.h"
#include "ext_sram.h"
#include "dma.h"
#include "rng.h"
#include "lptim.h"
#include "sdf_fram.h"
#include "spi.h"
#include "gpio.h"
#include "quadspi.h"
#include "jpeg.h"
#include "oled.h"
#include "world.h"
#include "AIBot.h"
#include "assemblerexternaliases.h"

#if( 0 != USART_ENABLE )
#include "usart.h"
#endif

#include "VoxMissile.h"

#include "debug.cpp"

NOINLINE static void LL_Init(void);
NOINLINE void SystemClock_Config(void);


// USE Target "VisualXTC"
// Release:  -Ofast      turn on LTO
// Compiler Options:                      
// -ffast-math -ffp-mode=fast -fvectorize -fno-exceptions -fomit-frame-pointer -munaligned-access -mimplicit-float -fno-math-errno -c -Wno-c++17-extensions     

// USE Target "VisualXTCDEBUG"
// Debug: -O1    turn off LTO
// Compiler Option:
// -O1 -fno-omit-frame-pointer -fno-exceptions -munaligned-access -ffp-mode=fast -ffast-math -fno-math-errno -c -g3 -Wno-c++17-extensions
// Linker Option:
// --bestdebug


STATIC_INLINE void RenderScene( uint32_t const tNow );
STATIC_INLINE void UpdateScene( uint32_t const tNow );
NOINLINE void PrepareScene(uint32_t const tNow);


NOINLINE static void MPU_Config_ReadOnly(void); // has to be done in main (after c runtime init)
NOINLINE static void CPU_CACHE_Enable(void); // has to be done in main (after c runtime init)

static RenderSync bRenderSync						// ****Private Access inside main.c Only, and below is public access for OLED interrupt***
	__attribute__((section (".dtcm_atomic")));				// important that this is .dtcm_atomic
																																	// and (below) is dtcm_const - makes the .dtcm_const section RO(read-Only)
namespace OLED 
{
RenderSync* const __restrict bbRenderSync 
		__attribute__((section (".dtcm_const")))(&bRenderSync);	// public, shuld only be accessed in oled.cpp
} // end namespace

int main(void) // imported by reset handler asm startup 2001db28
{		
	static constexpr uint32_t const MIN_FRAME_TIME = 16;
	static constexpr uint32_t const HEARTBEAT_LENGTH = 4;
	static constexpr uint32_t const HEARTBEAT_INTERVAL[HEARTBEAT_LENGTH] = { 400, 144, 248, 696 };

	static uint32_t tHeartbeatLast(0), indexHeartBeat(0);
	
	/* Re-config MPU to make ramfunc area readonly from this point on */
	MPU_Config_ReadOnly(); // scatter-loading completed before this
	
	/* Enable the CPU Cache */
  CPU_CACHE_Enable(); // should only be done after c-runtime is loaded (entry to main)

  /* Initialize all configured peripherals */
  MX_GPIO_Init(); // -- GPIO default state, should be first sets up analog low power gpio settings for all ports
									// GPIO specialization happens in each respective module
	LED_On(Red_OnBoard);
	__use_accurate_range_reduction();	// for C99 math sinf/cosf : only used internally in vector rotation (load time)
																		// or other places where accuracy is required. Otherwise use __sinf or __cosf during runtime / real-time loop
	MX_DMA_Init();
	
	QuadSPI_FRAM::Init();
	
	JPEGDecoder::Init();	// some reason because of HAL must be init here first
  
  MX_SPI1_Init();
	
	MX_LPTIM1_Init();
	LED_Off(Red_OnBoard);
	LL_SYSTICK_EnableIT();  // starts the 1ms tick interrupt
	MX_RNG_Init();
	
#if( 0 != USART_ENABLE )
	USART::InitUSART();
#endif

#ifndef PROGRAM_SDF_TO_FRAM
	QuadSPI_FRAM::MemoryMappedMode();
#else
	SDF_FRAM::ProgramSDF_FRAM();
#endif
	
	OLED::Init();
	DTCM::ScatterLoad();
	LED_On(Blue_OnBoard);

	world::Init(millis());
		
	PrepareScene(millis()); // important
	
  /* Infinite loop */
	
  while (1)
  {
		uint32_t const tNow = millis();

		if (tNow - tHeartbeatLast > HEARTBEAT_INTERVAL[indexHeartBeat])
		{
			LED_Blink(Blue_OnBoard);
			
			tHeartbeatLast = tNow;
			indexHeartBeat = (indexHeartBeat + 1) & (HEARTBEAT_LENGTH - 1);
		}
				
		if ( bRenderSync.State <= 0 ) // rendering frequency is limited by the lptimer interrupt, best 8ms, likely 16ms
		{
			bRenderSync.State = OLED::RenderSync::UNLOADED; // default state (rendering)
			
			// Rendering in lockstep synchronization with lp timer render interrupt, resulting in min frame time of 16ms (60 Hz ideal target)
			// for best low latency and rendering, further regulation of rendering execution period should NOT be implemented
			{		
#ifdef PROGRAM_SDF_TO_FRAM
				if ( QuadSPI_FRAM::QSPI_PROGRAMMINGSTATE_PENDING != QuadSPI_FRAM::GetProgrammingState() )
					RenderScene(tNow);
#else
				RenderScene(tNow);
#endif			
			}
		} // important this is else to skip updating scene when the just finished rendering pass executes
		else { // the goal is to update the scene without being on the same pass as rendering
					 // and to update the scene as close as possible to one pass before the render pass
					 // PENDING state cannot be used, timing to inaccurate due to demotion of millis systick timer
					 // LOADING is ok gets a good enough result (no weird skipping during camera pan)

			if ( LOADED == bRenderSync.State ) {
				UpdateScene(tNow); // always update scene when NOT on same iteration as rendering
				
#if( 0 != USART_ENABLE )
				USART::ProcessUSARTDoubleBuffer();
#endif
			}
			else { // STATE is PENDING // LP Timer Render 60Hz interrupt is either working or scheduled for work
				// enter low-power state and yield execution to hopefully the render lp timer interrupt
				__WFI();
			}
		}
				
		QuadSPI_FRAM::Update(tNow); // needs update?
  }
}

STATIC_INLINE void RenderScene( uint32_t const tNow )
{
	OLED::ClearFrameBuffers(tNow); // the ONLY place this should be called

	world::Render(tNow);

//	OLED::TestPixels(tNow);
	
	// LAST //
	OLED::Render(tNow);
	
	// leveraging parallel dma2d op ongoing begins //
}

STATIC_INLINE void UpdateScene( uint32_t const tNow )
{
	static constexpr uint32_t const ORIGIN_LOCATION_CENTER = 0,
																	ORIGIN_LOCATION_TOPLEFT = 1,
																	ORIGIN_LOCATION_TOP = 2,
																	ORIGIN_LOCATION_TOPRIGHT = 3,
																	ORIGIN_LOCATION_RIGHT = 4,
																	ORIGIN_LOCATION_BOTTOMRIGHT = 5,
																	ORIGIN_LOCATION_BOTTOM = 6,
																	ORIGIN_LOCATION_BOTTOMLEFT = 7,
																	ORIGIN_LOCATION_LEFT = 8;
	
	static constexpr uint32_t const MOVETIME_FAST = 2000, // ms
																	MOVETIME_SLOW = 40000,
																	HOLDTIME = world::TURN_INTERVAL * (world::NUM_AIBOTS - 1);
	
	static constexpr uint32_t const DELAY_NEW_MISSILE = 2500; // ms
	
	static char szTurnText[cAIBot::MAX_LENGTH_TURN_TEXT+1];
	
	static uint32_t CurOriginLocation(ORIGIN_LOCATION_CENTER),
									NextOriginLocation(ORIGIN_LOCATION_CENTER),
									tMoveTime(MOVETIME_FAST), tLastMoveCompleted(0), tStartCheckCantMove(0);
	
	static uint32_t tDelayMissileStart(0);
	
	static bool bMoveToggle(false), bMoveRandom(false);

	static bool bFirstMoveOnly(true);
	
	if ( !world::isDeferredInitComplete() ) {
		world::DeferredInit(tNow);
		
#if( 0 != USART_ENABLE )
		USART::DebugStartUp();
#endif
	}
	
	uint32_t const CurCameraState = world::getCameraState();
	if ( bFirstMoveOnly && UNLOADED == CurCameraState ) {
		
		if ( tNow - (tLastMoveCompleted) > HOLDTIME) {
		
			if (!bMoveRandom)
			{
				point2D_t const ptMoveToBotHome = world::getPendingTurnAIBot().getCurrentBuildLocation();
			//	world::getPendingTurnAIBot().getTurnText(szTurnText);
				//DebugMessage(szTurnText);
				if ( !world::moveTo(p2D_to_v2(ptMoveToBotHome), tMoveTime) ) {
					if ( 0 == tStartCheckCantMove )
						tStartCheckCantMove = millis();
				}
				else {
					tStartCheckCantMove = 0;
	#ifdef FIRST_MOVE_ONLY
					bFirstMoveOnly = false;
					tMoveTime = MOVETIME_SLOW;
	#endif
					bMoveRandom = true;
				}
				
				if ( 0 != tStartCheckCantMove ) {
					
					if ( tNow - tStartCheckCantMove > 35000 ) {
						
						bMoveRandom = true;
						tMoveTime = MOVETIME_SLOW;
					}
				}

			}
			else if ( nullptr == (VoxMissile const* __restrict)world::getActiveMissile() ) //if ( bMoveRandom )
			{
				
				if ( 0 == tDelayMissileStart )
					tDelayMissileStart = tNow;
					
				if (tNow - tDelayMissileStart > DELAY_NEW_MISSILE )
				{
					// Update current
					CurOriginLocation = NextOriginLocation;
					// get new random target unique location
					do
					{
						NextOriginLocation = RandomNumber(0, 8);
					} while( NextOriginLocation == CurOriginLocation );
					
					static constexpr uint32_t MISSILE_FOLLOW_UPDATE = 33; // ms
					
					VoxMissile const* __restrict pMissile(nullptr); // using a "known" downcast
																													// fortunately dynamic_cast is not required because it is known
																													// at compile time, reinterpret_cast works
					// set camera to move to new position
					switch(NextOriginLocation)
					{
						default:
						case ORIGIN_LOCATION_CENTER:
							pMissile = (VoxMissile const* __restrict)world::CreateMissile(tNow, world::getOrigin(), WORLD_CENTER);
							if ( nullptr != pMissile ) {
								world::follow( &pMissile->vLoc, MISSILE_FOLLOW_UPDATE );
								tDelayMissileStart = 0;
							}
							else
								world::moveTo(WORLD_CENTER, tMoveTime);
						break;
						case ORIGIN_LOCATION_TOPLEFT:
							pMissile = (VoxMissile const* __restrict)world::CreateMissile(tNow, world::getOrigin(), WORLD_TL);
							if ( nullptr != pMissile ) {
								world::follow( &pMissile->vLoc, MISSILE_FOLLOW_UPDATE );
								tDelayMissileStart = 0;
							}
							else
								world::moveTo(WORLD_TL, tMoveTime);
						break;
						case ORIGIN_LOCATION_TOP:
							pMissile = (VoxMissile const* __restrict)world::CreateMissile(tNow, world::getOrigin(), WORLD_TOP);
							if ( nullptr != pMissile ) {
								world::follow( &pMissile->vLoc, MISSILE_FOLLOW_UPDATE );
								tDelayMissileStart = 0;
							}
							else
								world::moveTo(WORLD_TOP, tMoveTime);
						break;
						case ORIGIN_LOCATION_TOPRIGHT:
							pMissile = (VoxMissile const* __restrict)world::CreateMissile(tNow, world::getOrigin(), WORLD_TR);
							if ( nullptr != pMissile ) {
								world::follow( &pMissile->vLoc, MISSILE_FOLLOW_UPDATE );
								tDelayMissileStart = 0;
							}
							else
								world::moveTo(WORLD_TR, tMoveTime);
						break;
						case ORIGIN_LOCATION_RIGHT:
							pMissile = (VoxMissile const* __restrict)world::CreateMissile(tNow, world::getOrigin(), WORLD_RIGHT);
							if ( nullptr != pMissile ) {
								world::follow( &pMissile->vLoc, MISSILE_FOLLOW_UPDATE );
								tDelayMissileStart = 0;
							}
							else
								world::moveTo(WORLD_RIGHT, tMoveTime);
						break;
						case ORIGIN_LOCATION_BOTTOMRIGHT:
							pMissile = (VoxMissile const* __restrict)world::CreateMissile(tNow, world::getOrigin(), WORLD_BR);
							if ( nullptr != pMissile ) {
								world::follow( &pMissile->vLoc, MISSILE_FOLLOW_UPDATE );
								tDelayMissileStart = 0;
							}
							else
								world::moveTo(WORLD_BR, tMoveTime);
						break;
						case ORIGIN_LOCATION_BOTTOM:
							pMissile = (VoxMissile const* __restrict)world::CreateMissile(tNow, world::getOrigin(), WORLD_BOTTOM);
							if ( nullptr != pMissile ) {
								world::follow( &pMissile->vLoc, MISSILE_FOLLOW_UPDATE );
								tDelayMissileStart = 0;
							}
							else
								world::moveTo(WORLD_BOTTOM, tMoveTime);
						break;
						case ORIGIN_LOCATION_BOTTOMLEFT:
							pMissile = (VoxMissile const* __restrict)world::CreateMissile(tNow, world::getOrigin(), WORLD_BL);
							if ( nullptr != pMissile ) {
								world::follow( &pMissile->vLoc, MISSILE_FOLLOW_UPDATE );
								tDelayMissileStart = 0;
							}
							else
								world::moveTo(WORLD_BL, tMoveTime);
						break;
						case ORIGIN_LOCATION_LEFT:
							pMissile = (VoxMissile const* __restrict)world::CreateMissile(tNow, world::getOrigin(), WORLD_LEFT);
							if ( nullptr != pMissile ) {
								world::follow( &pMissile->vLoc, MISSILE_FOLLOW_UPDATE );
								tDelayMissileStart = 0;
							}
							else
								world::moveTo(WORLD_LEFT, tMoveTime);
						break;
					}
				}
			}
			
		} //if new move to start
	} // camera state
	else if ( LOADED == CurCameraState )
	{
		if ( nullptr == (VoxMissile const* __restrict)world::getActiveMissile() )
		{
		tLastMoveCompleted = tNow;
#ifndef ENABLE_SKULL
		DebugMessage("( %d, %d )", int32::__floorf(world::getOrigin().x), int32::__floorf(world::getOrigin().y) );
#endif		
		}
	}
	
	world::Update(tNow);
}
NOINLINE void PrepareScene(uint32_t const tNow) {
	
	UpdateScene(tNow); // alwats update scene
	
	StartLPTimer();
}
/// * Interrupts *//
void LPTimer_Callback()
{
	if ( bRenderSync.State > 0 ) {	//  = work pending, PENDING = interuppt is working, UNLOADED = default state (rendering), MAIN_DMA_COMPLETED = signal rendering to continue
		
		if ( bRenderSync.tLastRenderCompleted > bRenderSync.tLastSendCompleted )
		{				
			if ( !xDMA2D::IsBusy_DMA2D() ) {
				
				bRenderSync.State = OLED::RenderSync::PENDING; // (interrupt is working)
				
				uint32_t const tNow = millis();
				/* SysTick_IRQn interrupt configuration */
				NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_NORMAL, 0)); // prevent premption for systick during critical framebuffer send operations
				
				OLED::SendFrameBuffer(tNow);
				
				/* SysTick_IRQn interrupt configuration */
				NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_VERY_HIGH, 0)); // restore, also improves timing as noted by profiling inside function
				
				bRenderSync.tLastSendCompleted = tNow;
			}
			//else // skip interrupt
			// state still equal to LOADED (work pending)			
		}
	}
}

void TransferComplete_Callback_DMA2_Stream3()
{	
	/* Disable DMA Tx Channel */
	LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_3);

	// CURRENTTARGETMEM0 = 0,
	// CURRENTTARGETMEM1 = 1;
	*OLED::bbOLEDBusy_DMA2[ OLED::getActiveStreamDoubleBufferIndex() ] = 0;		// Flag currently used target as free
}
void TransferError_Callback_DMA2_Stream3()
{
	LED_SignalError();
}
void TransferComplete_Callback_DMA2D()
{	
	xDMA2D::Interrupt_ISR();
}
void TransferError_Callback_DMA2D()
{
	LED_SignalError();
}
void TransferComplete_Callback_DMA2_StreamMem2Mem(uint32_t const Stream)
{
	DMAMem2MemComplete_DMA2_Stream(Stream);
}
void TransferError_Callback_DMA2_StreamMem2Mem(uint32_t const Stream)
{
	LED_SignalError();
}
void RNG_Callback()
{
	RNG_DataReady_Callback();
}
/// * Interrupts *//

NOINLINE static void LL_Init(void)
{
   NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  /* System interrupt init*/
  /* MemoryManagement_IRQn interrupt configuration */
  NVIC_SetPriority(MemoryManagement_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  /* BusFault_IRQn interrupt configuration */
  NVIC_SetPriority(BusFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  /* UsageFault_IRQn interrupt configuration */
  NVIC_SetPriority(UsageFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  /* SVCall_IRQn interrupt configuration */
  NVIC_SetPriority(SVCall_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  /* DebugMonitor_IRQn interrupt configuration */
  NVIC_SetPriority(DebugMonitor_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  /* PendSV_IRQn interrupt configuration */
  NVIC_SetPriority(PendSV_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
	/* SysTick_IRQn interrupt configuration */
  NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_VERY_HIGH, 0));
	
	// DMA2 Stream3 (OLED SPI) FrameBuffer Send Complete is VERYHIGH
	// Render Interrupt is HIGH can only be pre-empted by OLED SPI DMA Interrupt, which it may be waiting for in ToggleBuffers
	// Systick (millisecond timer) is NORMAL Don't need this to pre-empt render interrupt or OLED SPI DMA interrupts, only really matters is Update Loop in main() relies on accurate timing.
	// this does however make timing inaccurate for the render LPTimer interrupt, leaving systick timer as very high to allow precise timing
	// for now
	// Anything else is either NORMAL or LOW in priority except the major system interrupts
}

/** System Clock Configuration
*/
NOINLINE void SystemClock_Config(void)
{// Order of operations is very important here for correct bootup
	
	LL_RCC_HSE_Disable();
	
	/* Set FLASH latency */ 
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_7);

  /* Enable PWR clock */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

	LL_RCC_HSI_SetCalibTrimming(16); // default is 16Mhz for HSI

  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {}

	LL_PWR_EnableBkUpAccess(); // required do not change
	
	LL_RCC_LSE_SetDriveCapability(LL_RCC_LSEDRIVE_MEDIUMHIGH);

  LL_RCC_LSE_Enable();

  // Wait till LSE is ready
  while(LL_RCC_LSE_IsReady() != 1)
  {}
		
  /* Activation OverDrive Mode */
  LL_PWR_EnableOverDriveMode();
  while(LL_PWR_IsActiveFlag_OD() != 1)
  {
  };

  // Activation OverDrive Switching //have to look this up - not required?
  LL_PWR_EnableOverDriveSwitching();
  while(LL_PWR_IsActiveFlag_ODSW() != 1)
  {
  };
	
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, (CLOCKSPEED/1e6), LL_RCC_PLLP_DIV_2);

  // not using this for 48Mhz(see below) LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, (CLOCKSPEED/1e6), LL_RCC_PLLQ_DIV_9);

  LL_RCC_PLL_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL_IsReady() != 1)
  {}
	
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);	// AHB = 216Mhz or what is defined for CLOCKSPEED
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {}
	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
	
  LL_Init1msTick(CLOCKSPEED);

  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);

  LL_SetSystemCoreClock(CLOCKSPEED);
  
  /* Configure PLLSAI to enable 48M domain - this is a way more accurate 48MHz clock source
																						 based directly off of HSI and not the PLL
																						 so core clock frequency is independent of the final clock obtained here
    - Must keep SAME PLLSAI source (HSI) and PLLM factor (DIV8) 
    - Select PLLSAI1_N & PLLSAI1_P to have a frequency of 48MHz
        * PLLSAI_P output = (((HSI Freq / PLLM) * PLLSAI_N) / PLLSAI_P)
        *                  = (((16000000  /   8 ) *    96    ) /    4     ) */
  LL_RCC_PLLSAI_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, 96, LL_RCC_PLLSAIP_DIV_4);
  LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, 96, LL_RCC_PLLSAIP_DIV_4); // configures 48Mhz for RNG and USB

  /* Enable PLLSAI*/
  LL_RCC_PLLSAI_Enable();
  /*  Enable PLLSAI output mapped on 48MHz domain clock */
  LL_RCC_SetRNGClockSource(LL_RCC_RNG_CLKSOURCE_PLLSAI);
  /* Wait for PLLSA ready flag */
  while(LL_RCC_PLLSAI_IsReady() != 1) 
  {};

  /*  Write the peripherals independent clock configuration register :
      choose PLLSAI source as the 48 MHz clock is needed for the RNG 
      Linear Feedback Shift Register */
  LL_RCC_SetRNGClockSource(LL_RCC_RNG_CLKSOURCE_PLLSAI);
	
	// Setup Core peripheral Clock Sources
	LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM1_CLKSOURCE_LSE);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DTCMRAM|LL_AHB1_GRP1_PERIPH_DMA2D);
	LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_FMC);
}

// as per ST AN4667 D and I caches should be enabled, along with using DTCM & ITCM memories
// flash should be on the AXI bus range(0x08000000), NOT TCM bus range(0x00200000)
// single bank should be used instead of dual bank for optimal performance
// specifically, all of the above applies to the STM32F767 series
// so art + prefetch are disabled!
// well shit - not using the TCM bus range for Flash CAUSES VEENERS TO BE GENERATED FOR ALL RAMFUNCS IN ITCM
// so that adds alot of extra function call overhead for what is supposed to be fast ramfuncs!!!
// all due to moving from a branch from range AXI 0x08000000 to ITCM range 0x00000000
// vs                                         TCM 0x00200000 to ITCM range 0x00000000
// so I'm still using TCM to aviouod this and enabling the ART + ART PF
#define __FLASH_PREFETCH_BUFFER_ENABLE()  (FLASH->ACR |= FLASH_ACR_PRFTEN)
#define __FLASH_PREFETCH_BUFFER_DISABLE()   (FLASH->ACR &= (~FLASH_ACR_PRFTEN))

#define __FLASH_ART_ENABLE()  SET_BIT(FLASH->ACR, FLASH_ACR_ARTEN)
#define __FLASH_ART_DISABLE()   CLEAR_BIT(FLASH->ACR, FLASH_ACR_ARTEN)


NOINLINE static void InitCriticalRegisters() // imported by reset handler asm startup (SystemInitCustom)
{
	// Setup floating point rounding mode and flush denormals
	uint32_t FPSCR = __get_FPSCR();
	FPSCR &= (~(1 << 22));  // Round to nearest
	FPSCR &= (~(1 << 23));  // bits 22 & 23 must be clear
	FPSCR |= (1 << 24); // Enable denormal flush to zero, bit 24 must be set
	__set_FPSCR(FPSCR);
	
	__DSB();
	__ISB();
		
	// DTCM tweaks
	__IO uint32_t * CM7_DTCMCR = (uint32_t*)(0xE000EF94);
	*CM7_DTCMCR &= 0xFFFFFFFD; /* Disable read-modify-write, performance tweak good */
	__DSB();
	__ISB();
	
	// TCM Arbritration Control, higher priority given to software (dont know what demoted means)
	__IO uint32_t * CM7_AHBSCR = (uint32_t*)(0xE000EFA0);
	*CM7_AHBSCR &= 0xFFFFFFFD; /* Software aCESS DEMOTED (arithmetically larger numbers to represent lower priority. SO DEMOTED = HIGHER PRIORITY)*/
	__DSB();
	__ISB();
	
	__FLASH_ART_ENABLE();
	__DSB();
	
	__FLASH_PREFETCH_BUFFER_ENABLE();
}

/**
  * @brief  Configure the MPU attributes as Write Through for SRAM1/2.
  * @note   The Base Address is 0x20020000 since this memory interface is the AXI.
  *         The Configured Region Size is 512KB because the internal SRAM1/2 
  *         memory size is 384KB.
  * @param  None
  * @retval None
  */
NOINLINE static void MPU_Config_Internal(void) // imported by reset handler asm startup (SystemInitCustom)
{
	/// *** TCM MemoriesTightly coupled memories) 
// DTCM & ITCM are NOT cacheable, they are as fast as the L1 cache 
// ONLY the region for SRAM1 & SRAM2 are cacheable - and require cache coherency maintance, eg.) DTCM does not apply for cache coherency operations!
	/*
	For Cortex®-M7, TCMs memories always behave as Non-cacheable, Non-shared normal memories, irrespective
of the memory type attributes defined in the MPU for a memory region containing addresses held in the TCM.
Otherwise, the access permissions associated with an MPU region in the TCM address space are treated in the
same way as addresses outside the TCM address space.
	*/
	
	// Disable MPU 
  LL_MPU_Disable();
	
  LL_MPU_ConfigRegion(LL_MPU_REGION_NUMBER0, 0x00, DTCM::ITCM_RAMFUNC_ADDRESS, 							// at this point
         LL_MPU_REGION_SIZE_16KB | LL_MPU_REGION_PRIV_RW | LL_MPU_ACCESS_NOT_BUFFERABLE |		// has to be RW so the (ramcode) functions
         LL_MPU_ACCESS_CACHEABLE | LL_MPU_ACCESS_NOT_SHAREABLE | LL_MPU_TEX_LEVEL0 |				// can be scatter-loaded into ITCM
         LL_MPU_INSTRUCTION_ACCESS_ENABLE);
	
	// Config Stack/Heap Region DTCM 
	
	// (For remainder of DTCM + SRAM1 + SRAM2), SRAM1+SRAM2 utilizing write back as it is higher performance , DTCM is still write-through irrespective of this setting
	LL_MPU_ConfigRegion(LL_MPU_REGION_NUMBER1, 0x00, DTCM::DTCM_RWDATA_ADDRESS, 
         LL_MPU_REGION_SIZE_512KB | LL_MPU_REGION_FULL_ACCESS | LL_MPU_ACCESS_BUFFERABLE |
         LL_MPU_ACCESS_CACHEABLE | LL_MPU_ACCESS_NOT_SHAREABLE | LL_MPU_TEX_LEVEL1 |
         LL_MPU_INSTRUCTION_ACCESS_DISABLE);
	
	// Configure the MPU attributes as RW for STACK  at DTCM address
	// cacheable, bufferable, normal(RAM), not shareable, memory can be read and cached out of order
	// fastest setting if not using DMA, On (read) misses it updates the block in the
	// main memory and brings the block to the cache, size defined in scatter file and startup assembler file
	// STACK 0x1000 : 4KB OVERLAPPING REGION IN dtcm SPACE
	
  LL_MPU_ConfigRegion(LL_MPU_REGION_NUMBER2, 0x00, DTCM::DTCM_STACK_ADDRESS, //  DTCM is still write-through irrespective of this setting
         LL_MPU_REGION_SIZE_4KB | LL_MPU_REGION_PRIV_RW | LL_MPU_ACCESS_NOT_BUFFERABLE |
         LL_MPU_ACCESS_CACHEABLE | LL_MPU_ACCESS_NOT_SHAREABLE | LL_MPU_TEX_LEVEL0 |
         LL_MPU_INSTRUCTION_ACCESS_ENABLE);			 
	
				 
  // Enable MPU (any access not covered by any enabled region will cause a fault)
  LL_MPU_Enable(LL_MPU_CTRL_HFNMI_PRIVDEF);
	
	__DSB();
	__ISB();
}
NOINLINE static void MPU_Config_External(void) // imported by reset handler asm startup (SystemInitCustom)
{
	// Temporarily Disable MPU 
  LL_MPU_Disable();
	
	ExtSRAM::MPU_Config<LL_MPU_REGION_NUMBER4>();
	QuadSPI_FRAM::MPU_Config<LL_MPU_REGION_NUMBER5>(); // uses 2 region addresses
		
  // Enable MPU (any access not covered by any enabled region will cause a fault)
  LL_MPU_Enable(LL_MPU_CTRL_HFNMI_PRIVDEF);
	
	__DSB();
	__ISB();
}
NOINLINE static void MPU_Config_ReadOnly(void) // has to be done in main (after c runtime init)
{
	// Temporarily Disable MPU 
  LL_MPU_Disable();
	
	// Configure the MPU attributes as RO for RAMFUNC  at ITCM address
	// cacheable, bufferable, normal(RAM), not shareable, memory can be read and cached out of order
	// fastest setting if not using DMA, On (read) misses it updates the block in the
	// main memory and brings the block to the cache, size defined in scatter file
	// RAMFUNC 0x1000 : 4KB
	
  LL_MPU_ConfigRegion(LL_MPU_REGION_NUMBER0, 0x00, DTCM::ITCM_RAMFUNC_ADDRESS, 								// at this point
         LL_MPU_REGION_SIZE_16KB | LL_MPU_REGION_PRIV_RO_URO | LL_MPU_ACCESS_NOT_BUFFERABLE |			// reconfigure as RO
         LL_MPU_ACCESS_CACHEABLE | LL_MPU_ACCESS_NOT_SHAREABLE | LL_MPU_TEX_LEVEL0 |
         LL_MPU_INSTRUCTION_ACCESS_ENABLE);
	
	// Re-Enable MPU (any access not covered by any enabled region will cause a fault)
  LL_MPU_Enable(LL_MPU_CTRL_HFNMI_PRIVDEF);
	
	__DSB();
	__ISB();
}
/**
  * @brief  CPU L1-Cache enable.
  * @param  None
  * @retval None
  */
NOINLINE static void CPU_CACHE_Enable(void) // has to be done in main (after c runtime init)
{
  /* Invalidate I-Cache : ICIALLU register*/
  SCB_InvalidateICache();  
	
  /* Enable I-Cache */
  SCB_EnableICache();  
     
	__DSB();
	__ISB();
	
  /* Enable D-Cache */
  SCB_InvalidateDCache();
  SCB_EnableDCache();
	
	__DSB();
	__ISB();
}

#ifdef __cplusplus
 extern "C" {
#endif 

NOINLINE void SystemInitCustom(void) // imported into main startup asm 
{
	/* MCU Configuration----------------------------------------------------------*/
	InitCriticalRegisters();
	
	// Init MPU //
	MPU_Config_Internal(); // scatter-loading is pending at this point, will be finished upon entry to main()
	
	__DSB();
	__ISB();
	
	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
	LL_Init();

  /* Configure the system clock */
  SystemClock_Config();
	
	__DSB();
	__ISB();
	
	ExtSRAM::Init();
	
	__DSB();
	__ISB();
	
	MPU_Config_External(); // scatter-loading is pending at this point, will be finished upon entry to main()
	
	__DSB();
	__ISB();
}
#ifdef __cplusplus
}
#endif

NOINLINE void OnError_Handler(void)
{
  LED_SignalError();
}




