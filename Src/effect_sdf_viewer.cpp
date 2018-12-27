/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#include "effect_sdf_viewer.h"
#include "globals.h"
#include "RenderSDF_DMA2D.h"
#include "oled.h"
#include "dma.h"
#include "externSDFMulti.h"
#include "Point2D.h"
#include "rng.h"
#include "jpeg.h"
#include "quadspi.h"

#ifdef ENABLE_SKULL

#include "debug.cpp"

#include "FRAM\FRAM_SDF_MemoryLayout.h"
#if defined(VERIFY_SDF_FRAM_SPECIFIC) || defined(PROGRAM_SDF_TO_FRAM)
#include "sdf_fram.h"
#endif

using namespace SDF;

namespace SDF_Viewer
{
static constexpr uint32_t const CLIP_TOPBOTTOM_PIXELS = 4;
	 
static constexpr uint32_t const SDF_MIN_WIDTH = OLED::SCREEN_WIDTH,
																SDF_MAX_WIDTH = SDF_MIN_WIDTH << 1;

static constexpr float const SPEED_NORMAL = 4.26f,
														 SPEED_FAST = 80.0f;

static constexpr uint32_t const RenderFrameSDF_Interval = 20,
																RenderSDF_Balancer = 1;


static  constexpr uint32_t const  SDF_13_WIDTH = SDF_13_Header::ORIGWIDTH,
																  SDF_13_HEIGHT = SDF_13_Header::ORIGHEIGHT,
																  SDF_13_FOCUS_X = (SDF_13_WIDTH>>1) - 350,
																  SDF_13_FOCUS_Y = (SDF_13_HEIGHT>>1) - 100;
static  constexpr float32_t const SDF_13_SCALE = 1.3f;

static SDFSource const* const oSDF[] = {
	// 1
                    new SDFSource32({sSDFLayer(SDF_Succubus_13_0,SDF_Succubus_13_0_Size),sSDFLayer(SDF_Succubus_13_1,SDF_Succubus_13_1_Size),sSDFLayer(SDF_Succubus_13_2,SDF_Succubus_13_2_Size),sSDFLayer(SDF_Succubus_13_3,SDF_Succubus_13_3_Size),sSDFLayer(SDF_Succubus_13_4,SDF_Succubus_13_4_Size),
											sSDFLayer(SDF_Succubus_13_5,SDF_Succubus_13_5_Size),sSDFLayer(SDF_Succubus_13_6,SDF_Succubus_13_6_Size),sSDFLayer(SDF_Succubus_13_7,SDF_Succubus_13_7_Size),sSDFLayer(SDF_Succubus_13_8,SDF_Succubus_13_8_Size),sSDFLayer(SDF_Succubus_13_9, SDF_Succubus_13_9_Size),
											sSDFLayer(SDF_Succubus_13_10,SDF_Succubus_13_10_Size),sSDFLayer(SDF_Succubus_13_11,SDF_Succubus_13_11_Size),sSDFLayer(SDF_Succubus_13_12,SDF_Succubus_13_12_Size),sSDFLayer(SDF_Succubus_13_13,SDF_Succubus_13_13_Size),
										  sSDFLayer(SDF_Succubus_13_14,SDF_Succubus_13_14_Size),sSDFLayer(SDF_Succubus_13_15, SDF_Succubus_13_15_Size),sSDFLayer(SDF_Succubus_13_16, SDF_Succubus_13_16_Size),sSDFLayer(SDF_Succubus_13_17,SDF_Succubus_13_17_Size),
											sSDFLayer(SDF_Succubus_13_18,SDF_Succubus_13_18_Size),sSDFLayer(SDF_Succubus_13_19, SDF_Succubus_13_19_Size),sSDFLayer(SDF_Succubus_13_20, SDF_Succubus_13_20_Size),sSDFLayer(SDF_Succubus_13_21,SDF_Succubus_13_21_Size),
											sSDFLayer(SDF_Succubus_13_22,SDF_Succubus_13_22_Size),sSDFLayer(SDF_Succubus_13_23, SDF_Succubus_13_23_Size),sSDFLayer(SDF_Succubus_13_24, SDF_Succubus_13_24_Size),sSDFLayer(SDF_Succubus_13_25,SDF_Succubus_13_25_Size),
											sSDFLayer(SDF_Succubus_13_26,SDF_Succubus_13_26_Size),sSDFLayer(SDF_Succubus_13_27, SDF_Succubus_13_27_Size),sSDFLayer(SDF_Succubus_13_28, SDF_Succubus_13_28_Size),sSDFLayer(SDF_Succubus_13_29,SDF_Succubus_13_29_Size),
											sSDFLayer(SDF_Succubus_13_30,SDF_Succubus_13_30_Size),sSDFLayer(SDF_Succubus_13_31, SDF_Succubus_13_31_Size)},
										  SDF_13_Header::SHADES, SDF_13_Header::NUMSHADES,
											Pixels(SDF_13_WIDTH, SDF_13_HEIGHT),  SDF_13_SCALE,
											uVector2( (float)SDF_13_FOCUS_X / (float)SDF_13_WIDTH, (float)SDF_13_FOCUS_Y / (float)SDF_13_HEIGHT ),
											&RenderSignedDistanceField<Orient_Normal>,
											false)
									 ,		 
}; // End oSDF[]

static constexpr uint32_t const NUMSDFS = countof(oSDF);

typedef struct sSDFTiming
{
	uint32_t tLastLoaded;
	
	sSDFTiming()
	: tLastLoaded(0)
	{}
} SDFTiming;

static struct sSDFViewer
{
	static constexpr uint32_t const ZOOMING = 0,
																	PANNING = 1;
		
