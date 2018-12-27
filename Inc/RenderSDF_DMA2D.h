/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef RENDERSDF_DMA2D
#define RENDERSDF_DMA2D
#include "commonmath.h"
#include "sdf.h"
#include "oled.h"
#include "jpeg.h"

namespace SDF
{
static constexpr uint32_t const _MAXSHADES = 32;		// not using black layer for alll sdf's ** only shadescached uses this **
																										// except ones that are rendered with a alpha mask	// need to keep it at 16 because of this	
static constexpr uint32_t const n16SHADES = 16,
																n32SHADES = 32;
	
typedef struct sSDFLayer													
{
	static constexpr uint32_t const COMPRESSED_UNLOADED = 0,
														      COMPRESSED_PENDING = 1,
														      UNCOMPRESSED_LOADED = 2;
	
	uint8_t const* const __restrict SDF;
	uint32_t const                  Size;
	
	sSDFLayer()
	: SDF(nullptr), Size(0)
	{}
	explicit sSDFLayer( uint8_t const* const __restrict srcSDFBinary )
		: SDF(srcSDFBinary), Size(0)
	{}
	explicit sSDFLayer( uint8_t const* const __restrict srcSDFBinary, uint32_t const sizeInBytes )
		: SDF(srcSDFBinary), Size(sizeInBytes)
	{}
} const SDFLayer;

typedef struct sSDFSource
{    
	uint8_t const* const __restrict Shades;
	
	uint32_t const									NumShades;
	
	Pixels const										vDimensions;
	
	float32_t const									Scale;
	Vector2 const										vFocus;
	
	void (* const RenderSDF_PFUNC)(uint8_t * const __restrict, 
																 uint8_t const * const __restrict, uint8_t const, uint8_t const,
																 SignedDistanceFieldParams const * const __restrict); // solution for dynamic parameter that is defined
																																											// at compile time, template woes
	bool const	IsFRAM;
									
	virtual SDFLayer const& getLayer(uint32_t const uiIndex) const = 0;	// pure virtual overridden by derived classes 
																 
	explicit sSDFSource( uint8_t const* const& uiShades, uint32_t const& uiNumShades, 
											 Pixels const& vDim, float32_t const& fScale, Vector2 const& vFocusPt, 
											 void (* const PFUNC)(uint8_t * const __restrict, 
												                    uint8_t const * const __restrict, uint8_t const, uint8_t const,
																            SignedDistanceFieldParams const * const __restrict),
											  bool const& bFRAM)
	: Shades(uiShades), NumShades(uiNumShades), vDimensions(vDim), Scale(fScale), vFocus(vFocusPt), RenderSDF_PFUNC(PFUNC),
		IsFRAM(bFRAM)
	{
	}
	
	virtual ~sSDFSource() = default;
	
} const SDFSource;

typedef struct sSDFSource16 : SDFSource
{  
	SDFLayer const				Layer[SDF::n16SHADES];
	
	virtual SDFLayer const& getLayer(uint32_t const uiIndex) const { return(Layer[uiIndex]); }
	
	
	explicit sSDFSource16( SDFLayer const (&&aLayer)[SDF::n16SHADES], uint8_t const* const uiShades, uint32_t const uiNumShades, 
												 Pixels const vDim, float32_t const fScale, Vector2 const vFocusPt, 
												 void (* const PFUNC)(uint8_t * const __restrict, 
												                      uint8_t const * const __restrict, uint8_t const, uint8_t const,
																              SignedDistanceFieldParams const * const __restrict),
												  bool const bFRAM)
	: sSDFSource(uiShades, uiNumShades, vDim, fScale, vFocusPt, PFUNC, bFRAM), Layer{aLayer[0],  aLayer[1],  aLayer[2],  aLayer[3],
																																									 aLayer[4],  aLayer[5],  aLayer[6],  aLayer[7],
																																									 aLayer[8],  aLayer[9],  aLayer[10], aLayer[11],
																																									 aLayer[12], aLayer[13], aLayer[14], aLayer[15]}
	{	}
	
	virtual ~sSDFSource16() = default;
	
} const SDFSource16;

typedef struct sSDFSource32 : SDFSource
{  
	SDFLayer const				Layer[SDF::n32SHADES];
	
	virtual SDFLayer const& getLayer(uint32_t const uiIndex) const { return(Layer[uiIndex]); }
	
	
	explicit sSDFSource32( SDFLayer const (&&aLayer)[SDF::n32SHADES], uint8_t const* const uiShades, uint32_t const uiNumShades, 
												 Pixels const vDim, float32_t const fScale, Vector2 const vFocusPt, 
												 void (* const PFUNC)(uint8_t * const __restrict, 
												                      uint8_t const * const __restrict, uint8_t const, uint8_t const,
																              SignedDistanceFieldParams const * const __restrict),
												  bool const bFRAM)
	: sSDFSource(uiShades, uiNumShades, vDim, fScale, vFocusPt, PFUNC, bFRAM), Layer{aLayer[0],  aLayer[1],  aLayer[2],  aLayer[3],
																																									 aLayer[4],  aLayer[5],  aLayer[6],  aLayer[7],
																																									 aLayer[8],  aLayer[9],  aLayer[10], aLayer[11],
																																									 aLayer[12], aLayer[13], aLayer[14], aLayer[15],
																																									 aLayer[16], aLayer[17], aLayer[18], aLayer[19],
																																									 aLayer[20], aLayer[21], aLayer[22], aLayer[23],
																																									 aLayer[24], aLayer[25], aLayer[26], aLayer[27],
																																									 aLayer[28], aLayer[29], aLayer[30], aLayer[31]}
	{	}
	
	virtual ~sSDFSource32() = default;
	
} const SDFSource32;

typedef struct sSDFDynamicLayerParams
{
	Vector2 				vOffset;
	uint8_t					Opacity;
	
	sSDFDynamicLayerParams()
	: Opacity(0xFF)
	{}
		
} SDFDynamicLayerParams;

typedef struct sSDFPersistentState
{
	static constexpr uint32_t const UNLOADED = 2,
														      PENDING = 1,
														      RENDERED = 0;
	
	SignedDistanceFieldParams 	Config;
	SDFDynamicLayerParams*			LayerConfig;
	
	uint32_t CurRenderIndex;
					 	
	uint32_t State,
					 LayerState;
	
	uint32_t idJPEG;
	
	uint8_t LastShade;
	uint8_t Opacity;
	
	sSDFPersistentState(uint32_t const NumLayers)
	: State(UNLOADED), LayerState(SDFLayer::COMPRESSED_UNLOADED), CurRenderIndex(0), LastShade(0), Opacity(0xFF), idJPEG(0),
	  LayerConfig(nullptr)
	{
		LayerConfig = new SDFDynamicLayerParams[NumLayers];
	}
	
	~sSDFPersistentState()
	{
		SAFE_DELETE_ARRAY(LayerConfig);
	}
		
} SDFPersistentState;

extern uint8_t * const __restrict getAlphaMaskFrameBuffer();

namespace SDFPrivate
{
//extern uint8_t * __restrict SourceDeferredBuffer, * __restrict TargetDeferredBuffer;
extern uint8_t const getShadeByIndex( uint32_t const iDx, SDFSource const* const __restrict pSDFSource );
extern void RenderLayer_AlphaMask( SDFPersistentState const* const __restrict pState, 
																	 uint8_t const* const __restrict pLayerSourceBits);
extern void RenderLayer( uint32_t const uiRenderIndex, uint8_t& __restrict LastShade, SDFPersistentState const* const __restrict pState, SDFSource const* const __restrict pSDFSource, 
												 uint8_t const* const __restrict pLayerSourceBits);
extern void BeginNextJPEGDecompression(uint32_t const uiRenderIndex, SDFPersistentState* const __restrict pState,
																		   SDFSource const* const __restrict pSDFSource);
extern void RenderToActiveFrameBuffer_AlphaMask( uint8_t const Opacity );
extern void RenderToActiveFrameBuffer();

}

template <bool const LayerZeroUsedAsAlphaMask>
bool const RenderSDF_DMA2D( SDFPersistentState* const __restrict pState, sSDFSource const* const __restrict& __restrict pSDFSource )
{
	bool bRendering(false);
	
	if ( SDFPersistentState::RENDERED == pState->State )
	{
		if (!LayerZeroUsedAsAlphaMask)
			SDFPrivate::RenderToActiveFrameBuffer();
		else
			SDFPrivate::RenderToActiveFrameBuffer_AlphaMask(pState->Opacity);
		
		bRendering = true;
	}
	else
	{
		if ( SDFPersistentState::UNLOADED == pState->State )
		{
			if (LayerZeroUsedAsAlphaMask) {
				pState->CurRenderIndex = 0;	// Use Black Layer as Alpha Mask
				OLED::ClearBuffer_8bit(getAlphaMaskFrameBuffer());
			}
			else {
				pState->CurRenderIndex = 1;	// skip black layer for there is no usage in this RenderSDF DMA2D
			}
			pState->LayerState = SDFLayer::COMPRESSED_UNLOADED;
			pState->State = SDFPersistentState::PENDING;
			pState->LastShade = 0;
			OLED::ClearBuffer_8bit(OLED::getDeferredFrameBuffer());
		}
		
		uint32_t const uiRenderIndex(pState->CurRenderIndex);
		
		if ( uiRenderIndex < pSDFSource->NumShades )
		{
			switch(pState->LayerState)
			{
				case SDFLayer::COMPRESSED_UNLOADED:
					SDFPrivate::BeginNextJPEGDecompression(uiRenderIndex, pState, pSDFSource);
					break;
														 
				case SDFLayer::COMPRESSED_PENDING:
				{
					int32_t const JPEGStatus = JPEGDecoder::IsReady(pState->idJPEG);
					switch(JPEGStatus)
					{
						case JPEGDecoder::READY:
							pState->LayerState = SDFLayer::UNCOMPRESSED_LOADED;
						  goto JPEGReady; // Must use decompressed JPEG immediately, before another potential owner request a new decode
							
						case JPEGDecoder::TIMED_OUT:
							pState->LayerState = SDFLayer::COMPRESSED_UNLOADED;								
							return(false);
						default:
							break; // Busy / Pending
					}
					break;
				} // end case PENDING
				case SDFLayer::UNCOMPRESSED_LOADED:
JPEGReady:			
					// Reset for next potential layer 
					pState->LayerState = SDFLayer::COMPRESSED_UNLOADED;
					
					if (LayerZeroUsedAsAlphaMask) { // compile-time constexpr
						if (0 == uiRenderIndex) { // runtime check
							// Build Alpha Mask
							SDFPrivate::RenderLayer_AlphaMask(pState, JPEGDecoder::getDecompressionBuffer()); // now done layer, decompression buffer was used and now free
							OLED::ClearBuffer_8bit(OLED::getDeferredFrameBuffer());
						}
						else {
							SDFPrivate::RenderLayer(uiRenderIndex, pState->LastShade, pState, pSDFSource, JPEGDecoder::getDecompressionBuffer()); // now done layer, decompression buffer was used and now free
						}
					}
					else {
						SDFPrivate::RenderLayer(uiRenderIndex, pState->LastShade, pState, pSDFSource, JPEGDecoder::getDecompressionBuffer()); // now done layer, decompression buffer was used and now free
					}
					
					if ( ++pState->CurRenderIndex == pSDFSource->NumShades )
					{
						pState->State = SDFPersistentState::RENDERED;
						if (!LayerZeroUsedAsAlphaMask)
							SDFPrivate::RenderToActiveFrameBuffer();
						else
							SDFPrivate::RenderToActiveFrameBuffer_AlphaMask(pState->Opacity);
						
						bRendering = true;
					}
					else
					{
						// Start Next Layers JPEG Early if possible
						SDFPrivate::BeginNextJPEGDecompression(uiRenderIndex, pState, pSDFSource);;
					}		
				default:
					break;
			}
		} // switch
	} // end else (NOT RENDERED)
	
	return(bRendering);
}

} // endnamespace
#endif

