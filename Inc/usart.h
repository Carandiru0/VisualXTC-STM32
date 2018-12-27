/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#ifndef _USART_H
#define _USART_H
#include "PreprocessorCore.h"
#include "globals.h"

#if( 0 != USART_ENABLE )

namespace USART
{
	
void PushFrameBuffer( uint8_t const* const __restrict DitherTarget );
NOINLINE void InitUSART(void);
NOINLINE void DebugStartUp(void);
void ProcessUSARTDoubleBuffer(void);
	
} // end ns

// IRQ:
void USARTComplete_DMA1(void);

#endif
#endif



