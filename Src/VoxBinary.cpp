/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

The VOX File format is Copyright to their respectful owners.

 */

#include "globals.h"
#define VOX_IMPL
#include "VoxBinary.h"
#include "IsoVoxel.h"
#include "voxelModelsFRAM.h"

#ifdef VOX_DEBUG_ENABLED
#include "debug.cpp"
#endif

//std::vector<Volumetric::voxB::voxelDescPacked>	Volumetric::voxB::voxelModelBase::Voxels;

/*
1x4      | char       | chunk id
4        | int        | num bytes of chunk content (N)
4        | int        | num bytes of children chunks (M)

N        |            | chunk content

M        |            | children chunks
*/
typedef struct ChunkHeader
{
	char							 					id[4];
	int32_t								   numbytes;
	int32_t				   numbyteschildren;
	
	ChunkHeader() 
	: id{'0','0','0','0'},
		numbytes(0), numbyteschildren(0)
	{}
		
} ChunkHeader;

/*
5. Chunk id 'SIZE' : model size
-------------------------------------------------------------------------------
# Bytes  | Type       | Value
-------------------------------------------------------------------------------
4        | int        | size x
4        | int        | size y
4        | int        | size z : gravity direction
-------------------------------------------------------------------------------
*/
typedef struct ChunkDimensions : ChunkHeader
{
	int32_t 		Width,
							Depth,
							Height;
	
	ChunkDimensions()
	: Width(0), Depth(0), Height(0)
	{}
		
} ChunkDimensions;

/*
6. Chunk id 'XYZI' : model voxels
-------------------------------------------------------------------------------
# Bytes  | Type       | Value
-------------------------------------------------------------------------------
4        | int        | numVoxels (N)
4 x N    | int        | (x, y, z, colorIndex) : 1 byte for each component
-------------------------------------------------------------------------------
*/
typedef struct VoxelData
{
	uint8_t			x, y, z, shadeIndex;
} VoxelData;

typedef struct ChunkVoxels : ChunkHeader
{
	int32_t 								numVoxels;
	
} ChunkVoxels;

