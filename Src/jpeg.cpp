/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#include "jpeg.h"
#include "globals.h"

#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_msp.h"
#include "stm32f7xx_hal_jpeg.h"

#include "JPEG\jpeg_utils.h"
#include "JPEG\decode_dma.h"
#include "quadspi.h"
#include "rng.h"
#include "DTCM_Reserve.h"

#include "debug.cpp"

#if (JPEG_RGB_FORMAT == JPEG_ARGB8888)
  #define BYTES_PER_PIXEL 4  /* Number of bytes in a pixel */
#elif (JPEG_RGB_FORMAT == JPEG_RGB888)
  #define BYTES_PER_PIXEL 3  /* Number of bytes in a pixel */
#elif (JPEG_RGB_FORMAT == JPEG_RGB565)
  #define BYTES_PER_PIXEL 2  /* Number of bytes in a pixel */
#elif (JPEG_RGB_FORMAT == JPEG_GRAYSCALE)
  #define BYTES_PER_PIXEL 1  /* Number of bytes in a pixel */
#else
  #error "unknown JPEG_RGB_FORMAT "
#endif /* JPEG_RGB_FORMAT */

JPEG_HandleTypeDef    JPEG_Handle;
DMA_HandleTypeDef   	hdmaIn;
DMA_HandleTypeDef   	hdmaOut;
	
static struct sJPEGCurrent
{
	static constexpr uint32_t const TIMEOUT = 30000;
	
	JPEG_ConfTypeDef       JPEG_Info;
	uint32_t							 tStartDecode;
	
	uint32_t								ID;
	
	sJPEGCurrent()
	: ID(0)
	{}
		
} oJPEGCurrent;

namespace JPEGDecoder
{
/* ############## */
// 8bpp,     16KB  
uint8_t _DecompressedBuffer[SDFConstants::SDF_DIMENSION*SDFConstants::SDF_DIMENSION]
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))							// used a lot, fast reads for main SDF method
	__attribute__((section (".bss")));
}

namespace JPEGDecoder
{
NOINLINE void Init()
{
	LL_AHB2_GRP1_EnableClock( LL_AHB2_GRP1_PERIPH_JPEG );
	
	// JPEG interrupt Init 
  NVIC_SetPriority(JPEG_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),NVIC_PRIORITY_NORMAL, 0));
  NVIC_EnableIRQ(JPEG_IRQn);
	
	/*##-1- JPEG Initialization ################################################*/   
  /* Init The JPEG Color Look Up Tables used for YCbCr to RGB conversion   */  
  // dont need to for grayscale JPEG_InitColorTables(); 
	
  JPEG_Handle.Instance = JPEG;
  HAL_JPEG_Init(&JPEG_Handle); 
}

	
uint32_t const Start_Decode(uint8_t const* const srcJPEG_MemoryBuffer, 
														uint32_t const sizeInBytesOfJPEG,
														bool const bOnFRAM)
{
	static bool bFirstInstance(true);
	
	//if ( HAL_JPEG_STATE_READY != GetState() ) {
	//	HAL_JPEG_Abort(&JPEG_Handle);
	//}
	
	// Return a not ready state for multiple JPEG decode queries if a
	// active query has not finished yet, the ID will be zero when
	// the owning query has Polled IsReady and a READY state is returned
	// the owning query must then use the decompressed buffer before
	// another query is started
	if ( 0 == oJPEGCurrent.ID && HAL_JPEG_STATE_READY == GetState() ) {
		
		if ( !bFirstInstance )
			HAL_JPEG_DisableHeaderParsing(&JPEG_Handle);	// if width, height, colorspace is the same for each jpeg then the header info
																										// can persist between jpeg decodes, all registers are still valid
		
		if ( bOnFRAM ) {

			uint32_t const uiStatus = QuadSPI_FRAM::MemoryMappedMode();
		
			switch( uiStatus )
			{
				case QuadSPI_FRAM::QSPI_OP_BUSY:
#ifdef _DEBUG_OUT_OLED
					DebugMessage( "FRAM MemorxMapped Busy  =%d", QuadSPI_FRAM::GetHALQSPIErrorCode());
#endif
					return(false);
				case QuadSPI_FRAM::QSPI_OP_ERROR:
#ifdef _DEBUG_OUT_OLED
					DebugMessage( "FRAM MemorxMapped Error  =%d", QuadSPI_FRAM::GetHALQSPIErrorCode());
#endif
					return(false);
				default: // OK
					break;
			}

		}

		SCB_CleanDCache_by_Addr((uint32_t*)srcJPEG_MemoryBuffer, sizeInBytesOfJPEG);
		
		/*##-3- JPEG decoding with DMA (Not Blocking ) Method ################*/
		JPEG_Decode_DMA(&JPEG_Handle, srcJPEG_MemoryBuffer, sizeInBytesOfJPEG, JPEGDecoder::getDecompressionBuffer());

		//bFirstInstance = false;
		uint32_t const tNow = millis();
		oJPEGCurrent.ID = RandomNumber(1, UINT32_MAX >> 4) + tNow;
		oJPEGCurrent.tStartDecode = tNow;
		
		return(oJPEGCurrent.ID);
	}
	return(0);
}
int32_t const IsReady( uint32_t const idJPEG)
{
	uint32_t JpegProcessing_End(0);
	
	if (idJPEG != oJPEGCurrent.ID)
		return(NOT_READY);
	
	JpegProcessing_End = JPEG_OutputHandler(&JPEG_Handle);
	
	switch(JpegProcessing_End)
	{
		case READY:
			oJPEGCurrent.ID = 0; // Reset
			return(READY);
		default:
		case NOT_READY:
			if ( millis() - oJPEGCurrent.tStartDecode > oJPEGCurrent.TIMEOUT ) {
				HAL_JPEG_Abort(&JPEG_Handle);
				return(TIMED_OUT);
			}
	}
	return(NOT_READY);
}
uint32_t const getCurrentMCUBlockIndex()
{
	return(getCur_MCUBlockIndex());
}
uint32_t const getCurrentMCUTotalNb()
{
	return(getCur_MCUTotalNb());
}


void GetInfo( JPEG_ConfTypeDef const*& pInfo )
{
	/*##-5- Get JPEG Info  ###############################################*/
  HAL_JPEG_GetInfo(&JPEG_Handle, &oJPEGCurrent.JPEG_Info);
	
	pInfo = &oJPEGCurrent.JPEG_Info;
}

uint32_t const GetState()
{
	return(HAL_JPEG_GetState(&JPEG_Handle));
}

uint32_t const GetError()
{
	return(HAL_JPEG_GetError(&JPEG_Handle));
}

} // end namespace

