/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#ifndef __spi_H
#define __spi_H
#include "PreprocessorCore.h"

/* Includes ------------------------------------------------------------------*/

extern void _Error_Handler(char *, int);

NOINLINE void MX_SPI1_Init(void);

 void SPI1_TransmitByte(uint8_t const dataByte);
 void SPI1_TransmitHalfWord(uint16_t const dataHalfWord);
 void SPI1_TransmitHalfWord(uint16_t const dataByteFirst, uint16_t const dataByteSecond);

 void WaitForSPI1BufferEmpty(void);
 void WaitPendingSPI1ChipDeselect(void);



#endif /*__ spi_H */