namespace Volumetric
{
namespace voxB
{

STATIC_INLINE void ReadData( void* const __restrict DestStruct, uint8_t const* pReadPointer, uint32_t const SizeOfDestStruct )
{
	memcpy8((uint8_t* __restrict)DestStruct, pReadPointer, SizeOfDestStruct);
}

STATIC_INLINE bool const CompareTag( uint32_t const TagSz, uint8_t const * __restrict pReadPointer, char const* const& __restrict szTag )
{
	int32_t iDx = TagSz - 1;
	
	// push read pointer from beginning to end instead for start, makes life easier in loop
	pReadPointer = pReadPointer + iDx;
	do
	{
		
		if ( szTag[iDx] != *pReadPointer-- )
			return(false);
			
	} while (--iDx >= 0);
	
	return(true);
}

// simple (slow) linear search
static bool const BuildAdjacency_ForDynamic( uint32_t numVoxels, VoxelData const& __restrict source, VoxelData const* __restrict pVoxels, uint8_t& __restrict Adjacency )
{
	static constexpr uint32_t const uiMaxOcculusion( 9 + 8 ); // 9 above, 8 sides
	
	uint8_t pendingAdjacency(0);
	Adjacency = 0;
	
	uint32_t uiOcculusion(uiMaxOcculusion - 1);
	
	// only remove voxels that are completely surrounded above and too the sides (don't care about below)
	do
	{
		VoxelData const Compare( *pVoxels++ );
		
		int32_t const HeightDelta = ((int32_t)Compare.z - (int32_t)source.z);
		
		if ( (0 == HeightDelta) | (1 == HeightDelta) ) { // same height or 1 unit above only
			
			int32_t const SXDelta = (int32_t)Compare.x - (int32_t)source.x,
										SYDelta = (int32_t)Compare.y - (int32_t)source.y;
			int32_t const XDelta = absolute(SXDelta),
										YDelta = absolute(SYDelta);
			
			if ( (XDelta | YDelta) <= 1 ) // within 1 unit or same on x,y axis'	
			{
				if ( 0 == (HeightDelta | XDelta | YDelta) ) // same voxel ?
					continue;
				
					if ( (bool)HeightDelta ) { // 1 unit above...
						if ( 0 == (XDelta | YDelta) ) { // same x,y
							pendingAdjacency |= BIT_ADJ_ABOVE;
						}
					}
					
					if ( 0 != YDelta ) {
						if ( 0 == (HeightDelta | XDelta) ) { // same slice and x axis
							if ( SYDelta < 0 ) { // 1 unit infront...
								pendingAdjacency |= BIT_ADJ_FRONT; 
							}
							else /*if ( 1 == SYDelta )*/ { // 1 unit behind...
								pendingAdjacency |= BIT_ADJ_BACK; 
							}
						}
					}
					
					if ( 0 != XDelta ) {
						if ( 0 == (HeightDelta | YDelta) ) { // same slice and y axis
							if ( SXDelta < 0 ) { // 1 unit left...
								pendingAdjacency |= BIT_ADJ_LEFT; 
							}
							else /*if ( 1 == SXDelta )*/ { // 1 unit right...
								pendingAdjacency |= BIT_ADJ_RIGHT; 
							}
						}
					}
					--uiOcculusion; // Fully remove
			}
		}
			
	} while ( 0 != --numVoxels && 0 != uiOcculusion);
	
	Adjacency = pendingAdjacency;	// face culling used for voxels that are not removed
	
	return(0 != uiOcculusion);
}
// simple (slow) linear search
static bool const BuildAdjacency_ForStatic( uint32_t numVoxels, VoxelData const& __restrict source, VoxelData const* __restrict pVoxels, uint8_t& __restrict Adjacency )
{
	uint8_t pendingAdjacency(0);
	Adjacency = 0;
	
	// skip if root voxel at 0,0,0 always visible
	if ( 0 == (source.x | source.y | source.z) )
		return(true);
	
	do
	{
		VoxelData const Compare( *pVoxels++ );
		
		// BIT_ADJ_ABOVEFRONTLEFT = case where voxel would be completely occulded
		if ( Compare.x == source.x + 1 && Compare.y == source.y - 1 && Compare.z == source.z - 1) {
			return(false); // "out"
		}
		
		if ( !(BIT_ADJ_ABOVEFRONT & pendingAdjacency) ) {
			
			if ( Compare.y == source.y - 1 && Compare.z == source.z - 1 ) {
				if ( Compare.x == source.x ) {
					pendingAdjacency |= BIT_ADJ_ABOVEFRONT;
					continue;	// avoid possible duplicates
				}
			}
		}
		if ( !(BIT_ADJ_ABOVELEFT & pendingAdjacency) ) {
			
			if ( Compare.x == source.x + 1 && Compare.z == source.z - 1 ) {
				if ( Compare.y == source.y ) {
					pendingAdjacency |= BIT_ADJ_ABOVELEFT;
					continue; // avoid possible duplicates
				}
			}
		}
		if ( !(BIT_ADJ_FRONTLEFT & pendingAdjacency) ) {
			
			if ( Compare.x == source.x + 1 && Compare.y == source.y - 1 ) {
				if ( Compare.z == source.z ) {
					pendingAdjacency |= BIT_ADJ_FRONTLEFT;
					continue; // avoid possible duplicates
				}
			}
		}
		
		if ( !(BIT_ADJ_ABOVE & pendingAdjacency) ) {
			
			if ( Compare.z == source.z - 1 ) {
				if ( Compare.x == source.x && Compare.y == source.y ) {
					pendingAdjacency |= BIT_ADJ_ABOVE;
					continue; // avoid possible duplicates
				}
			}
		}
		if ( !(BIT_ADJ_FRONT & pendingAdjacency) ) {
			
			if ( Compare.y == source.y - 1 ) {
				if ( Compare.x == source.x && Compare.z == source.z ) {
					pendingAdjacency |= BIT_ADJ_FRONT;
					continue; // avoid possible duplicates
				}
			}
		}
		if ( !(BIT_ADJ_LEFT & pendingAdjacency) ) {
			
			if ( Compare.x == source.x + 1 ) {
				if ( Compare.y == source.y && Compare.z == source.z ) {
					pendingAdjacency |= BIT_ADJ_LEFT;
					continue; // avoid possible duplicates
				}
			}
		}
		
	} while ( 0 != --numVoxels );
	
	// cull whole voxel cases
	uint32_t testCase = (BIT_ADJ_ABOVEFRONT | BIT_ADJ_ABOVELEFT | BIT_ADJ_FRONT | BIT_ADJ_LEFT);
	if ( (testCase & pendingAdjacency) == testCase ) {
		return(false);
	}
	
	testCase = (BIT_ADJ_ABOVEFRONT | BIT_ADJ_ABOVELEFT | BIT_ADJ_FRONTLEFT);
	if ( (testCase & pendingAdjacency) == testCase ) {
		return(false);
	}
	
	testCase = (BIT_ADJ_ABOVE | BIT_ADJ_FRONT | BIT_ADJ_LEFT);
	if ( (testCase & pendingAdjacency) == testCase ) {
		return(false);
	}
	
	// Voxel is not completely occulded
	Adjacency = pendingAdjacency;
	
	return(true);		// source voxel is still "in"
}
// builds the voxel model, loading from magicavoxel .vox format, returning the model with the voxel traversal
// supporting 128x64x64 size voxel model.
NOINLINE bool const Load( voxelModelBase* const __restrict pDestMem, uint8_t const * const pSourceVoxBinaryData, uint8_t*& __restrict FRAMWritePointer )
{
	typedef bool const (* const adjacency_culling)(uint32_t, VoxelData const& __restrict, VoxelData const* __restrict, uint8_t& __restrict );
	adjacency_culling CullVoxels( pDestMem->isDynamic_ ? &BuildAdjacency_ForDynamic : &BuildAdjacency_ForStatic );
	
	uint8_t const * pReadPointer(nullptr);
	
	// Check Header
	pReadPointer = pSourceVoxBinaryData /*+ 0*/;

	static constexpr uint32_t const  OFFSET_MAIN_CHUNK = 8;				// n bytes to structures
	static constexpr uint32_t const  TAG_LN = 4;
	static constexpr char const      TAG_VOX[TAG_LN] 					= { 'V', 'O', 'X', ' ' },
														       TAG_MAIN[TAG_LN] 				= { 'M', 'A', 'I', 'N' },
														       TAG_DIMENSIONS[TAG_LN] 	= { 'S', 'I', 'Z', 'E' },
														       TAG_XYZI[TAG_LN] 				= { 'X', 'Y', 'Z', 'I' },
																	 TAG_PALETTE[TAG_LN]			= { 'R', 'G', 'B', 'A' };			
	
	if (CompareTag(countof(TAG_VOX), pReadPointer, TAG_VOX)) {
		// skip version #
		
		// read MAIN Chunk
		ChunkHeader const rootChunk;
		
		pReadPointer += OFFSET_MAIN_CHUNK;
		ReadData((void* const __restrict)&rootChunk, pReadPointer, sizeof(rootChunk));
		
		if (CompareTag(countof(TAG_MAIN), (uint8_t const* const)rootChunk.id, TAG_MAIN)) { // if we have a valid structure for root chunk
			//TAG_MAIN	
			// bypass PACK Chunk (only using single models //
			
			// ChunkData
			if ( rootChunk.numbyteschildren > 0 ) {
				
				ChunkDimensions const sizeChunk;
				pReadPointer += sizeof(rootChunk);		// move to expected "SIZE" chunk
				ReadData((void* const __restrict)&sizeChunk, pReadPointer, sizeof(sizeChunk));
				
				if (CompareTag(countof(TAG_DIMENSIONS), (uint8_t const* const)sizeChunk.id, TAG_DIMENSIONS)) { // if we have a valid structure for size chunk
					
					ChunkVoxels voxelsChunk;
					pReadPointer += sizeof(sizeChunk);	// move to expected "XYI" chunk
					ReadData((void* const __restrict)&voxelsChunk, pReadPointer, sizeof(voxelsChunk));
					
					if (CompareTag(countof(TAG_XYZI), (uint8_t const* const)voxelsChunk.id, TAG_XYZI)) { // if we have a valid structure for xyzi chunk
					
						pReadPointer += sizeof(voxelsChunk);	// move to expected voxel array data
						
						// Use Target Memory for loading
						if ( (sizeChunk.Width <= MAX_DIMENSION_X) && (sizeChunk.Depth <= MAX_DIMENSION_YZ) && (sizeChunk.Height <= MAX_DIMENSION_YZ) && voxelsChunk.numVoxels > 0) { // ensure less than maximum dimension suported
							
							uint32_t const numVoxels = voxelsChunk.numVoxels;
														
							// input linear access array
							VoxelData const* const pVoxelRoot = reinterpret_cast<VoxelData const* const>(pReadPointer);
	
							// output linear access array
							if ( numVoxels * sizeof(voxelDescPacked) < (Heap_Size>>1) ) // only reserve for models with a potential maximum less than Heap_Size
								pDestMem->VoxelsTemp.reserve( numVoxels );											// otherwise best effort load the large voxel model
																																					  // hoping to allocate enough heap memory from actual voxels used
							// load all voxels																					  // size = (numVoxels - voxelsCulled) * [sizeof(voxelDescPacked) (4bytes)]
							do
							{
								VoxelData const curVoxel( *reinterpret_cast<VoxelData const* const>(pReadPointer) );
								pReadPointer += sizeof(VoxelData);
								
								uint8_t pendingAdjacency;
								if ( CullVoxels( numVoxels, curVoxel, pVoxelRoot, pendingAdjacency ) ) {

									pDestMem->VoxelsTemp.push_back( voxelDescPacked( voxCoord(curVoxel.x, curVoxel.y, sizeChunk.Height - curVoxel.z), 
																							    voxAdjacency(pendingAdjacency), curVoxel.shadeIndex) );
								}
								
							} while ( 0 != --voxelsChunk.numVoxels );
														
							// Sort the voxels by "slices" on .z (height offset) axis
							std::sort(pDestMem->VoxelsTemp.begin(), pDestMem->VoxelsTemp.end());	
							pDestMem->VoxelsTemp.shrink_to_fit(); // optimize memory usage after culling voxels
							
							pDestMem->numVoxels = pDestMem->VoxelsTemp.size();
							
							ChunkHeader paletteChunk;
							// read the expected "RGBA" chunk (optional) if it does not exist, the shadeIndex = greyscale level
							ReadData((void* const __restrict)&paletteChunk, pReadPointer, sizeof(paletteChunk));
							if (CompareTag(countof(TAG_PALETTE), (uint8_t const* const)paletteChunk.id, TAG_PALETTE)) { // if we have a valid structure for palette chunk
																
								pReadPointer += sizeof(paletteChunk);	// move to expected palette array data
								uint32_t* pPalette = new uint32_t[256];	// temporary array for translation of index to palette calculated greyscale level
								
								// Load Palette Data!
								pPalette[0] = 0;
								for ( uint32_t iDx = 0 ; iDx < 255 ; ++iDx ) {
									// swizzle RGBA to ARGB
									pPalette[iDx + 1] = (*(pReadPointer+3) << 24) | (*(pReadPointer) << 16) | (*(pReadPointer+1) << 8) | *(pReadPointer+2),
									pReadPointer += sizeof(uint32_t);
								}

								// Modify "shadeIndex" to equivalent grayscale Level
								uint32_t uiCount(pDestMem->numVoxels);
								voxelDescPacked* pVoxel = pDestMem->VoxelsTemp.data();
								do
								{
									xDMA2D::AlphaLuma const ARGB( pPalette[ pVoxel->Shade ] );
									pVoxel->Shade = ARGB.getConvertColorToLinearGray();
									++pVoxel;
									
								} while (--uiCount);
								delete [] pPalette; pPalette = nullptr;
							}
#ifdef VOX_DEBUG_ENABLED	
							DebugMessage("vox load (%d, %d, %d)", sizeChunk.Width, sizeChunk.Depth, sizeChunk.Height);
							DebugMessage("vox mem usage %d bxtes, %d voxels culled", (pDestMem->numVoxels * sizeof(voxelDescPacked)), (numVoxels - pDestMem->numVoxels));
#endif							                                                                                  
							pDestMem->maxDimensions = vec3_t(sizeChunk.Width - 1, sizeChunk.Depth - 1, sizeChunk.Height - 1); // must be -1, eg.) 0 -> 7 for 8x8x8 model (affects fit and scale accuracy)
							pDestMem->maxDimensionsInv = v3_inverse( pDestMem->maxDimensions );

							voxelModelDescHeader descModel;
							descModel.numVoxels = pDestMem->numVoxels;
							descModel.dimensionX = pDestMem->maxDimensions.x; descModel.dimensionY = pDestMem->maxDimensions.y; descModel.dimensionZ = pDestMem->maxDimensions.z;
							pDestMem->VoxelsFRAM = ProgramModelData(FRAMWritePointer, descModel, pDestMem->VoxelsTemp.data());
							// Free Temporary stl vector memory
							pDestMem->VoxelsTemp.clear();
							pDestMem->VoxelsTemp.shrink_to_fit();
							return(true);
						}
#ifdef VOX_DEBUG_ENABLED
						else
							DebugMessage("vox dimensions too large");
#endif
					}
#ifdef VOX_DEBUG_ENABLED
					else
						DebugMessage("expected XYZI chunk fail");
#endif				
				}
#ifdef VOX_DEBUG_ENABLED
				else
					DebugMessage("expected SOZE chunk fail");
#endif		
			}
#ifdef VOX_DEBUG_ENABLED
			else
				DebugMessage("root has no chunk data fail");
#endif		
		}
#ifdef VOX_DEBUG_ENABLED
		else
			DebugMessage("no MAIN tag");
#endif
	}
#ifdef VOX_DEBUG_ENABLED
	else
		DebugMessage("no VOX tag");
#endif	
		
	return(false);
}




} // end namespace voxB
} // end namespace Volumetric


