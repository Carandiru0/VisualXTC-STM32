/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __dma_H
#define __dma_H  
#include "PreprocessorCore.h"

/* Includes ------------------------------------------------------------------*/

// a pragma disable warning that actually disables the warning!
typedef struct sMem2Mem
{
	uint8_t const * const ConstData;
	uint8_t* const  		  SRAMData;
	
	uint32_t const SizeInWords;
	volatile bool* const __restrict bbFinished;
	
	void operator= ( sMem2Mem && in );
	
	sMem2Mem() : bbFinished(nullptr), ConstData(nullptr), SRAMData(nullptr), SizeInWords(0) {}
		
	explicit sMem2Mem(volatile bool* const pbFinished,
										uint8_t const * const pConstData,
										uint8_t* const pSRAMData,
										uint32_t const uiSizeInWords)
											: bbFinished(pbFinished), ConstData(pConstData), SRAMData(pSRAMData), SizeInWords(uiSizeInWords)
										{
											
										}
} Mem2Mem;

void InitM2M( Mem2Mem * const Config, 
							volatile bool* const pbFinished,
							uint8_t const * const pConstData,
							uint8_t* const pSRAMData,
							uint32_t const uiSizeInWords );

void DeferredCopyMemoryToMemory( Mem2Mem & Config );
void DMAMem2MemComplete_DMA2_Stream(uint32_t const Stream);
/* DMA memory to memory transfer handles -------------------------------------*/
extern void _Error_Handler(char*, int);


NOINLINE void MX_DMA_Init(void);



#endif /* __dma_H */