	static uint8_t RenderBuffer0[OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT]	// 8bpp,     16KB
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".bss"))),
								 RenderBuffer1[OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT]	// 8bpp,     16KB
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".bss"))),
								 RenderBuffer2[OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT]	// 8bpp,     16KB
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".bss")));
	
	static uint8_t ShadesCached[_MAXSHADES]
	  __attribute__((section (".dtcm")));
		
	SDFSource const * __restrict SDFMulti
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)));
	
	uint8_t * __restrict WorkingRenderBuffer  				
	__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))),
					* __restrict NextRenderBuffer  						// SDF Render finalizes to this
	__attribute__((aligned(ARMV7M_DCACHE_LINESIZE))),
				  * __restrict CurRenderBuffer							// for linear interpolation between rendered SDF
	__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)));
	
	float32_t CurStepY, CurStepWidth, CurStepHeight, CurStepWidthRatio, CurStepHeightRatio,
	          tInvDeltaRenderTotal, LerpErrorCorrectionModifier;
	float32_t CurSpeed;
	
	//OLED::Effects::ScanlinerIntercept FocusIntercept;
	SDFDynamicLayerParams*		LayerConfig;
	SignedDistanceFieldParams Config;
	SDFTiming									Timing[NUMSDFS];
	
	uint32_t CacheInvalidated,
					 DeltaRenderTotal;

	uint32_t CameraState;
	uint32_t JPEGLayerDecompressed,
					 idJPEG;
	
	sSDFViewer() : CurStepY(0), CurStepWidth(0), CurStepHeight(0), CurStepWidthRatio(0),  CurStepHeightRatio(0), CacheInvalidated(1),
	DeltaRenderTotal(0), tInvDeltaRenderTotal(0.0f), LerpErrorCorrectionModifier(1.0f), CurSpeed(SPEED_FAST),
	JPEGLayerDecompressed(SDFLayer::COMPRESSED_UNLOADED), idJPEG(0), CameraState(PANNING),
	LayerConfig(nullptr)
	
	{
		WorkingRenderBuffer = RenderBuffer2;
		NextRenderBuffer = RenderBuffer1;
		CurRenderBuffer = RenderBuffer0;
	}
} oViewer;

// bss places it a lower priority (higher address) which
// saves DTCM Memory for data that is defined in a section w/o .bss
uint8_t sSDFViewer::RenderBuffer0[OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT]	
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".bss"))),
				sSDFViewer::RenderBuffer1[OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT]	
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".bss"))),
				sSDFViewer::RenderBuffer2[OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT]
  __attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".bss")));


uint8_t sSDFViewer::ShadesCached[_MAXSHADES] 
	  __attribute__((section (".dtcm")));

