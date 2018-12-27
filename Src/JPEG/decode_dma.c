/**
  ******************************************************************************
  * @file    JPEG/JPEG_DecodingFromFLASH_DMA/Src/decode_dma.c
  * @author  MCD Application Team
  * @brief   This file provides routines for JPEG decoding from memory with DMA method.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
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
#include "JPEG\decode_dma.h"
#include "JPEG\jpeg_utils.h"

/** @addtogroup STM32F7xx_HAL_Examples
  * @{
  */

/** @addtogroup JPEG_DecodingFromFLASH_DMA
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  uint8_t State;  
  uint8_t* const DataBuffer;
  uint32_t DataBufferSize;

}JPEG_Data_BufferTypeDef;

/* Private define ------------------------------------------------------------*/

#define CHUNK_SIZE_IN  ((uint32_t)(16384)) 
// 1 MCU (8x8) = 64 bytes, 128 width = 16 MCUs  x  128 height = 16 MCUs,  total of 256 MCUs x 64 bytes = 16 KB
#define CHUNK_SIZE_OUT ((uint32_t)(8192)) // seems there is a problem with memory mapped fram if I lower these values

#define JPEG_BUFFER_EMPTY 0
#define JPEG_BUFFER_FULL  1

#define NB_OUTPUT_DATA_BUFFERS 2

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static struct sDecoderState
{
	JPEG_YCbCrToRGB_Convert_Function pConvert_Function;

	uint8_t MCU_Data_OutBuffers[NB_OUTPUT_DATA_BUFFERS][CHUNK_SIZE_OUT];
	
	JPEG_Data_BufferTypeDef Jpeg_OUT_BufferTab[NB_OUTPUT_DATA_BUFFERS];

	uint32_t MCU_TotalNb;
	uint32_t MCU_BlockIndex;
	uint32_t Jpeg_HWDecodingEnd;

	uint32_t JPEG_OUT_Read_BufferIndex;
	uint32_t JPEG_OUT_Write_BufferIndex;
	__IO uint32_t Output_Is_Paused;

	
	uint32_t JPEG_InputImageIndex;
	uint32_t JPEG_InputImageSize_Bytes;
	
	uint8_t* FrameBuffer;
	uint8_t const* JPEG_InputImageBuffer;
	
} oDecoderState = { .pConvert_Function = 0,
										.Jpeg_OUT_BufferTab =
{
	{JPEG_BUFFER_EMPTY , oDecoderState.MCU_Data_OutBuffers[0],  0},
	{JPEG_BUFFER_EMPTY , oDecoderState.MCU_Data_OutBuffers[1],  0}
}

};

static void ResetDecoderState()
{
	oDecoderState.pConvert_Function = 0;
	
	for (uint32_t iDx = 0 ; iDx < NB_OUTPUT_DATA_BUFFERS ; ++iDx) {
		oDecoderState.Jpeg_OUT_BufferTab[iDx].State = JPEG_BUFFER_EMPTY;
		oDecoderState.Jpeg_OUT_BufferTab[iDx].DataBufferSize = 0;
	}
	
	oDecoderState.MCU_TotalNb = 0;
	oDecoderState.MCU_BlockIndex = 0;
	oDecoderState.Jpeg_HWDecodingEnd = 0;

	oDecoderState.JPEG_OUT_Read_BufferIndex = 0;
	oDecoderState.JPEG_OUT_Write_BufferIndex = 0;
	oDecoderState.Output_Is_Paused = 0;

	oDecoderState.JPEG_InputImageIndex = 0;
	oDecoderState.JPEG_InputImageSize_Bytes = 0;
	
	oDecoderState.FrameBuffer = 0;
	oDecoderState.JPEG_InputImageBuffer = 0;
}
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Decode_DMA
  * @param hjpeg: JPEG handle pointer
  * @param  JPEGImageBufferAddress : jpg image buffer Address.
  * @param  JPEGImageSize_Bytes    : jpg image size in bytes.
  * @param  DestAddress : ARGB8888 destination Frame Buffer Address.
  * @retval None
  */
