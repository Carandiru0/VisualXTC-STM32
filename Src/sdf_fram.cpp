/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#include "sdf_fram.h"

#include "externSDFMulti.h"
#include "quadspi.h"
#include "effect_sdf_viewer.h"

#include "debug.cpp"

//////
#if defined(VERIFY_SDF_FRAM_SPECIFIC) || defined(PROGRAM_SDF_TO_FRAM)
extern const unsigned char SDFMulti_FRAM[];
extern const unsigned int  SDFMulti_FRAM_Size;
#endif
//////

#if defined(_DEBUG_OUT_OLED) && (defined(PROGRAM_SDF_TO_FRAM) || defined(VERIFY_SDF_FRAM_SPECIFIC))
static void ShowFRAMError(uint32_t const uiBytesVerified, uint32_t const uiBytesProgrammed, char szSource = '-')
{
	uint32_t const HAL_QSPI_ErrorCode = QuadSPI_FRAM::GetHALQSPIErrorCode();
	
	switch(QuadSPI_FRAM::GetLastProgrammingError())
	{
		case QuadSPI_FRAM::QSPI_PROGRAMMINGERROR_WRITEENABLEFAIL:
			DebugMessage( "FRAM %c Write Enable FAIL  x%d - %d =%d", szSource, uiBytesVerified, uiBytesProgrammed, HAL_QSPI_ErrorCode);
			break;
		case QuadSPI_FRAM::QSPI_PROGRAMMINGERROR_WRITEFAIL:
			DebugMessage( "FRAM %c Write FAIL  %d - %d =%d", szSource, uiBytesVerified, uiBytesProgrammed, HAL_QSPI_ErrorCode);
			break;
		case QuadSPI_FRAM::QSPI_PROGRAMMINGERROR_SIZEFAIL:
			DebugMessage( "FRAM %c Write Sixe too Big  %d - %d =%d", szSource, uiBytesVerified, uiBytesProgrammed, HAL_QSPI_ErrorCode);
			break;
		case QuadSPI_FRAM::QSPI_PROGRAMMINGERROR_WRITEDISABLEFAIL:
			DebugMessage( "FRAM %c Write Disable FAIL  %d - %d =%d", szSource, uiBytesVerified, uiBytesProgrammed, HAL_QSPI_ErrorCode);
			break;
		case QuadSPI_FRAM::QSPI_PROGRAMMINGERROR_READFAIL:
			DebugMessage( "FRAM %c Read FAIL  %d - %d =%d", szSource, uiBytesVerified, uiBytesProgrammed, HAL_QSPI_ErrorCode);
			break;
		case QuadSPI_FRAM::QSPI_PROGRAMMINGERROR_NOTEQUAL:
			DebugMessage( "FRAM %c Verifx Not Equal  %d - %d =%d", szSource, uiBytesVerified, uiBytesProgrammed, HAL_QSPI_ErrorCode);
		  break;
		case QuadSPI_FRAM::QSPI_PROGRAMMINGERROR_WRITEINCOMPLETE:
			DebugMessage( "FRAM %c Write Incomplete  %d - %d =%d", szSource, uiBytesVerified, uiBytesProgrammed, HAL_QSPI_ErrorCode);
			break;
		case QuadSPI_FRAM::QSPI_MEMORYMAPPED_ENABLEFAIL:
			DebugMessage( "FRAM %c MemorxMapped Enable FAIL  %d - %d =%d", szSource, uiBytesVerified, uiBytesProgrammed, HAL_QSPI_ErrorCode);
			break;
		case QuadSPI_FRAM::QSPI_PROGRAMMINGERROR_VERIFYINCOMPLETE:
			DebugMessage( "FRAM %c Verifx Incomplete  %d - %d =%d", szSource, uiBytesVerified, uiBytesProgrammed, HAL_QSPI_ErrorCode);
			break;
		default:
		case QuadSPI_FRAM::QSPI_PROGRAMMINGERROR_NONE:
			DebugMessage( "FRAM %c Unknown State  %d - %d =%d", uiBytesVerified, uiBytesProgrammed, HAL_QSPI_ErrorCode);
			break;
	}
}
#else
#define ShowFRAMError(n,m,l) 
#endif

#ifdef PROGRAM_SDF_TO_FRAM
static void VerifySDF_FRAM_All(bool& bVerified, uint32_t& uiVerified, uint8_t const* Address, uint32_t const uiExpectedProgrammed)
{
	if (!bVerified)	// Verify SDF Data
	{
		uint32_t const State = QuadSPI_FRAM::VerifyFRAM( uiVerified, uiExpectedProgrammed, Address,
																			               QuadSPI_FRAM::ProgrammingDesc(SDFMulti_FRAM, SDFMulti_FRAM_Size) );
																											
																												
		switch( State )
		{
			case QuadSPI_FRAM::QSPI_VERIFY_PROGRAMMINGSTATE_IDLE:
				DebugMessage( "FRAM Verifx Idle  %d - %d", uiVerified, uiExpectedProgrammed);
				break;
			case QuadSPI_FRAM::QSPI_VERIFY_PROGRAMMINGSTATE_PENDING:
				DebugMessage( "FRAM Verifing...  %d - %d", uiVerified, uiExpectedProgrammed);
				break;
			case QuadSPI_FRAM::QSPI_VERIFY_PROGRAMMINGSTATE_COMPLETE:
				DebugMessage( "FRAM All Verified Complete !  %d - %d", uiVerified, uiExpectedProgrammed);	
				bVerified = true;
				QuadSPI_FRAM::ResetVerifyFRAMState();
				break;
			case QuadSPI_FRAM::QSPI_VERIFY_PROGRAMMINGSTATE_ERROR:
			default:
				ShowFRAMError(uiVerified, uiExpectedProgrammed);
				break;
		}
	}
}
#endif // PROGRAM_SDF_TO_FRAM