static uint32_t const SelectNextSDFByTiming(uint32_t const tNow)
{
	uint32_t uiSelectedIndex(0);
	
	uint32_t tMaxDelta(0);
	
	for (int32_t iDx = NUMSDFS - 1; iDx >= 0 ; --iDx )
	{
		uint32_t const CurDelta = tNow - oViewer.Timing[iDx].tLastLoaded;
		if ( CurDelta > tMaxDelta ) {
			tMaxDelta = CurDelta;
			uiSelectedIndex = iDx;
		}
	}
	
	return(uiSelectedIndex);
}

static void LoadNextSDF(uint32_t const tNow)
{
	static constexpr int32_t const UNITIALIZED_STATE = -1;
	static constexpr int32_t const IS_FIRST_RUN = 1,
																  IS_FIRST_RUN_FINISHED = 0;
	
	static int32_t iFirstFastRun(UNITIALIZED_STATE);
	
	static uint32_t countSDFLoads(0);
	
	static uint32_t lastSDFIndex(0), lastSDFIndex2(0), lastSDFIndex3(0);
		
	// Anything that may use the uninit state goes here
	if (unlikely( UNITIALIZED_STATE == iFirstFastRun))
	{
		iFirstFastRun = IS_FIRST_RUN;
		countSDFLoads = 0;
	}
	else
	{
		
		// one or the other applies
		if ( IS_FIRST_RUN_FINISHED == iFirstFastRun ) {
			oViewer.CurSpeed = SPEED_NORMAL; // After first fast pass of all SDF's return and keep it normal
		}
		else { // IS_FIRST_RUN
			
		}
	}
	
	// Everything that does not need the unitnit state goes here
	uint32_t nextSDFIndex(0);
	if ( (IS_FIRST_RUN_FINISHED == iFirstFastRun) ) { /// only if first fast scan
		
		if ( RandomNumber32() < (UINT32_MAX >> 1) ) // apply randomization 50% of the time
		{
			uint32_t const uiRandomIndex = RandomNumber32(0, NUMSDFS - 1);
			// if the returned random number does not equal the last 3 SDF's loaded
			if (uiRandomIndex != lastSDFIndex && uiRandomIndex != lastSDFIndex2 && uiRandomIndex != lastSDFIndex3 )
				nextSDFIndex = uiRandomIndex;
			else // fall-back to load by oldest
				nextSDFIndex = SelectNextSDFByTiming(tNow);
		}
		else // fall-back to load by oldest
			nextSDFIndex = SelectNextSDFByTiming(tNow);
	}
	else // fall-back to load by oldest
		nextSDFIndex = SelectNextSDFByTiming(tNow);
	 
	oViewer.SDFMulti = oSDF[nextSDFIndex];
	oViewer.Timing[nextSDFIndex].tLastLoaded = tNow;
	
	SAFE_DELETE_ARRAY(oViewer.LayerConfig);
	oViewer.LayerConfig = new SDFDynamicLayerParams[ oViewer.SDFMulti->NumShades ];
	
	memset(oViewer.ShadesCached, 0x00, _MAXSHADES);
	memcpy8(oViewer.ShadesCached, oViewer.SDFMulti->Shades, oViewer.SDFMulti->NumShades);
	
	oViewer.CurStepY = 0;
	oViewer.CurStepWidthRatio = (float32_t)oViewer.SDFMulti->vDimensions.area.width / (float32_t)oViewer.SDFMulti->vDimensions.area.height;
	oViewer.CurStepHeightRatio = (float32_t)oViewer.SDFMulti->vDimensions.area.height /(float32_t)oViewer.SDFMulti->vDimensions.area.width;
	oViewer.CurStepWidth = SDF_MIN_WIDTH;
	oViewer.CurStepHeight = SDF_MIN_WIDTH * oViewer.CurStepHeightRatio;
	
	oViewer.CameraState = sSDFViewer::PANNING;
	oViewer.JPEGLayerDecompressed = SDFLayer::COMPRESSED_UNLOADED;	
	
	if ( NUMSDFS == ++countSDFLoads ) {
		iFirstFastRun = IS_FIRST_RUN_FINISHED;
	}
	
	lastSDFIndex3 = lastSDFIndex2;
	lastSDFIndex2 = lastSDFIndex;
	lastSDFIndex = nextSDFIndex;
}
void Initialize()
{
	memset(oViewer.RenderBuffer0, 0x00, OLED::SCREEN_WIDTH*OLED::SCREEN_HEIGHT );
	LoadNextSDF( millis() );
}

