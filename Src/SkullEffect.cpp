/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#include "globals.h"

#ifdef ENABLE_SKULL

#include "skulleffect.h"
#include "RenderSDF_DMA2D.h"
//#include "FLASH\\SDF_SKULL_Header_JPEG.h"
#include "oled.h"

#include "externSDFMulti.h"

using namespace SDF;

/*
extern const unsigned char SDF_Skull_0[];
extern const unsigned char SDF_Skull_1[];
extern const unsigned char SDF_Skull_2[];
extern const unsigned char SDF_Skull_3[];
extern const unsigned char SDF_Skull_4[];
extern const unsigned char SDF_Skull_5[];
extern const unsigned char SDF_Skull_6[];
extern const unsigned char SDF_Skull_7[];

extern const unsigned char SDF_Skull_8[];
extern const unsigned char SDF_Skull_9[];
extern const unsigned char SDF_Skull_10[];
extern const unsigned char SDF_Skull_11[];
extern const unsigned char SDF_Skull_12[];
extern const unsigned char SDF_Skull_13[];
extern const unsigned char SDF_Skull_14[];
extern const unsigned char SDF_Skull_15[];

extern const unsigned int SDF_Skull_0_Size;
extern const unsigned int SDF_Skull_1_Size;
extern const unsigned int SDF_Skull_2_Size;
extern const unsigned int SDF_Skull_3_Size;
extern const unsigned int SDF_Skull_4_Size;
extern const unsigned int SDF_Skull_5_Size;
extern const unsigned int SDF_Skull_6_Size;
extern const unsigned int SDF_Skull_7_Size;
extern const unsigned int SDF_Skull_8_Size;
extern const unsigned int SDF_Skull_9_Size;
extern const unsigned int SDF_Skull_10_Size;
extern const unsigned int SDF_Skull_11_Size;
extern const unsigned int SDF_Skull_12_Size;
extern const unsigned int SDF_Skull_13_Size;
extern const unsigned int SDF_Skull_14_Size;
extern const unsigned int SDF_Skull_15_Size;
*/

namespace SkullEffect
{

	/*
static  constexpr uint32_t const  SDF_Skull_WIDTH = SDF_Skull_Header::ORIGWIDTH,
																  SDF_Skull_HEIGHT = SDF_Skull_Header::ORIGHEIGHT,
																	SDF_Skull_FOCUS_X = (SDF_Skull_WIDTH>>1),
																  SDF_Skull_FOCUS_Y = (SDF_Skull_HEIGHT>>1) - 300;
static  constexpr float32_t const SDF_Skull_SCALE = 0.9f;
	*/
	
static  constexpr uint32_t const  SDF_13_WIDTH = SDF_13_Header::ORIGWIDTH,
																  SDF_13_HEIGHT = SDF_13_Header::ORIGHEIGHT,
																  SDF_13_FOCUS_X = (SDF_13_WIDTH>>1),
																  SDF_13_FOCUS_Y = (SDF_13_HEIGHT>>1) - 100;
static  constexpr float32_t const SDF_13_SCALE = 2.3f;

static struct sSkullDesc
{
	static constexpr uint32_t const ZOOM_0 = 0,
																	ZOOM_1 = 1,
																	ZOOM_2 = 2,
																	ZOOM_COMPLETE = 3;
	
	static constexpr uint32_t const ZOOM_INTERVAL = 8500; // ms
	
	static SDFSource const* const oSDF;
	
	SDFPersistentState State;
	
	uint32_t ZoomLevelState;
	
	uint32_t tLastZoomStateChange;
	
	bool	bDeltaZoomStateChange,
				bHasRendered_CurZoomLevel;
	