/*
MagicaVoxel .vox File Format [10/18/2016]

1. File Structure : RIFF style
-------------------------------------------------------------------------------
# Bytes  | Type       | Value
-------------------------------------------------------------------------------
1x4      | char       | id 'VOX ' : 'V' 'O' 'X' 'space', 'V' is first
4        | int        | version number : 150

Chunk 'MAIN'
{
    // pack of models
    Chunk 'PACK'    : optional

    // models
    Chunk 'SIZE'
    Chunk 'XYZI'

    Chunk 'SIZE'
    Chunk 'XYZI'

    ...

    Chunk 'SIZE'
    Chunk 'XYZI'

    // palette
    Chunk 'RGBA'    : optional

    // materials
    Chunk 'MATT'    : optional
    Chunk 'MATT'
    ...
    Chunk 'MATT'
}
-------------------------------------------------------------------------------


2. Chunk Structure
-------------------------------------------------------------------------------
# Bytes  | Type       | Value
-------------------------------------------------------------------------------
1x4      | char       | chunk id
4        | int        | num bytes of chunk content (N)
4        | int        | num bytes of children chunks (M)

N        |            | chunk content

M        |            | children chunks
-------------------------------------------------------------------------------


3. Chunk id 'MAIN' : the root chunk and parent chunk of all the other chunks


4. Chunk id 'PACK' : if it is absent, only one model in the file
-------------------------------------------------------------------------------
# Bytes  | Type       | Value
-------------------------------------------------------------------------------
4        | int        | numModels : num of SIZE and XYZI chunks
-------------------------------------------------------------------------------


5. Chunk id 'SIZE' : model size
-------------------------------------------------------------------------------
# Bytes  | Type       | Value
-------------------------------------------------------------------------------
4        | int        | size x
4        | int        | size y
4        | int        | size z : gravity direction
-------------------------------------------------------------------------------


6. Chunk id 'XYZI' : model voxels
-------------------------------------------------------------------------------
# Bytes  | Type       | Value
-------------------------------------------------------------------------------
4        | int        | numVoxels (N)
4 x N    | int        | (x, y, z, colorIndex) : 1 byte for each component
-------------------------------------------------------------------------------


7. Chunk id 'RGBA' : palette
-------------------------------------------------------------------------------
# Bytes  | Type       | Value
-------------------------------------------------------------------------------
4 x 256  | int        | (R, G, B, A) : 1 byte for each component
                      | * <NOTICE>
                      | * color [0-254] are mapped to palette index [1-255], e.g : 
                      | 
                      | for ( int i = 0; i <= 254; i++ ) {
                      |     palette[i + 1] = ReadRGBA(); 
                      | }
-------------------------------------------------------------------------------


8. Default Palette : if chunk 'RGBA' is absent
-------------------------------------------------------------------------------
unsigned int default_palette[256] = {
	0x00000000, 0xffffffff, 0xffccffff, 0xff99ffff, 0xff66ffff, 0xff33ffff, 0xff00ffff, 0xffffccff, 0xffccccff, 0xff99ccff, 0xff66ccff, 0xff33ccff, 0xff00ccff, 0xffff99ff, 0xffcc99ff, 0xff9999ff,
	0xff6699ff, 0xff3399ff, 0xff0099ff, 0xffff66ff, 0xffcc66ff, 0xff9966ff, 0xff6666ff, 0xff3366ff, 0xff0066ff, 0xffff33ff, 0xffcc33ff, 0xff9933ff, 0xff6633ff, 0xff3333ff, 0xff0033ff, 0xffff00ff,
	0xffcc00ff, 0xff9900ff, 0xff6600ff, 0xff3300ff, 0xff0000ff, 0xffffffcc, 0xffccffcc, 0xff99ffcc, 0xff66ffcc, 0xff33ffcc, 0xff00ffcc, 0xffffcccc, 0xffcccccc, 0xff99cccc, 0xff66cccc, 0xff33cccc,
	0xff00cccc, 0xffff99cc, 0xffcc99cc, 0xff9999cc, 0xff6699cc, 0xff3399cc, 0xff0099cc, 0xffff66cc, 0xffcc66cc, 0xff9966cc, 0xff6666cc, 0xff3366cc, 0xff0066cc, 0xffff33cc, 0xffcc33cc, 0xff9933cc,
	0xff6633cc, 0xff3333cc, 0xff0033cc, 0xffff00cc, 0xffcc00cc, 0xff9900cc, 0xff6600cc, 0xff3300cc, 0xff0000cc, 0xffffff99, 0xffccff99, 0xff99ff99, 0xff66ff99, 0xff33ff99, 0xff00ff99, 0xffffcc99,
	0xffcccc99, 0xff99cc99, 0xff66cc99, 0xff33cc99, 0xff00cc99, 0xffff9999, 0xffcc9999, 0xff999999, 0xff669999, 0xff339999, 0xff009999, 0xffff6699, 0xffcc6699, 0xff996699, 0xff666699, 0xff336699,
	0xff006699, 0xffff3399, 0xffcc3399, 0xff993399, 0xff663399, 0xff333399, 0xff003399, 0xffff0099, 0xffcc0099, 0xff990099, 0xff660099, 0xff330099, 0xff000099, 0xffffff66, 0xffccff66, 0xff99ff66,
	0xff66ff66, 0xff33ff66, 0xff00ff66, 0xffffcc66, 0xffcccc66, 0xff99cc66, 0xff66cc66, 0xff33cc66, 0xff00cc66, 0xffff9966, 0xffcc9966, 0xff999966, 0xff669966, 0xff339966, 0xff009966, 0xffff6666,
	0xffcc6666, 0xff996666, 0xff666666, 0xff336666, 0xff006666, 0xffff3366, 0xffcc3366, 0xff993366, 0xff663366, 0xff333366, 0xff003366, 0xffff0066, 0xffcc0066, 0xff990066, 0xff660066, 0xff330066,
	0xff000066, 0xffffff33, 0xffccff33, 0xff99ff33, 0xff66ff33, 0xff33ff33, 0xff00ff33, 0xffffcc33, 0xffcccc33, 0xff99cc33, 0xff66cc33, 0xff33cc33, 0xff00cc33, 0xffff9933, 0xffcc9933, 0xff999933,
	0xff669933, 0xff339933, 0xff009933, 0xffff6633, 0xffcc6633, 0xff996633, 0xff666633, 0xff336633, 0xff006633, 0xffff3333, 0xffcc3333, 0xff993333, 0xff663333, 0xff333333, 0xff003333, 0xffff0033,
	0xffcc0033, 0xff990033, 0xff660033, 0xff330033, 0xff000033, 0xffffff00, 0xffccff00, 0xff99ff00, 0xff66ff00, 0xff33ff00, 0xff00ff00, 0xffffcc00, 0xffcccc00, 0xff99cc00, 0xff66cc00, 0xff33cc00,
	0xff00cc00, 0xffff9900, 0xffcc9900, 0xff999900, 0xff669900, 0xff339900, 0xff009900, 0xffff6600, 0xffcc6600, 0xff996600, 0xff666600, 0xff336600, 0xff006600, 0xffff3300, 0xffcc3300, 0xff993300,
	0xff663300, 0xff333300, 0xff003300, 0xffff0000, 0xffcc0000, 0xff990000, 0xff660000, 0xff330000, 0xff0000ee, 0xff0000dd, 0xff0000bb, 0xff0000aa, 0xff000088, 0xff000077, 0xff000055, 0xff000044,
	0xff000022, 0xff000011, 0xff00ee00, 0xff00dd00, 0xff00bb00, 0xff00aa00, 0xff008800, 0xff007700, 0xff005500, 0xff004400, 0xff002200, 0xff001100, 0xffee0000, 0xffdd0000, 0xffbb0000, 0xffaa0000,
	0xff880000, 0xff770000, 0xff550000, 0xff440000, 0xff220000, 0xff110000, 0xffeeeeee, 0xffdddddd, 0xffbbbbbb, 0xffaaaaaa, 0xff888888, 0xff777777, 0xff555555, 0xff444444, 0xff222222, 0xff111111
};
-------------------------------------------------------------------------------


9. Chunk id 'MATT' : material, if it is absent, it is diffuse material
-------------------------------------------------------------------------------
# Bytes  | Type       | Value
-------------------------------------------------------------------------------
4        | int        | id [1-255]

4        | int        | material type
                      | 0 : diffuse
                      | 1 : metal
                      | 2 : glass
                      | 3 : emissive
 
4        | float      | material weight
                      | diffuse  : 1.0
                      | metal    : (0.0 - 1.0] : blend between metal and diffuse material
                      | glass    : (0.0 - 1.0] : blend between glass and diffuse material
                      | emissive : (0.0 - 1.0] : self-illuminated material

4        | int        | property bits : set if value is saved in next section
                      | bit(0) : Plastic
                      | bit(1) : Roughness
                      | bit(2) : Specular
                      | bit(3) : IOR
                      | bit(4) : Attenuation
                      | bit(5) : Power
                      | bit(6) : Glow
                      | bit(7) : isTotalPower (*no value)

4 * N    | float      | normalized property value : (0.0 - 1.0]
                      | * need to map to real range
                      | * Plastic material only accepts {0.0, 1.0} for this version
-------------------------------------------------------------------------------
											
											*/
											
											