uint8_t const getShadeByIndex( uint32_t const iDx )
{
	return(oViewer.ShadesCached[iDx]);
}
static bool const UpdateParallaxOffsets( uint32_t const tNow )
{
	static constexpr float32_t DEFAULT_MAGNITUDE = 0.33f;
	static constexpr int32_t const GOINGLEFT = -1,
																 CENTERED = 0,
																 GOINGRIGHT = 1,
																 INTERVAL = 24;
	
	static Vector2 vDir(1.0f , 0.0f);
	static uint32_t tLastUpdate;
	static int32_t LastState = GOINGLEFT, NextState = GOINGLEFT;

	bool bOverdraw(false);
	
	if ( tNow - tLastUpdate >  INTERVAL )
	{		
		switch(NextState)
		{
			case GOINGLEFT:
				vDir.pt.x = -1.0f * DEFAULT_MAGNITUDE;
			  NextState = CENTERED; // next state
				bOverdraw = true;
				break;
			case CENTERED:
				if (GOINGLEFT == LastState) {
					vDir.pt.x = 0.5f * DEFAULT_MAGNITUDE;
					NextState = GOINGRIGHT;
				}
				else {
					vDir.pt.x = -0.0f * DEFAULT_MAGNITUDE;
					NextState = GOINGLEFT;
				}
				break;
			case GOINGRIGHT:
				vDir.pt.x = 1.0f * DEFAULT_MAGNITUDE;
			  NextState = CENTERED; // next state
				bOverdraw = true;
				break;
		}

		float fScaleSpread;
		if( oViewer.SDFMulti->NumShades < SDF::n16SHADES )
		{
			fScaleSpread = 0.5f;
		}
		else {
			fScaleSpread = 0.25f;
		}
		
		for ( uint32_t iDx = oViewer.SDFMulti->NumShades - 1 ; iDx != 0 ; --iDx )
		{
			oViewer.LayerConfig[iDx].vOffset.pt.x = (vDir.pt.x * ((float32_t)(iDx + 1)) * fScaleSpread) * ( oViewer.SDFMulti->Scale );
		}
		
		LastState = NextState;
		tLastUpdate = tNow;
	}
	
	return(bOverdraw);
}

STATIC_INLINE void UpdateXOffset(SignedDistanceFieldParams& __restrict Config)
{
	Config.xOffset = (float32_t)Config.outputHeight * oViewer.SDFMulti->vFocus.pt.x;
}
STATIC_INLINE void UpdateYOffset(SignedDistanceFieldParams& __restrict Config, bool const bDirection)
{
	switch(oViewer.CameraState)
	{
	case sSDFViewer::ZOOMING:
	{
		float const fMaxRange = clampf( (oViewer.CurStepWidth - (float32_t)SDF_MIN_WIDTH) / (float32_t)(SDF_MAX_WIDTH >> 1) );
		if (bDirection) {
			
			Config.yOffset = smoothstep( CLIP_TOPBOTTOM_PIXELS, 
																	((float32_t)Config.outputHeight * oViewer.SDFMulti->vFocus.pt.y), fMaxRange);
		}		
		else {

			// we are at the focus point
			Config.yOffset = ((float32_t)Config.outputHeight * oViewer.SDFMulti->vFocus.pt.y);
			
			/*
			Config.yOffset = smoothstep( ((float32_t)Config.outputHeight * oViewer.SDFMulti->vFocus.pt.y),
																	 Config.outputHeight - CLIP_TOPBOTTOM_PIXELS, 1.0f - fMaxRange);*/
		}
		
		//SerDebugOut(3, (int32_t)(fMaxRange * 100.0f * 2.0f));
		break;
	}
	default:
	case sSDFViewer::PANNING:
	{
		float const fMaxRange = clampf( oViewer.CurStepY / ((Config.outputHeight - OLED::SCREEN_HEIGHT) << 2) );

		Config.yOffset = smoothstep( CLIP_TOPBOTTOM_PIXELS, Config.outputHeight - OLED::SCREEN_HEIGHT - CLIP_TOPBOTTOM_PIXELS, fMaxRange);
		
		//SerDebugOut(3, (int32_t)(fMaxRange * 100.0f));
		break;
	}
} // switch
}