uint32_t JPEG_Decode_DMA(JPEG_HandleTypeDef *hjpeg, uint8_t const * const JPEGImageBuffer, uint32_t JPEGImageSize_Bytes, uint8_t* const DestBuffer){
	ResetDecoderState();
	
  oDecoderState.FrameBuffer = DestBuffer;
          
  oDecoderState.JPEG_InputImageIndex = 0;
  oDecoderState.JPEG_InputImageBuffer = JPEGImageBuffer;
  oDecoderState.JPEG_InputImageSize_Bytes = JPEGImageSize_Bytes;
  
  /* Start JPEG decoding with DMA method */
  HAL_JPEG_Decode_DMA(hjpeg, (uint8_t*)JPEGImageBuffer, CHUNK_SIZE_IN, oDecoderState.Jpeg_OUT_BufferTab[0].DataBuffer, CHUNK_SIZE_OUT);
  
  return 0;
}

/**
  * @brief  JPEG Ouput Data BackGround Postprocessing .
  * @param hjpeg: JPEG handle pointer
  * @retval 1 : if JPEG processing has finiched, 0 : if JPEG processing still ongoing
  */
uint32_t JPEG_OutputHandler(JPEG_HandleTypeDef *hjpeg)
{
  uint32_t ConvertedDataCount;
  
  if(oDecoderState.Jpeg_OUT_BufferTab[oDecoderState.JPEG_OUT_Read_BufferIndex].State == JPEG_BUFFER_FULL)
  {  
    oDecoderState.MCU_BlockIndex += oDecoderState.pConvert_Function(oDecoderState.Jpeg_OUT_BufferTab[oDecoderState.JPEG_OUT_Read_BufferIndex].DataBuffer, 
			oDecoderState.FrameBuffer, oDecoderState.MCU_BlockIndex, 
			oDecoderState.Jpeg_OUT_BufferTab[oDecoderState.JPEG_OUT_Read_BufferIndex].DataBufferSize, &ConvertedDataCount);    
    
    oDecoderState.Jpeg_OUT_BufferTab[oDecoderState.JPEG_OUT_Read_BufferIndex].State = JPEG_BUFFER_EMPTY;
    oDecoderState.Jpeg_OUT_BufferTab[oDecoderState.JPEG_OUT_Read_BufferIndex].DataBufferSize = 0;
    
    if(++oDecoderState.JPEG_OUT_Read_BufferIndex >= NB_OUTPUT_DATA_BUFFERS)
    {
      oDecoderState.JPEG_OUT_Read_BufferIndex = 0;
    }
    
    if(oDecoderState.MCU_BlockIndex == oDecoderState.MCU_TotalNb)
    {
      return 1;
    }
  }
  else if((oDecoderState.Output_Is_Paused == 1) &&
          (oDecoderState.JPEG_OUT_Write_BufferIndex == oDecoderState.JPEG_OUT_Read_BufferIndex) &&
          (oDecoderState.Jpeg_OUT_BufferTab[oDecoderState.JPEG_OUT_Read_BufferIndex].State == JPEG_BUFFER_EMPTY))
  {
    oDecoderState.Output_Is_Paused = 0;
    HAL_JPEG_Resume(hjpeg, JPEG_PAUSE_RESUME_OUTPUT);            
  }

  return 0;  
}

uint32_t const getCur_MCUBlockIndex()
{
	return(oDecoderState.MCU_BlockIndex);
}
uint32_t const getCur_MCUTotalNb()
{
	return(oDecoderState.MCU_TotalNb);
}

/**
  * @brief  JPEG Info ready callback
  * @param hjpeg: JPEG handle pointer
  * @param pInfo: JPEG Info Struct pointer
  * @retval None
  */
