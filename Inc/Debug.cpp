/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef DEBUG_HPP
#define DEBUG_HPP
#include "PreprocessorCore.h"
#include <stdio.h>
#include <float.h>

#ifdef _DEBUG_OUT_OLED   // a longer nInterval converges on the maximum and average faster, giving the most appropriate result
												 // 500 - 750 ms seems to be good. Maximum & Average is calculated over a duration of 1 second for accuracy
#define PROFILE_START() {static uint32_t tLastDebug(0), tLastReset(0), uiTotalMaximum(0), uiMaximum(0), uiAvgMax(0), uiCountMax(0); \
												 static bool bSkipNextTimingOnMessageOut(true); \
											   uint32_t const tProfileStart = micros();
#define PROFILE_END(nInterval, nFilter)	if ( !bSkipNextTimingOnMessageOut ) { \
																					uint32_t const tDelta = micros() - tProfileStart; \
																					if ( tDelta < nFilter ) { \
																						uiMaximum = max( uiMaximum, tDelta ); \
																						uiAvgMax += uiMaximum; ++uiCountMax; \
																						if ( millis() - tLastDebug > nInterval) { \
																							uiTotalMaximum = max( uiTotalMaximum, uiMaximum ); \
																							DebugMessage("%d / %d", uiTotalMaximum, (uiAvgMax / uiCountMax)); \
																							uiMaximum = 0; \
																							tLastDebug = millis(); \
																							if (tLastDebug - tLastReset > 1000) { \
																								uiTotalMaximum = 0; uiAvgMax = 0; uiCountMax = 0; \
																								tLastReset = tLastDebug; \
																							} \
																							bSkipNextTimingOnMessageOut = true; \
																				 } } } else bSkipNextTimingOnMessageOut = false; }
											
static constexpr uint32_t const DEBUG_COUNT = 12;
static constexpr int32_t const DEBUG_OUT_RESET = -999;

typedef struct sDEBUG
{
	static constexpr uint32_t const QUEUE_SIZE = 3,
																	MESSAGE_BYTES = 256;
	int32_t i[DEBUG_COUNT];
	float32_t f[DEBUG_COUNT];
	
	static struct sDebugMessageQueue
	{
		static uint32_t Position;
		char szMessage[MESSAGE_BYTES];
		
		/* example of varaidic template
		template <typename... Args>
		void print_error(const char *file, int line, const char *format,
										 const Args & ... args) {
			fmt::print("{}: {}: ", file, line);
			fmt::print(format, args...);
		}
		*/
		template <typename T>
		T const& Argument(T const& value) noexcept
		{
			return(value);
		}
		
		void write(char const* const str) noexcept	// overload for constant strings, compiler complains about insecurity
		{
			strncpy(szMessage, str, MESSAGE_BYTES);

			if ( ++Position == QUEUE_SIZE )
				Position = 0;
		}
		
		template <typename... Args>
		void write(char const* const str, Args const& ... args) noexcept
		{
			snprintf(szMessage, MESSAGE_BYTES, str, Argument(args)...);
			
			if ( ++Position == QUEUE_SIZE )
				Position = 0;
		}
	} oDebugMessageQueue[QUEUE_SIZE]
	__attribute__((section (".bss.debugmessagequeue")));
	
	sDEBUG()
	{
		for ( uint32_t iDx = 0 ; iDx < DEBUG_COUNT ; ++iDx ) {
			i[iDx] = DEBUG_OUT_RESET;
			f[iDx] = DEBUG_OUT_RESET;
		}
	}
	
	sDebugMessageQueue& getDebugWriter() { return(oDebugMessageQueue[sDebugMessageQueue::Position]); }
	bool const isQueueEmpty() const {
		if ( 0 == sDebugMessageQueue::Position ) {

			int32_t iDx(QUEUE_SIZE-1);
			while (iDx >= 0)
			{
				if (0 != oDebugMessageQueue[iDx].szMessage[0])
					return(false);
				
				--iDx;
			}
			return(true);
		}
		
		return(false);
	}
	
} DEBUG_ONLY;

extern DEBUG_ONLY oDEBUG
	__attribute__((section (".bss.debugobject")));

#define SerDebugOut(n,m)  { oDEBUG.i[n] = m; }
#define SerDebugOut_Float(n,m)  { oDEBUG.f[n] = m; }
#define DebugMessage oDEBUG.getDebugWriter().write
#define IsDebugQueueEmpty() oDEBUG.isQueueEmpty()
#define GetCurDebugMessage(n) oDEBUG.oDebugMessageQueue[n].szMessage
#define ResetDebugMessageQueue() { int32_t iDx = DEBUG_ONLY::QUEUE_SIZE - 1; while ( iDx >= 0 ) { memset(oDEBUG.oDebugMessageQueue[iDx].szMessage, 0x00, 256); --iDx; } DEBUG_ONLY::sDebugMessageQueue::Position = 0; }
#define DebugQueuePosition DEBUG_ONLY::sDebugMessageQueue::Position

#else

#define SerDebugOut(n,m)
#define SerDebugOut_Float(n,m) 
#define DebugMessage (void)

#endif

extern void StartUp_Output_Sys();


#endif