static void BeginNextJPEGDecompression(uint32_t const uiRenderIndex)
{
	uint32_t const idJPEGUnique = JPEGDecoder::Start_Decode( oViewer.SDFMulti->getLayer(uiRenderIndex).SDF,
																													 oViewer.SDFMulti->getLayer(uiRenderIndex).Size, oViewer.SDFMulti->IsFRAM );
	
	if ( 0 != idJPEGUnique ) {
		oViewer.idJPEG = idJPEGUnique;
		oViewer.JPEGLayerDecompressed = SDFLayer::COMPRESSED_PENDING;				
	}
	//else {
		// Not Ready, ignore and try on next pass
	//}															
}
static bool const CheckJPEGDecompressionState(uint32_t const uiRenderIndex, uint8_t const* __restrict& __restrict pLayerSourceBits)
{
	switch(oViewer.JPEGLayerDecompressed)
	{
		case SDFLayer::COMPRESSED_UNLOADED:
			BeginNextJPEGDecompression(uiRenderIndex);
			break;
												 
		case SDFLayer::COMPRESSED_PENDING:
		{
			int32_t const JPEGStatus = JPEGDecoder::IsReady(oViewer.idJPEG);
			switch(JPEGStatus)
			{
				case JPEGDecoder::READY:
					oViewer.JPEGLayerDecompressed = SDFLayer::UNCOMPRESSED_LOADED;
					goto JPEGReady; // Must use decompressed JPEG immediately, before another potential owner request a new decode
					
				case JPEGDecoder::TIMED_OUT:
					oViewer.JPEGLayerDecompressed = SDFLayer::COMPRESSED_UNLOADED;				
					return(false);	// return error state
				//default:
					// busy...
			}
			break;
		} // end case PENDING
		case SDFLayer::UNCOMPRESSED_LOADED:
JPEGReady:
			pLayerSourceBits = JPEGDecoder::getDecompressionBuffer();
			break;
	}
	return(true); // no errors or timed out
}

#define HANDLE_JPEG_ERROR { \
	eRenderStatus = RENDER_SLEEPING; \
	LastShade = 0; \
	bDirection = true; \
	bLoadNextSDF = true; \
	SerDebugOut(0, -1);	 \
	return; \
} \

#ifdef PROGRAM_SDF_TO_FRAM
void TestSDF_All_Layers_FRAM()
{
	static int32_t iRenderIndex(-1);
	static uint32_t uiLastSDFLayerZeroAddress(0);
	
	if ( iRenderIndex >= 0 ) {
		bool bVerified(false);
		uint32_t uiVerified(0);
			
		// Test one layer per update
		SDF_FRAM::VerifySDF_FRAM(bVerified, uiVerified, oViewer.SDFMulti->Layer[iRenderIndex].SDF, oViewer.SDFMulti->Layer[iRenderIndex].Size);
		
		if (bVerified) {
			// done this sdf, flag to not run again
			if (++iRenderIndex == oViewer.SDFMulti->NumShades)
				iRenderIndex = -1;
		}
	}
	else {
			// only start test if sdf is different in fram
			if ( uiLastSDFLayerZeroAddress != (uint32_t const)oViewer.SDFMulti->Layer[0].SDF )
			{
				// start new SDF test next update
				iRenderIndex = 0;
				uiLastSDFLayerZeroAddress = (uint32_t const)oViewer.SDFMulti->Layer[0].SDF;
			}
	}
}
#endif