	sSkullDesc()
	: State(SDF::n16SHADES), ZoomLevelState(ZOOM_0), bDeltaZoomStateChange(true), bHasRendered_CurZoomLevel(false)
	{}
		
} oSkullDesc;

SDFSource const* const sSkullDesc::oSDF =
	/*
                    new SDFSource16({sSDFLayer(SDF_Skull_0,SDF_Skull_0_Size),sSDFLayer(SDF_Skull_1,SDF_Skull_1_Size),sSDFLayer(SDF_Skull_2,SDF_Skull_2_Size),sSDFLayer(SDF_Skull_3,SDF_Skull_3_Size),sSDFLayer(SDF_Skull_4,SDF_Skull_4_Size),
											sSDFLayer(SDF_Skull_5,SDF_Skull_5_Size),sSDFLayer(SDF_Skull_6,SDF_Skull_6_Size),sSDFLayer(SDF_Skull_7,SDF_Skull_7_Size),sSDFLayer(SDF_Skull_8,SDF_Skull_8_Size),sSDFLayer(SDF_Skull_9, SDF_Skull_9_Size),
											sSDFLayer(SDF_Skull_10,SDF_Skull_10_Size),sSDFLayer(SDF_Skull_11,SDF_Skull_11_Size),sSDFLayer(SDF_Skull_12,SDF_Skull_12_Size),sSDFLayer(SDF_Skull_13,SDF_Skull_13_Size),
										  sSDFLayer(SDF_Skull_14,SDF_Skull_14_Size),sSDFLayer(SDF_Skull_15, SDF_Skull_15_Size)},
										  SDF_Skull_Header::SHADES, SDF_Skull_Header::NUMSHADES,
											Pixels(SDF_Skull_WIDTH, SDF_Skull_HEIGHT), SDF_Skull_SCALE,
											uVector2( (float)SDF_Skull_FOCUS_X / (float)SDF_Skull_WIDTH, (float)SDF_Skull_FOCUS_Y / (float)SDF_Skull_HEIGHT ),
											nullptr,
											false);
	*/
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
											false);
	
											
void Reset(uint32_t const tNow)
{
	oSkullDesc.bDeltaZoomStateChange = true;
	oSkullDesc.bHasRendered_CurZoomLevel = false;
	oSkullDesc.tLastZoomStateChange = tNow;
	oSkullDesc.ZoomLevelState = sSkullDesc::ZOOM_0;
	oSkullDesc.State.State = SDFPersistentState::UNLOADED;
}

bool const Render(uint32_t const tNow)
{
	static uint32_t tLastUpdate,
									xOffset(0);

	if (oSkullDesc.bHasRendered_CurZoomLevel && tNow - oSkullDesc.tLastZoomStateChange > sSkullDesc::ZOOM_INTERVAL)
	{
		if (sSkullDesc::ZOOM_COMPLETE != oSkullDesc.ZoomLevelState ) // Don't Change until manual reset
		{
			++oSkullDesc.ZoomLevelState;

			oSkullDesc.bDeltaZoomStateChange = true;
			oSkullDesc.tLastZoomStateChange = tNow;
		}
		
		// else : Already rendered at final zoom level, no change
	}
	
	if ( oSkullDesc.bDeltaZoomStateChange )
	{
		// Config Signed Distance Field
		SignedDistanceFieldParams Config;
		float const fHeightRatio = (float32_t)oSkullDesc.oSDF->vDimensions.area.height /(float32_t)oSkullDesc.oSDF->vDimensions.area.width;
			  
		float yOffset(0.0f);
		switch(oSkullDesc.ZoomLevelState)
		{
			case sSkullDesc::ZOOM_0:
				yOffset = 0.8f;
				Config.outputWidth = 256;
				oSkullDesc.State.Opacity = Constants::OPACITY_100 - 1;
				break;
			case sSkullDesc::ZOOM_1:
				yOffset = 0.97f;
				Config.outputWidth = 256;
				oSkullDesc.State.Opacity = Constants::OPACITY_100 - 1;
				break;
			case sSkullDesc::ZOOM_2:
				yOffset = 1.5f;
				Config.outputWidth = 256;
				oSkullDesc.State.Opacity = Constants::OPACITY_100 - 1;
				break;
			case sSkullDesc::ZOOM_COMPLETE:
			default:
				if ( oSkullDesc.bHasRendered_CurZoomLevel )
					return(true); // done
		}
		
		Config.outputHeight = fHeightRatio * (float32_t)Config.outputWidth;
		
		Config.xOffset = 0;//(float32_t)Config.outputHeight * oSkullDesc.oSDF.vFocus.pt.x;
		Config.yOffset = (float32_t)Config.outputHeight * yOffset * oSkullDesc.oSDF->vFocus.pt.y;
		SetVector2(&Config.InverseOutputWidthHeight, 1.0f/(float32_t)Config.outputWidth, 1.0f/(float32_t)Config.outputHeight);
			
		oSkullDesc.State.Config = Config;
		oSkullDesc.bDeltaZoomStateChange = false;
		oSkullDesc.State.State = SDFPersistentState::UNLOADED;
		oSkullDesc.bHasRendered_CurZoomLevel = (sSkullDesc::ZOOM_COMPLETE == oSkullDesc.ZoomLevelState);	//just in case failsafe
	}
	
	//OLED::FillFrontBuffer(0x3f, 0x00);
	oSkullDesc.bHasRendered_CurZoomLevel = RenderSDF_DMA2D<true>( &oSkullDesc.State, oSkullDesc.oSDF );
	
	return(false);
}


} //endnamespace

#endif


