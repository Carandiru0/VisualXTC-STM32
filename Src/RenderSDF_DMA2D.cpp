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
#include "RenderSDF_DMA2D.h"
#include "DTCM_Reserve.h"
#include "DMA2D.hpp"

namespace SDF
{

namespace SDFPrivate
{
//uint8_t * __restrict SourceDeferredBuffer(nullptr), * __restrict TargetDeferredBuffer(nullptr);

uint8_t const getShadeByIndex( uint32_t const iDx, SDFSource const* const __restrict pSDFSource )
{
	return(pSDFSource->Shades[iDx]);
}

void RenderLayer_AlphaMask( SDFPersistentState const* const __restrict pState, 
														uint8_t const* const __restrict pLayerSourceBits)
{
	/*
	// Must invert the sdf rendering for layer 0 to get proper mask
	RenderSignedDistanceField_DMA2D<Orient_Normal, true>(OLED::getDeferredFrameBuffer(), pLayerSourceBits, &pState->Config);
	
	// Building Alpha Mask
	OLED::Render8bitScreenLayer_Blended(OLED::getDeferredFrameBuffer(), getAlphaMaskFrameBuffer(), pState->Opacity, 0xFF); 
	*/
}
void RenderLayer( uint32_t const uiRenderIndex, uint8_t& __restrict LastShade, SDFPersistentState const* const __restrict pState, SDFSource const* const __restrict pSDFSource, 
									uint8_t const* const __restrict pLayerSourceBits)
{
	uint8_t const CurShade = getShadeByIndex(uiRenderIndex, pSDFSource);
	
	//RenderSignedDistanceField_DMA2D(SourceDeferredBuffer, pLayerSourceBits, &pState->Config);
	RenderSignedDistanceField(OLED::getDeferredFrameBuffer(), pLayerSourceBits, CurShade, LastShade, &pState->Config);
	
	//OLED::Render8bitScreenLayer_Blended(SourceDeferredBuffer, TargetDeferredBuffer, 0xFF, CurShade); 
	
	LastShade = CurShade;
}

void BeginNextJPEGDecompression(uint32_t const uiRenderIndex, SDFPersistentState* const __restrict pState,
																SDFSource const* const __restrict pSDFSource)
{
	uint32_t const idJPEGUnique = JPEGDecoder::Start_Decode( pSDFSource->getLayer(uiRenderIndex).SDF,
																													 pSDFSource->getLayer(uiRenderIndex).Size, pSDFSource->IsFRAM );
	
	if ( 0 != idJPEGUnique ) {
		pState->idJPEG = idJPEGUnique;
		pState->LayerState = SDFLayer::COMPRESSED_PENDING;			
	}
	//else {
		// Not Ready, ignore and try on next pass
	//}															
}

void RenderToActiveFrameBuffer_AlphaMask( uint8_t const Opacity )
{
	/*
	// Use mask to prep back layer with clearing the pixels for the mask
	OLED::Render8bitScreenLayer_Blended(getAlphaMaskFrameBuffer(), (uint8_t* const __restrict)DTCM::getBackBuffer(), Opacity, 0x00);
	// then it is opaque on front layer
	// alpha can be modulated by modifying the original mask alpha
	// as it is also anded with the shade in the next render func here.
	if ( 0xFF == Opacity ) {
		// for fully opaque could use this:
		OLED::Render8bitScreenLayer_Front(OLED::getDeferredFrameBuffer());
	}
	else {
		OLED::Render8bitScreenLayer_AlphaMask_Front(OLED::getDeferredFrameBuffer(), getAlphaMaskFrameBuffer() );
	}
	*/
	OLED::Render8bitScreenLayer_Back(OLED::getDeferredFrameBuffer());
}
void RenderToActiveFrameBuffer()
{
	// Opaque
	OLED::Render8bitScreenLayer_Back(OLED::getDeferredFrameBuffer());
} 
} // end namespace

uint8_t * const __restrict getAlphaMaskFrameBuffer() {
	return( xDMA2D::getEffectBuffer_8bit() );
}

// todo use layer 0 as alpha mask

} //endnamespace

