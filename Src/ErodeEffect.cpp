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
#include "ErodeEffect.h"
	
sErodeEffect::sErodeEffect()
	: TransitionRenderState(ANIM_UNLOADED), tLastErode(0), tNextErode(0),
	FrontOrBackBuffer(OLED::BACK_BUFFER), Opacity(Constants::OPACITY_100),
	WorkingRenderBuffer(OLED::getEffectFrameBuffer_2_HalfSize()),								// BROKOEN ERODE with half size
	NextRenderBuffer(OLED::getEffectFrameBuffer_1()),
	CurRenderBuffer(OLED::getEffectFrameBuffer_0())
{
}

void sErodeEffect::InitializeEffectBuffers( uint8_t const * const __restrict srcBufferL8 )
{
	// Ensure CurBuffer & Working framebuffers are equal
			// NextBuffer is eroded + blur
			// WorkingBuffer will be the eroded buffer
			// Curbuffer remains original
	
	OLED::RenderCopy8bitScreenLayer(CurRenderBuffer, srcBufferL8);
	OLED::RenderCopy8bitScreenLayer(WorkingRenderBuffer, CurRenderBuffer);
	OLED::ClearBuffer_8bit(NextRenderBuffer);
	
	tNextErode = 0;
}
void sErodeEffect::Reset()
{
	FrontOrBackBuffer = OLED::BACK_BUFFER;
	TransitionRenderState = ANIM_UNLOADED;
	tLastErode = 0; tNextErode = 0;
	Opacity = Constants::OPACITY_100;
}

uint32_t const sErodeEffect::Erode()
{
	return(OLED::Effects::Erode_8bit(WorkingRenderBuffer));
}
void sErodeEffect::Blur( uint32_t const& Radius )
{
	OLED::Effects::GaussianBlur_8bit(NextRenderBuffer, WorkingRenderBuffer, Radius);
}
					
void sErodeEffect::Render( uint32_t const tNow )
{
	if ( likely(0 != tNextErode) ) // avoid division by zero
	{
		float const tLerp = clampf( (float const)tNow / (float const)tNextErode );
		
		if (OLED::BACK_BUFFER == FrontOrBackBuffer)
			OLED::Render8bitScreenLayer_Lerp_Back(CurRenderBuffer, NextRenderBuffer, tLerp);
		else
			OLED::Render8bitScreenLayer_Lerp_Front(CurRenderBuffer, NextRenderBuffer, tLerp, Opacity);
	}
}