void Update(uint32_t const tNow)
{
	static constexpr int32_t const RENDER_SLEEPING = -1,
																 RENDER_START = 0,
																 RENDER_PIXELSTEP = 1;

	static constexpr float32_t const PAN_UP_SPEED = 7.5f,
																	 PAN_DOWN_SPEED = 4.5f;
	
	static int32_t eRenderStatus = RENDER_SLEEPING;
	
	static bool bDirection(true), bLoadNextSDF(false);
	static uint32_t tNextRenderUpdate(0), tLastUpdated, tLastFrameRendered, 
									tFrameRenderDeltaTime(RenderFrameSDF_Interval);
	
	static uint8_t LastShade(0);
	
#ifdef PROGRAM_SDF_TO_FRAM
	SDF_FRAM::ProgramSDF_FRAM();
#endif
	
	if ( (RENDER_SLEEPING == eRenderStatus) )
	{
		if (tNow > tNextRenderUpdate)
		{
			bool bPanSmoothing(oViewer.CameraState); // Off for zooming by default
			float const tDeltaSpeed = (((float)(tNow - tLastUpdated)) * 0.001f) * 0.5f * oViewer.CurSpeed;

			if (sSDFViewer::ZOOMING == oViewer.CameraState) 
			{
				if (bDirection)
				{
					if (bLoadNextSDF) {
						LoadNextSDF(tNow);
						bLoadNextSDF = false;
						return;
					}
					oViewer.CurStepWidth += oViewer.CurStepWidthRatio * tDeltaSpeed;
					oViewer.CurStepHeight += oViewer.CurStepHeightRatio * tDeltaSpeed;
					
					if ( oViewer.CurStepWidth >= SDF_MAX_WIDTH){
						bDirection = false;
					}
				}
				else
				{
					oViewer.CurStepWidth -= oViewer.CurStepWidthRatio * tDeltaSpeed;
					oViewer.CurStepHeight -= oViewer.CurStepHeightRatio * tDeltaSpeed;
					
					if ( oViewer.CurStepWidth <= SDF_MIN_WIDTH){
						bDirection = true;
						bLoadNextSDF = true;
					}
				}
			}
			else	// PANNING
			{
				if (bDirection)
				{
					oViewer.CurStepY += oViewer.CurStepHeightRatio * tDeltaSpeed * PAN_DOWN_SPEED;
					
					if ( oViewer.CurStepY >= (oViewer.SDFMulti->vDimensions.area.height - OLED::SCREEN_HEIGHT)){
						bDirection = false;
					}
				}
				else
				{
					oViewer.CurStepY -= oViewer.CurStepHeightRatio * tDeltaSpeed * PAN_UP_SPEED;
					
					if ( oViewer.CurStepY <= 0.0f){
						bDirection = true;
						oViewer.CameraState = sSDFViewer::ZOOMING;
					}
				}
				bPanSmoothing = (absolute(((int32_t)oViewer.CurStepY) - ((int32_t)oViewer.Config.yOffset)) > RENDER_PIXELSTEP);
			}
			
			tLastUpdated = tNow;
			
			SignedDistanceFieldParams Config(oViewer.Config);
			
			if ( absolute(((int32_t)oViewer.CurStepWidth) - ((int32_t)Config.outputWidth)) > RENDER_PIXELSTEP || bPanSmoothing )
			{
				Config.outputWidth = __fmaxf(oViewer.CurStepWidth * oViewer.SDFMulti->Scale, (float32_t const)SDF_MIN_WIDTH);
				Config.outputHeight = __fmaxf(oViewer.CurStepHeight * oViewer.SDFMulti->Scale, (float32_t const)SDF_MIN_WIDTH * oViewer.CurStepHeightRatio);
			
				SetVector2(&Config.InverseOutputWidthHeight, 1.0f/(float32_t)Config.outputWidth, 1.0f/(float32_t)Config.outputHeight);
				
				UpdateXOffset(Config);
				UpdateYOffset(Config, bDirection);
				
				oViewer.Config = Config;	 // Save Config
		
				UpdateParallaxOffsets(tNow);
				
				eRenderStatus = RENDER_START; // Start	
				OLED::ClearBuffer_8bit(oViewer.WorkingRenderBuffer);
			}
		} // if (tNow - tLastRenderUpdate > RenderUpdateSDF_Interval)
	} // if ( (RENDER_SLEEPING == eRenderStatus) )
	else if (tNow - tLastFrameRendered > tFrameRenderDeltaTime)
	{		
		uint32_t const uiRenderIndex(eRenderStatus);		// will always be in unsigned range here
		uint8_t const* __restrict pLayerSourceBits
			__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))(nullptr);
			
		if ( unlikely(!CheckJPEGDecompressionState(uiRenderIndex, pLayerSourceBits)) ) {
			HANDLE_JPEG_ERROR;
		}
		
		if (nullptr != pLayerSourceBits )
		{
			// Add the parallax component
			uint32_t const restoreOrigOffset = oViewer.Config.xOffset;
			oViewer.Config.xOffset += oViewer.LayerConfig[uiRenderIndex].vOffset.pt.x;
				
			uint8_t const CurShade = getShadeByIndex(uiRenderIndex);

			oViewer.SDFMulti->RenderSDF_PFUNC( oViewer.WorkingRenderBuffer, pLayerSourceBits, 
																				 CurShade, LastShade, &oViewer.Config );
			
			LastShade = CurShade;
			oViewer.Config.xOffset = restoreOrigOffset;
			
			oViewer.JPEGLayerDecompressed = SDFLayer::COMPRESSED_UNLOADED;	// Layer has been rendered from uncompressed buffer,
																																			// reset flag status for next layer
			if (oViewer.SDFMulti->NumShades == ++eRenderStatus)
			{
				// Done rendering frames spread over time
				eRenderStatus = RENDER_SLEEPING;
				LastShade = 0; // important to reset for next render iteration
				
				// Move Last Next Buffer to Current, update Next buffer to now free buffer
				swap_pointer(oViewer.CurRenderBuffer, oViewer.NextRenderBuffer);
				swap_pointer(oViewer.NextRenderBuffer, oViewer.WorkingRenderBuffer);
				
				oViewer.CacheInvalidated = 0;
			}
			else {	// next layer pending on next pass
				
				uint32_t const uiNextRenderIndex(eRenderStatus);		// will always be in unsigned range here
				// Early start of JPEG decode so its closer to ready on the next layer pass
				BeginNextJPEGDecompression(uiNextRenderIndex);
			}
			tLastFrameRendered = millis();
					
			tFrameRenderDeltaTime = min( ((tLastFrameRendered - tNow)) + (RenderSDF_Balancer>>1), RenderFrameSDF_Interval );
			tNextRenderUpdate = tLastFrameRendered + (RenderSDF_Balancer>>1);
		}
	}	
}