#ifdef PROGRAM_SDF_TO_FRAM
namespace SDF_FRAM
{
	
NOINLINE void ProgramSDF_FRAM()
{
#ifndef VERIFY_SDF_FRAM_ONLY
	static bool bProgrammed(false);
#else
	static bool bProgrammed(true);
#endif
	static bool bVerified(false), bVerifiedAgain(false);
	static uint32_t uiProgrammed(0), uiVerified(0);
	
	if ( !bProgrammed ) // Programming the binary data for the SDFs
	{
#ifndef VERIFY_SDF_FRAM_ONLY
		uint32_t const State = QuadSPI_FRAM::ProgramFRAM( uiProgrammed,
																											QuadSPI_FRAM::ProgrammingDesc(SDFMulti_FRAM, SDFMulti_FRAM_Size) );
																											
		switch( State )
		{
			case QuadSPI_FRAM::QSPI_PROGRAMMINGSTATE_IDLE:
				DebugMessage( "FRAM Programming Idle  %d", uiProgrammed);
				break;
			case QuadSPI_FRAM::QSPI_PROGRAMMINGSTATE_PENDING:
				DebugMessage( "FRAM Programming...  %d", uiProgrammed);
				break;
			case QuadSPI_FRAM::QSPI_PROGRAMMINGSTATE_COMPLETE:
				DebugMessage( "FRAM Programming Complete !  %d", uiProgrammed);
				
				if ( QuadSPI_FRAM::QSPI_PROGRAMMINGSTATE_IDLE != QuadSPI_FRAM::AckComplete_ResetProgrammingState())
					ShowFRAMError(uiVerified, uiProgrammed);
				else
					bProgrammed = true;
				break;
			case QuadSPI_FRAM::QSPI_PROGRAMMINGSTATE_ERROR:
			default:
				ShowFRAMError(uiVerified, uiProgrammed);
				break;
		}
#endif
	}
	else if (!bVerified)	// Verify SDF Data
	{
#ifdef VERIFY_SDF_FRAM_ONLY
		uiProgrammed = SDFMulti_FRAM_Size;
#endif
		VerifySDF_FRAM_All(bVerified, uiVerified, QuadSPI_FRAM::QSPI_Address, uiProgrammed);
	}
	/*else if (!bVerifiedAgain)
	{
		uiVerified = 0;
		VerifySDF_FRAM(bVerifiedAgain, uiVerified, uiTotalProgrammed);
	}*/
}
} // end namespace
#endif // PROGRAM_SDF_TO_FRAM

#if defined(VERIFY_SDF_FRAM_SPECIFIC) || defined(PROGRAM_SDF_TO_FRAM)
namespace SDF_FRAM
{
	
void VerifySDF_FRAM(bool& bVerified, uint32_t& uiVerified, uint8_t const* Address, uint32_t const uiExpectedProgrammed)
{
	if (!bVerified)	// Verify SDF Data
	{
		uint8_t const* const SrcAddress = SDFMulti_FRAM + ((uint32_t const)Address - (uint32_t const)QuadSPI_FRAM::QSPI_Address);
		uint32_t const State = QuadSPI_FRAM::VerifyFRAM( uiVerified, uiExpectedProgrammed, Address,
																			               QuadSPI_FRAM::ProgrammingDesc(SrcAddress, uiExpectedProgrammed) );
																											
																												
		switch( State )
		{
			case QuadSPI_FRAM::QSPI_VERIFY_PROGRAMMINGSTATE_IDLE:
				DebugMessage( "FRAM Verifx Idle  %d - %d", uiVerified, uiExpectedProgrammed);
				break;
			case QuadSPI_FRAM::QSPI_VERIFY_PROGRAMMINGSTATE_PENDING:
				DebugMessage( "FRAM Verifing...  %d - %d", uiVerified, uiExpectedProgrammed);
				break;
			case QuadSPI_FRAM::QSPI_VERIFY_PROGRAMMINGSTATE_COMPLETE:
				DebugMessage( "FRAM Verified Complete !  %d - %d", uiVerified, uiExpectedProgrammed);	
				bVerified = true;
				QuadSPI_FRAM::ResetVerifyFRAMState();
				break;
			case QuadSPI_FRAM::QSPI_VERIFY_PROGRAMMINGSTATE_ERROR:
			default:
				ShowFRAMError(uiVerified, uiExpectedProgrammed);
				break;
		}
	}
}

} // end namespace
#endif