void HAL_JPEG_InfoReadyCallback(JPEG_HandleTypeDef *hjpeg, JPEG_ConfTypeDef *pInfo)
{
  if(JPEG_GetDecodeColorConvertFunc(pInfo, &oDecoderState.pConvert_Function, &oDecoderState.MCU_TotalNb) != HAL_OK)
  {
    OnError_Handler();
  }  
}

/**
  * @brief  JPEG Get Data callback
  * @param hjpeg: JPEG handle pointer
  * @param NbDecodedData: Number of decoded (consummed) bytes from input buffer
  * @retval None
  */
void HAL_JPEG_GetDataCallback(JPEG_HandleTypeDef *hjpeg, uint32_t NbDecodedData)
{
  uint32_t inDataLength;  
  
  oDecoderState.JPEG_InputImageIndex += NbDecodedData;
  
  if(oDecoderState.JPEG_InputImageIndex < oDecoderState.JPEG_InputImageSize_Bytes)
  {
    oDecoderState.JPEG_InputImageBuffer += NbDecodedData;
    
    if((oDecoderState.JPEG_InputImageSize_Bytes - oDecoderState.JPEG_InputImageIndex) >= CHUNK_SIZE_IN)
    {
      inDataLength = CHUNK_SIZE_IN;
    }
    else
    {
      inDataLength = oDecoderState.JPEG_InputImageSize_Bytes - oDecoderState.JPEG_InputImageIndex;
    }    
		
		HAL_JPEG_ConfigInputBuffer(hjpeg, (uint8_t *)oDecoderState.JPEG_InputImageBuffer, inDataLength);
  }
 /* else
  {
    inDataLength = 0; 
  }*/
  
}

/**
  * @brief  JPEG Data Ready callback
  * @param hjpeg: JPEG handle pointer
  * @param pDataOut: pointer to the output data buffer
  * @param OutDataLength: length of output buffer in bytes
  * @retval None
  */
void HAL_JPEG_DataReadyCallback (JPEG_HandleTypeDef *hjpeg, uint8_t *pDataOut, uint32_t OutDataLength)
{
  oDecoderState.Jpeg_OUT_BufferTab[oDecoderState.JPEG_OUT_Write_BufferIndex].State = JPEG_BUFFER_FULL;
  oDecoderState.Jpeg_OUT_BufferTab[oDecoderState.JPEG_OUT_Write_BufferIndex].DataBufferSize = OutDataLength;
    
  if(++oDecoderState.JPEG_OUT_Write_BufferIndex >= NB_OUTPUT_DATA_BUFFERS)
  {
    oDecoderState.JPEG_OUT_Write_BufferIndex = 0;        
  }

  if(oDecoderState.Jpeg_OUT_BufferTab[oDecoderState.JPEG_OUT_Write_BufferIndex].State != JPEG_BUFFER_EMPTY)
  {
    HAL_JPEG_Pause(hjpeg, JPEG_PAUSE_RESUME_OUTPUT);
    oDecoderState.Output_Is_Paused = 1;
  }
  HAL_JPEG_ConfigOutputBuffer(hjpeg, oDecoderState.Jpeg_OUT_BufferTab[oDecoderState.JPEG_OUT_Write_BufferIndex].DataBuffer, CHUNK_SIZE_OUT); 
}

/**
  * @brief  JPEG Error callback
  * @param hjpeg: JPEG handle pointer
  * @retval None
  */
void HAL_JPEG_ErrorCallback(JPEG_HandleTypeDef *hjpeg)
{
  OnError_Handler();
}

/**
  * @brief  JPEG Decode complete callback
  * @param hjpeg: JPEG handle pointer
  * @retval None
  */
void HAL_JPEG_DecodeCpltCallback(JPEG_HandleTypeDef *hjpeg)
{    
  oDecoderState.Jpeg_HWDecodingEnd = 1; 
}

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