STATIC_INLINE void RenderMultiSDF(uint32_t const tNow)
{	
	static uint32_t tLastInvalidate, tLastLerpRender, tLerpAccumulated;
		
	if ( 0 == oViewer.CacheInvalidated ) {

		// Update timespan for lerp render
		oViewer.DeltaRenderTotal = (tNow - tLastInvalidate);
		
		oViewer.tInvDeltaRenderTotal = 1.0f / oViewer.DeltaRenderTotal;
		oViewer.LerpErrorCorrectionModifier = (float)oViewer.DeltaRenderTotal / (float)tLerpAccumulated;
		
		// the next buffer was just moved to current, render what we have been lerping towards
		OLED::Render8bitScreenLayer_Back(oViewer.CurRenderBuffer);
		oViewer.CacheInvalidated = 1;
		
		tLerpAccumulated = 0;
		tLastLerpRender = tNow; // Reset base
		tLastInvalidate = tNow;
	}
	else
	{
		// Render the current to next interpolated image
		tLerpAccumulated += (tNow - tLastLerpRender);
		
		if ( unlikely(tLerpAccumulated >= oViewer.DeltaRenderTotal)) {

			// Over estimated time for complete lerp, just render finished result( the end of the lerp - nextbuffer)
			OLED::Render8bitScreenLayer_Back(oViewer.NextRenderBuffer);
		}
		else {
		//	static constexpr float32_t const EpsilonLerpClose = 0.01f;
			
			float const tLerp = __fminf(1.0f, ((float32_t)(tLerpAccumulated)) * oViewer.tInvDeltaRenderTotal * oViewer.LerpErrorCorrectionModifier);
			
		//	if ( unlikely(__fabsf(1.0f - tLerp) < EpsilonLerpClose)) {
				// Normal render, no interpolation needed for less than 1% difference
		//		OLED::Render8bitScreenLayer_Back(oViewer.NextRenderBuffer);
		//	}
		//	else {
				OLED::Render8bitScreenLayer_Lerp_Back(oViewer.CurRenderBuffer, oViewer.NextRenderBuffer, tLerp);
		//	}
		}
		tLastLerpRender = tNow;
	}
	
}

void Render(uint32_t const tNow)
{
  RenderMultiSDF(tNow);
}




} // namespace


#endif // skull
