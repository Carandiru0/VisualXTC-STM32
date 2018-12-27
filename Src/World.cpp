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

//#define DEBUG_RENDER

#include "World.h"
#include "oled.h"
#include "rng.h"
#include "lighting.h"

#include "isoVoxel.h"
#include "noise.h"
#include "FLASH\imports.h"
#include "BuildingGeneration.h"
#include "AIBot.h"

#include "fireEffect.h"
#include "fractalboxesEffect.h"
#include "fogEffect.h"

#include "voxelModelsFRAM.h"
#define VOX_IMPL
#include "VoxMissile.h"
#include "Shockwave.h"
#include "Explosion.h"


#ifdef ENABLE_SKULL
#include "effect_sdf_viewer.h"
#endif

#include "debug.cpp"

using namespace world;

static constexpr uint32_t const EFFECT_DURATION = 5 * 60 * 1000; // ms

static constexpr uint32_t const UNIT_EFFECT = 0,
															 FIRE_EFFECT = 1,
															 FRACTALBOXES_EFFECT = 2,
															 FOG_EFFECT = 3,
															 SHOCKWAVE_EFFECT = 4,
															 NUM_EFFECTS = 4;

static constexpr uint32_t const STARTING_EFFECT = FIRE_EFFECT;

namespace Lighting
{
Lighting::ActiveLightSet ActiveLighting
	__attribute__((section (".dtcm")));				// bug fix, the ActiveLighting CTOR must be called b4 oWorld's CTOR
} // end namespace

static struct WorldEntity
{
	static constexpr uint32_t const UPDATE_AI_INTERVAL = 100, //ms  should divide clean TURN_INTERVAL, otherwise drift will occur
																	UPDATE_TRACKING_INTERVAL = 100; // ms
	
	static constexpr uint32_t const FLOOR_HEIGHT_PIXELS = 3,					// these values severely change performance and fog building appearance
																	FLOOR_WINDOW_HEIGHT_PIXELS = 2;
	
	struct CameraEntity
	{
		// These must be an even number
		static constexpr uint32_t const GRID_VOXELS_MAXVISIBLE_X = 18,		// should be even numbers
																	  GRID_VOXELS_MAXVISIBLE_Y = 20;
				
		point2D_t voxelIndex_TopLeft,
							voxelIndex_Center,
							voxelOffset;
		vec2_t		voxelIndex_vector_TopLeft,
							voxelIndex_vector_Center,
							voxelOffset_vector;
		
		vec2_t Origin,
					 PrevPosition,
					 TargetPosition,
					 vTarget,
					 vFollowVelocity;
		
		float tInvMoveTotalPosition;
		
		vec2_t const* FollowTarget;
			
		uint32_t tFollowUpdateInterval,
						 tLastFollowUpdate;
		
		uint32_t tMoveTotalPosition,
						 tMovePositionStart;
		
		uint32_t State;
		
		CameraEntity()
		: Origin(Iso::MIN_VOXEL_COORD, Iso::MAX_VOXEL_COORD), // virtual coordinates
			State(UNLOADED), FollowTarget(nullptr), tFollowUpdateInterval(0), tLastFollowUpdate(0)
		{}
	} oCamera;
	
	static Iso::Voxel theGrid[Iso::WORLD_GRID_SIZE*Iso::WORLD_GRID_SIZE]	// sizeof(IsoVoxel) == 2bytes
			__attribute__((section (".ext_sram.thegrid")));											  // 288*288*2 bytes = 165,888 bytes
	
	uint32_t const groundHeightPixels[8] = { 0,
																					 getGroundHeight_InPixels<1>(),
																					 getGroundHeight_InPixels<2>(),
																					 getGroundHeight_InPixels<3>(),
																					 getGroundHeight_InPixels<4>(),
																					 getGroundHeight_InPixels<5>(),
																					 getGroundHeight_InPixels<6>(),
																					 getGroundHeight_InPixels<7>() 
																					},
								 buildingHeightPixels[8] = { 0,
																						 BuildingGen::getBuildingHeight_InPixels<1>() + WorldEntity::FLOOR_WINDOW_HEIGHT_PIXELS,
																						 BuildingGen::getBuildingHeight_InPixels<2>() + 2 * WorldEntity::FLOOR_WINDOW_HEIGHT_PIXELS,
																						 BuildingGen::getBuildingHeight_InPixels<3>() + 3 * WorldEntity::FLOOR_WINDOW_HEIGHT_PIXELS,
																						 BuildingGen::getBuildingHeight_InPixels<4>() + 4 * WorldEntity::FLOOR_WINDOW_HEIGHT_PIXELS,
																						 BuildingGen::getBuildingHeight_InPixels<5>() + 5 * WorldEntity::FLOOR_WINDOW_HEIGHT_PIXELS,
																						 BuildingGen::getBuildingHeight_InPixels<6>() + 6 * WorldEntity::FLOOR_WINDOW_HEIGHT_PIXELS,
																						 BuildingGen::getBuildingHeight_InPixels<7>() + 7 * WorldEntity::FLOOR_WINDOW_HEIGHT_PIXELS 
																						};

	cAIBot		m_bot[NUM_AIBOTS];
														
  VoxMissile* 				m_pCurMissile;
	ExplosionInstance* 	m_pCurExplosion;
	ShockwaveInstance* 	m_pCurShockwave;
	
	Lighting::Light lPrimary, lSecondary, lTertiary;
	Lighting::Material const mGround,
													 mBuilding, mBuildingWindow;
	
	uint32_t tStart;
	uint32_t HashSeedStructure, HashSeedDestruction;
	uint32_t FogHeight;  // IN PIXEL UNITS
	uint32_t tLastUpdateAI, tLastUpdatedTracking, tLastTurn, BotIndexPendingTurn;
	uint32_t iCurEffect;
	uint32_t tStartEffect;
	
	bool bDeferredInitComplete;
	
	WorldEntity()
	: lPrimary(0.7f, 0.35f, 50.0f), lSecondary(0.40f, 0.2f, 50.0f), lTertiary(0.5f, 0.10f, 16.0f),
		mGround(0.85f),
	  mBuilding(0.79f),
		mBuildingWindow(0.99f),
		tStart(0),
		FogHeight(DEFAULT_FOG_HEIGHT_PIXELS),
	  iCurEffect(UNIT_EFFECT),
	  tStartEffect(0),
		tLastUpdateAI(0), tLastUpdatedTracking(0), tLastTurn(0), BotIndexPendingTurn(0),
		HashSeedStructure(0), HashSeedDestruction(0),
		m_pCurMissile(nullptr), m_pCurExplosion(nullptr), m_pCurShockwave(nullptr),
		bDeferredInitComplete(false),
		m_bot{ ( v2_to_p2D(WORLD_CENTER) )/*, ( v2_to_p2D(WORLD_TR) ), ( v2_to_p2D(WORLD_CENTER) ), ( v2_to_p2D(WORLD_BL) )*/  }
	{	}
	
} oWorld __attribute__((section (".dtcm")));

Iso::Voxel WorldEntity::theGrid[Iso::WORLD_GRID_SIZE*Iso::WORLD_GRID_SIZE]
					__attribute__((section (".ext_sram.thegrid")));		

// ####### Private Init merthods

// "voxelindices" are the (0,0) to (X,Y) integral representation of Grid Space
// To transform to ScreenSpace, it must be transformed to Isometric World Space relative to the
// TL corner of screen voxel index
// then finally offset by the Camera pre-computed pixeloffset
// it is then in pixel space or alternatively screen space
// If the result is negative or greater than screenbounds (in pixels) it's not onscreen/visible

// Grid Space Integral Units Only
__attribute__((pure)) STATIC_INLINE point2D_t const getScreenCenter_VoxelIndex() { return(oWorld.oCamera.voxelIndex_Center); }

// Naturally objects not confined to integral voxel indices eg.) missiles, dynamically moving objects, camera
// should be represented by a vec2_t in Grid Space (-x,-y)=>(X,Y) form
// So that the world representation of the world origin lays at (0.0f, 0.0f)

// Layout of World (*GridSpace Coordinates (0,0)=>(X,Y) form) // Is an Isometric plane
/*
															(0,288)
															 / *** \
										y axis-->	/       \
														 /         \   <--- x axis
														/           \
													 /             \
													*             *** (288,288)
										(0,0)  \             /
                            \           /
                   x axis--> \         / <--- y axis
                              \       /
                               \     /
                                \   /
                                 *** (288,0)
*/
// Therefore the neighbours of a voxel, follow same isometric layout/order //
/*

											NBR_TL
							NBR_L						NBR_T
		 NBR_BL						VOXEL						NBR_TR
							NBR_B						NBR_R		
											NBR_BR
											
*/

// defined in same order as constexpr NBR_TL => NBR_L (Clockwise order)
namespace world
{
point2D_t const ADJACENT[ADJACENT_NEIGHBOUR_COUNT] = { point2D_t(-1, 1), point2D_t (0,  1), point2D_t(1,  1), point2D_t (1,   0),
															                         point2D_t(1,   -1), point2D_t (0,   -1), point2D_t(-1,  -1), point2D_t (-1,  0) };
					
} // end namespace

// Grid Space Coordinates (0,0) to (X,Y) Only																	
__attribute__((pure)) STATIC_INLINE Iso::Voxel const* const __restrict getNeighbour(point2D_t const& __restrict voxelIndex, point2D_t const& __restrict relativeOffset)
{
	// at this point, function expects voxelIndex to be in (0,0) => (WORLD_GRID_SIZE, WORLD_GRID_SIZE) range
	point2D_t const voxelNeighbour( p2D_add(voxelIndex, relativeOffset) );
	
	// this function will also return the owning voxel
	// if zero,zero is passed in for the relative offset
	
	// Check bounds
	if ( voxelNeighbour.pt.x >= 0 && voxelNeighbour.pt.y >= 0 ) {
		
		if ( voxelNeighbour.pt.x < Iso::WORLD_GRID_SIZE && voxelNeighbour.pt.y < Iso::WORLD_GRID_SIZE ) {
			
			return( (oWorld.theGrid + ((voxelNeighbour.pt.y * Iso::WORLD_GRID_SIZE) + voxelNeighbour.pt.x)) );
		}
	}
	
	return(nullptr);
}

__attribute__((pure)) STATIC_INLINE Iso::Voxel const* const __restrict FindRootVoxelOfStructure( point2D_t const& __restrict voxelIndex, Iso::Voxel const& __restrict oVoxelSibling )
{
	// return itself if some how it's already the root
	if ( unlikely(isRootPlotPart(oVoxelSibling)) )
		return(&oVoxelSibling);
	
	// "root" of building plot, is always BR corner (max x, max y)
	// find root, siblings are always less than root in coordinates
	// so check in positive direction only relative to this sibling
	// NBR_T, NBR_TR, NBR_R
	uint32_t const uiCurHeightStep( Iso::getHeightStep(oVoxelSibling) );
	Iso::Voxel const* __restrict pNeighbour(nullptr);
	
	// NBR_T
	pNeighbour = ::getNeighbour(voxelIndex, ADJACENT[NBR_T]);
	if (nullptr != pNeighbour) {	// Test
		Iso::Voxel const oNeighbour(*pNeighbour);	// Read (Copy)
		if ( isRootPlotPart(oNeighbour) ) {
			// extra step, siblings are aware of structure height
			if ( Iso::getHeightStep(oNeighbour) == uiCurHeightStep )
				return(pNeighbour); // Found!
		}
	}

	// NBR_TR
	pNeighbour = ::getNeighbour(voxelIndex, ADJACENT[NBR_TR]);
	if (nullptr != pNeighbour) {	// Test
		Iso::Voxel const oNeighbour(*pNeighbour);	// Read (Copy)
		if ( isRootPlotPart(oNeighbour) ) {
			// extra step, siblings are aware of structure height
			if ( Iso::getHeightStep(oNeighbour) == uiCurHeightStep )
				return(pNeighbour); // Found!
		}
	}
	
	// NBR_R
	pNeighbour = ::getNeighbour(voxelIndex, ADJACENT[NBR_R]);
	if (nullptr != pNeighbour) {	// Test
		Iso::Voxel const oNeighbour(*pNeighbour);	// Read (Copy)
		if ( isRootPlotPart(oNeighbour) ) {
			// extra step, siblings are aware of structure height
			if ( Iso::getHeightStep(oNeighbour) == uiCurHeightStep )
				return(pNeighbour); // Found!
		}
	}
	
	// uh oh...
	return(nullptr);
}

// Grid Space Coordinates (0,0) to (X,Y) Only		
__attribute__((pure)) STATIC_INLINE bool const isCompletelyOcclulded( point2D_t const& __restrict voxelIndex, Iso::Voxel const& oVoxelTest )
{
	static constexpr int32_t const HEIGHT_THRESHOLD = GROUND_HEIGHT_NOISE_6; // change to HEIGHT_SCALED_3 and test after 
	// Must be atleast twice the height of a isometric
	// voxel, GRID_RADII(12) * 2 = 24pixels
	// HEIGHT_SCALED_3 * 9 = 27 (Height scaledstep is equal to 9, so level 3 would be minimum 
	uint32_t const heightOccludee( Iso::getHeightStep(oVoxelTest) ); 
	
	if ( Iso::MAX_HEIGHT_STEP != heightOccludee )
	{
		// Quick check to see if occulded by a voxel directly below itself with greater height
		Iso::Voxel const* const __restrict pOccluder = ::getNeighbour(voxelIndex, ADJACENT[NBR_BR]);
		
		if ( nullptr != pOccluder ) {
			Iso::Voxel const oOcculder(*pOccluder);
			if ( isVoxelVisible(oOcculder) ) {
				uint32_t const heightOccluder(Iso::getHeightStep(oOcculder));
				
				return( heightOccluder >= HEIGHT_THRESHOLD && heightOccluder > heightOccludee ); 
			}
		}
	}
	return(false);
}

static void ComputeGroundOcclusion()
{
	point2D_t voxelIndex(Iso::WORLD_GRID_SIZE - 1, Iso::WORLD_GRID_SIZE - 1);
	
	// Traverse Heigh Generated Grid	
							
	while ( voxelIndex.pt.y >= 0 )
	{
		voxelIndex.pt.x = Iso::WORLD_GRID_SIZE - 1;

		while ( voxelIndex.pt.x >= 0 ) 
		{		
			Iso::Voxel* const theGrid = (oWorld.theGrid + ((voxelIndex.pt.y * Iso::WORLD_GRID_SIZE) + voxelIndex.pt.x));
			
			// Get / Copy current voxel from external SRAM
			Iso::Voxel oVoxel(*theGrid);
			
			if (isGround(oVoxel))
			{
				if ( false == isCompletelyOcclulded(voxelIndex, oVoxel) ) {
					
					Iso::Voxel const* __restrict pNeighbour(nullptr);
					uint32_t const curVoxelHeightStep(Iso::getHeightStep(oVoxel));
					uint8_t OcclusionShading(0);
					// Therefore the neighbours of a voxel, follow same isometric layout/order //
					/*

																NBR_TL
												NBR_L						NBR_T
							 NBR_BL						VOXEL						NBR_TR
												NBR_B						NBR_R		
																NBR_BR
																
					*/
					// For top part of ground, check "corner" neighbours NBR_L & NBR_T to matched baked ao mask patterns
					pNeighbour = ::getNeighbour(voxelIndex, ADJACENT[NBR_L]);
					if (nullptr != pNeighbour) {
						Iso::Voxel const oNeighbour(*pNeighbour);
						if ( Iso::getHeightStep(oNeighbour) > curVoxelHeightStep ) {
							OcclusionShading |= Iso::OCCLUSION_SHADING_TOP_LEFT;
						}
					}
					pNeighbour = ::getNeighbour(voxelIndex, ADJACENT[NBR_T]);
					if (nullptr != pNeighbour) {
						Iso::Voxel const oNeighbour(*pNeighbour);
						if ( Iso::getHeightStep(oNeighbour) > curVoxelHeightStep ) {
							OcclusionShading |= Iso::OCCLUSION_SHADING_TOP_RIGHT;
						}
					}
					
					// further tests only apply to voxels of height greater than zero
					if ( Iso::DESC_HEIGHT_STEP_0 != curVoxelHeightStep ) 
					{
						// Check if sides need to be rendered, neighbours NBR_B and NBR_R may occluded side of this voxel if same height or greater.
						pNeighbour = ::getNeighbour(voxelIndex, ADJACENT[NBR_B]);
						if (nullptr != pNeighbour) {
							Iso::Voxel const oNeighbourBottom(*pNeighbour);
							if ( Iso::getHeightStep(oNeighbourBottom) >= (curVoxelHeightStep) ) {
								
								pNeighbour = ::getNeighbour(voxelIndex, ADJACENT[NBR_R]);
								if (nullptr != pNeighbour) {
									Iso::Voxel const oNeighbourRight(*pNeighbour);
									if ( Iso::getHeightStep(oNeighbourRight) >= (curVoxelHeightStep) ) {
										OcclusionShading |= Iso::OCCLUSION_SIDES_NOT_VISIBILE;
									}
								}
							}
						}
						
						// For side parts of ground, check left/right neighbours NBR_BL & NBR_TR to match baked ao mask patterns
						pNeighbour = ::getNeighbour(voxelIndex, ADJACENT[NBR_BL]);
						if (nullptr != pNeighbour) {
							Iso::Voxel const oNeighbour(*pNeighbour);
							if ( Iso::getHeightStep(oNeighbour) >= (curVoxelHeightStep) ) {
								OcclusionShading |= Iso::OCCLUSION_SHADING_SIDE_LEFT;
							}
						}
						pNeighbour = ::getNeighbour(voxelIndex, ADJACENT[NBR_TR]);
						if (nullptr != pNeighbour) {
							Iso::Voxel const oNeighbour(*pNeighbour);
							if ( Iso::getHeightStep(oNeighbour) >= (curVoxelHeightStep) ) {
								OcclusionShading |= Iso::OCCLUSION_SHADING_SIDE_RIGHT;
							}
						}
					}
					
					// finally, if no flags were set on occlusion, flag as such
					if ( 0 == OcclusionShading ) {
						Iso::setOcclusion(oVoxel, Iso::OCCLUSION_SHADING_NONE);
					}
					else {	// other wise save computed ao shading
						Iso::setOcclusion(oVoxel, OcclusionShading);
					}
				}
				else {
					// mark this voxel as not visible
					setNotVisible(oVoxel);
				}
				// Update current voxel in external SRAM
				*theGrid = oVoxel;
			}
			
			--voxelIndex.pt.x;
		}
		--voxelIndex.pt.y;
	}
}

static void GenerateGround(uint32_t const tNow)
{
	static constexpr float const NOISE_SCALAR_HEIGHT = 21.0f;
	
	static constexpr int32_t const EDGE_DETECTION_THRESHOLD = 236;
	
	
	int32_t yVoxel(Iso::WORLD_GRID_SIZE - 1), xVoxel(Iso::WORLD_GRID_SIZE - 1);
	
	// Traverse Grid
							
	float fXOffset, fYOffset;
	
	Noise::NewNoisePermutation(); // this also sets the current permutation storage to default native
	
	while ( yVoxel >= 0 )
	{
		xVoxel = Iso::WORLD_GRID_SIZE - 1;
				
		fYOffset = (float)yVoxel * Iso::INVERSE_WORLD_GRID_FSIZE;
		
		while ( xVoxel >= 0 ) 
		{					
			fXOffset = (float)xVoxel * Iso::INVERSE_WORLD_GRID_FSIZE;
						
			// Get perlin noise for this voxel
			uint32_t uNoiseHeight;
			
			{
				float const fNoiseHeight = Noise::getSimplexNoise2D( NOISE_SCALAR_HEIGHT * fXOffset, NOISE_SCALAR_HEIGHT * fYOffset )
																 * Noise::getPerlinNoise( NOISE_SCALAR_HEIGHT * 0.5f * fXOffset, NOISE_SCALAR_HEIGHT * 0.5f * fYOffset, 0.0f,
																													Noise::interpolator::SmoothStep() ) 
																 * 2.0f * Constants::nf255;
			
			float const fLog = Constants::nf255 - lerp(0.0f, Constants::nf255,
																								 ARM__logf(fNoiseHeight) / ARM__logf(Constants::nf255));

			float const fLerp = lerp(0.0f, Constants::nf255,
										           fNoiseHeight / Constants::nf255);
			
			int32_t const opXOR = ((int32_t)fLerp ^ (uint32_t)fLog);
			int32_t const opSUB = (int32_t)(fLerp - fLog);
			int32_t const opEDGES = (opSUB ^ opXOR);

			int32_t const opShadedEDGES = (opEDGES > EDGE_DETECTION_THRESHOLD ? GROUND_HEIGHT_NOISE_1 + 1 : opXOR);
			
			uNoiseHeight = __USAT( opShadedEDGES, Constants::SATBIT_256 );
#ifdef DEBUG_FLAT_GROUND
			uNoiseHeight = 0;
#endif
			}
			
			Iso::Voxel oVoxel;
			oVoxel.Desc = Iso::TYPE_GROUND;
			oVoxel.Hash = 0;		// Initially visibility is set off on all voxels until ComputeOcclusion()

			// Set the height based on perlin noise
			// There are only 8 distinct height levels used 
			if (uNoiseHeight > GROUND_HEIGHT_NOISE_7) {
				Iso::setHeightStep(oVoxel, 7);
			}
			else if (uNoiseHeight > GROUND_HEIGHT_NOISE_6) {
				Iso::setHeightStep(oVoxel, 6);
			}
			else if (uNoiseHeight > GROUND_HEIGHT_NOISE_5) {
				Iso::setHeightStep(oVoxel, 5);
			}
			else if (uNoiseHeight > GROUND_HEIGHT_NOISE_4) {
				Iso::setHeightStep(oVoxel, 4);
			}
			else if (uNoiseHeight > GROUND_HEIGHT_NOISE_3) {
				Iso::setHeightStep(oVoxel, 3);
			}
			else if (uNoiseHeight > GROUND_HEIGHT_NOISE_2) {
				Iso::setHeightStep(oVoxel, 2);
			}
			else if (uNoiseHeight > GROUND_HEIGHT_NOISE_1) {
				Iso::setHeightStep(oVoxel, 1);
			}
			else {
				Iso::setHeightStep(oVoxel, Iso::DESC_HEIGHT_STEP_0);
			}
	
			// Save Voxel to Grid SRAM
			*(oWorld.theGrid + ((yVoxel * Iso::WORLD_GRID_SIZE) + xVoxel)) = oVoxel;

			--xVoxel;
		}
		--yVoxel;
	}
	
	// Compute visibility and ambient occlusion based on neighbour occupancy
	ComputeGroundOcclusion();
}

static void LoadVoxelModels()
{
	bool bLoaded(false);
	
	bLoaded = Volumetric::LoadAllModels();
	if (!bLoaded)
		DebugMessage("FAIL load Voxel Models");
	
}

namespace world
{
	
// ####### Public Init merthods
NOINLINE void Init( uint32_t const tNow )
{
#ifdef ENABLE_SKULL
	SDF_Viewer::Initialize();
#endif
	
	// Set Initial  Active lighting 
	Lighting::Light* const ppLights[Lighting::MAX_NUM_ACTIVE_LIGHTS] = {&oWorld.lPrimary, &oWorld.lSecondary, &oWorld.lTertiary};
	Lighting::ActiveLighting.Initialize(ppLights, tNow);
	// Activate Main Light
	Lighting::ActiveLighting.ActivateLight(vec2_t(0.0f,0.0f), 0, tNow, Lighting::ActiveLighting.INFINITE_DURATION);
	
	// Initialize distinct hash seeds used for this program instance
	oWorld.HashSeedStructure = RandomNumber(0, UINT32_MAX);
	oWorld.HashSeedDestruction = RandomNumber(0, UINT32_MAX);
	
	GenerateGround(tNow);
	
	BuildingGen::Init(tNow);
	
	IsoDepth::Init();
	
	oWorld.tStart = millis();
}

NOINLINE void DeferredInit( uint32_t const tNow )
{
	static constexpr uint32_t const DELAY_INIT = 3000; // ms
	
	if ( !oWorld.bDeferredInitComplete ) {
		
		if ( tNow - oWorld.tStart > DELAY_INIT ) {
			
			LoadVoxelModels();
			
			oWorld.bDeferredInitComplete = true;
		}
		
	}
}
bool const isDeferredInitComplete() { return(oWorld.bDeferredInitComplete); }

} // end namespace

// ####### Private Update & Render merthods

typedef struct sFloorArgs
{
	static constexpr uint32_t const MAX_DIAMONDS = 2;
		
	halfdiamond2D_t cornersleftright[MAX_DIAMONDS];
	uint32_t diamondSize;
	int32_t  numDiamonds;
	
} FloorArgs;

typedef struct sHashPlotDesc
{
	FloorArgs descFloor;
	uint32_t	Height,
						PlotSize;
	
} HashPlotDesc;
template<uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE|OLED::FOG_ENABLE)>
static bool const RenderHashFloorLevel(point2D_t const Origin, uint32_t const uiHeight, uint32_t const HeightRemaining,
																			 FloorArgs const& descFloor)
{
	bool bDoRoofTop(false);
	int32_t LumaExtra(-1);
	
	for ( int32_t iDj = descFloor.numDiamonds - 1 ; iDj >= 0 ; --iDj )
	{		
		point2D_t xPoint, yPoint;
		// get plot corner extents (top diamond)
		halfdiamond2D_t cornersT;
		
		{
			halfdiamond2D_t cornersB(descFloor.cornersleftright[iDj]);
			cornersB.pts.t.pt.y -= uiHeight;
			cornersB.pts.b.pt.y -= uiHeight;
			
			// Test bottom diamond bottom extent against top of screen
			if (cornersB.pts.b.pt.y < 0)
				return(false); // indicate floor is not visible
			
			{
				point2D_t const ptExtents(0, uiHeight);
				cornersT.pts.t = p2D_sub(cornersB.pts.t, ptExtents);
				cornersT.pts.b = p2D_sub(cornersB.pts.b, ptExtents);
			}
						
			//if (cornersT.pts.t.pt.y > (OLED::SCREEN_HEIGHT))
			//	return(true); // skip non visible floors
			
			// Draw Vertical Lines Filling Left side & front side of building
			// starting from bottom work to left & RIGHT
			
			xPoint = point2D_t(cornersT.pts.b.pt.x, cornersB.pts.b.pt.x);
			yPoint = point2D_t(cornersT.pts.b.pt.y, cornersB.pts.b.pt.y);
		}
		
		if constexpr ( OLED::SHADE_ENABLE & RenderingFlags ) {
			
			// only calculate once for both diamonds if this is the case
			if ( LumaExtra < 0 ) {
				if ( HeightRemaining <= WorldEntity::FLOOR_HEIGHT_PIXELS ) {
					LumaExtra = Lighting::Shade<ISFOGENABLED(RenderingFlags)>(vec3_t(Origin.pt.x, uiHeight, Origin.pt.y), Lighting::NORMAL_TOPFACE, oWorld.mBuilding);
					bDoRoofTop = true;
				}
				else {
					LumaExtra = Lighting::Shade<ISFOGENABLED(RenderingFlags)>(vec3_t(Origin.pt.x, uiHeight, Origin.pt.y), Lighting::NORMAL_TOPFACE, oWorld.mBuildingWindow);
				}
			}
		}
			
		for (int iDx = (descFloor.diamondSize) - 1, iD = 0; iDx >= 0; iDx--, iD++)
		{
			point2D_t curXPoints, curYPoints;
			point2D_t const ptIncrement(iD >> 1, iD >> 1);
			
			curXPoints = p2D_add(xPoint, point2D_t(-iD, iD));
			curYPoints = p2D_sub(yPoint, ptIncrement);

			{ // scope rooftop
				// Draw Rooftop
				point2D_t yPointStart;

				// Leverage left and front(right) x point iterations, and leverage y point aswell for length
				int32_t iLength;
				if ( bDoRoofTop ) {
					yPointStart = point2D_t(cornersT.pts.t.pt.y, cornersT.pts.t.pt.y);		// top => to bottom

					yPointStart = p2D_add(yPointStart, ptIncrement);
					yPointStart = p2D_saturate<OLED::Height_SATBITS>(yPointStart);   
					iLength = (curYPoints.pt.x - yPointStart.pt.x);
				}
				else {
					yPointStart = point2D_t(cornersT.pts.b.pt.y, cornersT.pts.b.pt.y);		// bottom => top

					yPointStart = p2D_sub(yPointStart, ptIncrement);
					yPointStart = p2D_saturate<OLED::Height_SATBITS>(yPointStart);
					
					iLength = WorldEntity::FLOOR_WINDOW_HEIGHT_PIXELS; // window strip
					yPointStart.pt.x -= iLength; // DrawVLine drawls pixels starting from top always
				}
				
				if (likely(iLength > 0)) {
					
					if ( OLED::CheckVLine_YAxis(yPointStart.pt.x, iLength) ) 
					{
						if ( 0 == iDj || bDoRoofTop ) {  // only for single diamond, or left most diamond (excluding rooftop)
							// Occlusion bits are not for the top
							if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) {   // DrawVLine drawls pixels starting from top always
								OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.x, yPointStart.pt.x, iLength, LumaExtra);
							}
							if ( 1 == iD && bDoRoofTop ) { // fill in middle line exception case - this works (hack)
								if ( OLED::CheckVLine_XAxis(curXPoints.pt.x + 1) ) { 
									OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.x + 1, yPointStart.pt.x, iLength, LumaExtra);
								}
							}
						}
						if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
							OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.y, yPointStart.pt.x, iLength, LumaExtra);
						}
					}
				}
			} // scope rooftop

			if ( OLED::CheckVLine_YAxis(curYPoints.pt.x, WorldEntity::FLOOR_HEIGHT_PIXELS) ) 
			{
				vec3_t vLineMidpoint;
				uint32_t Luma;
				
				if ( OLED::SHADE_ENABLE & RenderingFlags ) {
					// Get midpoint of line as world poistion for light incident ray
					vLineMidpoint = vec3_t(0.0f, uiHeight >> 1, curYPoints.pt.y - ((curYPoints.pt.y - curYPoints.pt.x) >> 1));	
				}
				
				// saturate y axis only to clip lines, x axis bounds are fully checked, y axis must be clipped to zero for line length calculation
				curYPoints = p2D_saturate<OLED::Height_SATBITS>(curYPoints);
				
				if ( 0 == iDj && OLED::CheckVLine_XAxis(curXPoints.pt.x) ) { // only for single diamond, or left most diamond
					
					if constexpr ( OLED::SHADE_ENABLE & RenderingFlags ) {
							// Left Face
							vLineMidpoint.x = curXPoints.pt.x;
							Luma = Lighting::Shade<ISFOGENABLED(RenderingFlags)>(vLineMidpoint, Lighting::NORMAL_LEFTFACE, oWorld.mBuilding);
					}
					
					OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.x, curYPoints.pt.x, WorldEntity::FLOOR_HEIGHT_PIXELS, Luma);
				}
				
				if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
					
					
					if constexpr ( OLED::SHADE_ENABLE & RenderingFlags ) {
						// Front Face
						vLineMidpoint.x = curXPoints.pt.y;
						Luma = Lighting::Shade<ISFOGENABLED(RenderingFlags)>(vLineMidpoint, Lighting::NORMAL_FRONTFACE, oWorld.mBuilding);
					}
					
					OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.y, curYPoints.pt.x, WorldEntity::FLOOR_HEIGHT_PIXELS, Luma);
				}
			}
		} // for
	} // for
	
	return(true); 
}

static bool const SetupHashPlot( HashPlotDesc& descHashPlot, Iso::Voxel const oVoxel, point2D_t const Origin )
{
	uint32_t const uiHeight( oWorld.buildingHeightPixels[Iso::getHeightStep(oVoxel)] ), // in pixels
								 uiPlotSize( Iso::getPlotSize(oVoxel) ); // in discreete units, 1,2,3,4 needs to be factor of grid radii to be pixels
	
	if (0 == uiHeight)
		return(false); // vacant small plot
	
	if ( 1 == uiPlotSize ) {
		if ( (PsuedoRandom5050()) ) // reduce the ugly 1 size plot building, too many and create some space
			return(false);														 // improve performance as well (less ugly density)
	}
	
	point2D_t plotRadiiInPixels;
	
	{
		point2D_t const gridRadiiMagnitude( Iso::GRID_RADII, Iso::GRID_RADII>>1 );
		
		plotRadiiInPixels = p2D_mul( point2D_t(uiPlotSize, uiPlotSize), gridRadiiMagnitude );
		
		switch(uiPlotSize)
		{
			case BuildingGen::PLOT_SIZE_4x4:
				plotRadiiInPixels = p2D_sub(plotRadiiInPixels, /*gridSpacingRoads*/point2D_t(32, 16) );
				break;
			case BuildingGen::PLOT_SIZE_3x3:
				plotRadiiInPixels = p2D_sub(plotRadiiInPixels, /*gridSpacingRoads*/point2D_t(8, 4) );
				break;
			case BuildingGen::PLOT_SIZE_2x2:
				plotRadiiInPixels = p2D_sub(plotRadiiInPixels, /*gridSpacingRoads*/point2D_t(6, 3) );
				break;
			default:
				plotRadiiInPixels = p2D_sub(plotRadiiInPixels, /*gridSpacingRoads*/point2D_t(4, 2) );
				break;
		}

	}
	
	// Only the root "part" of the plot is called to render, siblings are ignored because this function renders their space
	// The "root" part is always at the BR corner (max x, max y), so drawing must increase on the y axis, and decrease on the x axis
	// by plotsize * grid radii, grid coordinates
	
	// note that the Origin passed in is right in the middle of a normal grid radii voxel, being the BR corner of the entire plot structure
	// offset by point2D(Iso::GRID_RADII/2, Iso::GRID_RADII/4)
	
	// (Isometric) Plot layout
	/*
						       _													// Width is still 2 * height (in pixels)
		            * # # * 											// The Box inside the plot leaves a "border" of empty space, to be filled by ground level
		        * # # # # # # * 									// the box is customized with various roof top blocks, sub divisions etc
		    * # # # # # # # # # # * 							// to emulate the look of a unique building
	  * # # # # # # # # # # # # # # *						// all "tiles" contained within this plot
* # # # # # # # # # # # # # # # # # # *				.. are rendered in this function
    * # # # # # # # # # # # # # # *
	      * # # # # # # # # # # * 
		        * # # # # # # * 
				        * # # * 
					         -	
	*/
	
	// get plot corner extents (bottom diamond)
	diamond2D_t cornersB;
	
	{
		// need to find origin of the plot, everything is based on it
		// transform passed in "tile" origin  of root (BR corner (max x, max y)) to the actual BR corner of the plot

		point2D_t const TRCorner( p2D_add( Origin, point2D_t((Iso::GRID_RADII), (Iso::GRID_RADII>>1)) ) );

		point2D_t const plotOrigin( p2D_sub( TRCorner, point2D_t( plotRadiiInPixels.pt.x, plotRadiiInPixels.pt.y >> 1 ) ) );
		
		{
			point2D_t const ptExtents(plotRadiiInPixels.pt.x, 0);
			cornersB.pts.l = p2D_sub(plotOrigin, ptExtents);
			cornersB.pts.r = p2D_add(plotOrigin, ptExtents);
		}
		{
			point2D_t const ptExtents(0, plotRadiiInPixels.pt.y);
			cornersB.pts.t = p2D_sub(plotOrigin, ptExtents);
			cornersB.pts.b = p2D_add(plotOrigin, ptExtents);
		}
	}
		
	// DRAW BOUNDS (bottom
#ifdef DEBUG_RENDER
	OLED::DrawLine<OLED::BACK_BUFFER>(cornersB.pts.b.pt.x, cornersB.pts.b.pt.y, cornersB.pts.r.pt.x, cornersB.pts.r.pt.y, OLED::g8);
	OLED::DrawLine<OLED::BACK_BUFFER>(cornersB.pts.r.pt.x, cornersB.pts.r.pt.y, cornersB.pts.t.pt.x, cornersB.pts.t.pt.y, OLED::g8);
	OLED::DrawLine<OLED::BACK_BUFFER>(cornersB.pts.t.pt.x, cornersB.pts.t.pt.y, cornersB.pts.l.pt.x, cornersB.pts.l.pt.y, OLED::g8);
	OLED::DrawLine<OLED::BACK_BUFFER>(cornersB.pts.l.pt.x, cornersB.pts.l.pt.y, cornersB.pts.b.pt.x, cornersB.pts.b.pt.y, OLED::g8);
#endif	
	// Set Depth Level per/object - being the minimum distance for the whole building to near origin
	// the mid point(critical important) on the building object will represent the depth
	// if not the midpoint adjacent buildings fight for order
	OLED::setDepthLevel( p2D_sub(cornersB.pts.b, p2D_subh(cornersB.pts.b, cornersB.pts.t)).pt.y );
	
	FloorArgs descFloor;
	
	if ( uiPlotSize > 1 && PsuedoRandom5050() )
	{
		static constexpr uint32_t const MAX_OFFSET = Iso::GRID_RADII;
		
		{
			point2D_t lastDiamondAnchor( cornersB.pts.l );
			uint32_t maxDiamondSize( p2D_subh(cornersB.pts.r, cornersB.pts.l).pt.x );
			uint32_t const diamondSize = maxDiamondSize >> 1;
			
			uint32_t const uiOffset(PsuedoRandomNumber(0, MAX_OFFSET<<8)>>8);
			
			point2D_t const diamondOrigin(diamondSize, 0);
			point2D_t const diamondOffset(p2D_add(diamondOrigin, point2D_t(uiOffset, uiOffset>>1)));
			
			point2D_t const ptExtents(0, diamondSize>>1);

			descFloor.cornersleftright[0].pts.t = p2D_add(p2D_sub(lastDiamondAnchor, ptExtents), diamondOffset);
			descFloor.cornersleftright[0].pts.b = p2D_add(p2D_add(lastDiamondAnchor, ptExtents), diamondOffset);

			lastDiamondAnchor = descFloor.cornersleftright[0].pts.t.v;

			descFloor.cornersleftright[1].pts.t = p2D_add(p2D_sub(lastDiamondAnchor, ptExtents), diamondOrigin);
			descFloor.cornersleftright[1].pts.b = p2D_add(p2D_add(lastDiamondAnchor, ptExtents), diamondOrigin);
			
			descFloor.diamondSize = diamondSize;
			descFloor.numDiamonds = 2;
		}
	}
	else {
		
		descFloor.cornersleftright[0] = cornersB;
		descFloor.diamondSize = plotRadiiInPixels.pt.x;
		descFloor.numDiamonds = 1;
	}
	
	// Return setup hash plot
	descHashPlot.Height = uiHeight;
	descHashPlot.PlotSize = uiPlotSize;
	descHashPlot.descFloor = descFloor;
	
	return(true); // if valid plot
}

// dynamic width, height & depth - good for structures
template<uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE|OLED::FOG_ENABLE)>
static void RenderHashPlot(Iso::Voxel const oVoxel, point2D_t const Origin, Lighting::Material const& mMaterial)
{
	HashPlotDesc descHashPlot;
	
	if ( SetupHashPlot(descHashPlot, oVoxel, Origin) )
	{	
		int32_t HeightRemaining(descHashPlot.Height);

		while ( HeightRemaining > 0 ) {
			
			uint32_t const uiFloorLevel( oWorld.buildingHeightPixels[Iso::getHeightStep(oVoxel)] - HeightRemaining );
			
			if ( uiFloorLevel < (oWorld.FogHeight << 1) + 1 ) {  // allowing more "fade-out"
				if ( !RenderHashFloorLevel<RenderingFlags>(Origin, uiFloorLevel, HeightRemaining, descHashPlot.descFloor) ) {
					// Floor Not visible, stop drawing more floors
					break;
				}
			}
			else {
				if ( !RenderHashFloorLevel<RenderingFlags & (~OLED::FOG_ENABLE)>(Origin, uiFloorLevel, HeightRemaining, descHashPlot.descFloor) ) {
					// Floor Not visible, stop drawing more floors
					break;
				}
			}
			HeightRemaining -= WorldEntity::FLOOR_HEIGHT_PIXELS;
		}
	}
}

template<uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE|OLED::FOG_ENABLE)>
static void RenderHashPlot_Collapsing(Iso::Voxel const oVoxel, point2D_t const Origin, Lighting::Material const& mMaterial)
{
	HashPlotDesc descHashPlot;
	
	if ( SetupHashPlot(descHashPlot, oVoxel, Origin) )
	{	
		int32_t HeightRemaining(descHashPlot.Height);

		while ( HeightRemaining > 0 ) {
			
			uint32_t const uiFloorLevel( oWorld.buildingHeightPixels[Iso::getHeightStep(oVoxel)] - HeightRemaining );
			
			if ( uiFloorLevel < (oWorld.FogHeight << 1) + 1 ) {  // allowing more "fade-out"
				if ( !RenderHashFloorLevel<RenderingFlags>(Origin, uiFloorLevel, HeightRemaining, descHashPlot.descFloor) ) {
					// Floor Not visible, stop drawing more floors
					break;
				}
			}
			else {
				if ( !RenderHashFloorLevel<RenderingFlags & (~OLED::FOG_ENABLE)>(Origin, uiFloorLevel, HeightRemaining, descHashPlot.descFloor) ) {
					// Floor Not visible, stop drawing more floors
					break;
				}
			}
			HeightRemaining -= WorldEntity::FLOOR_HEIGHT_PIXELS;
		}
	}
}

template<uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE|OLED::FOG_ENABLE)>
static void RenderHashPlot_Collapsed(Iso::Voxel const oVoxel, point2D_t const Origin, Lighting::Material const& mMaterial)
{
	HashPlotDesc descHashPlot;
	
	if ( SetupHashPlot(descHashPlot, oVoxel, Origin) )
	{	
		int32_t HeightRemaining(descHashPlot.Height);

		while ( HeightRemaining > 0 ) {
			
			uint32_t const uiFloorLevel( oWorld.buildingHeightPixels[Iso::getHeightStep(oVoxel)] - HeightRemaining );
			
			if ( uiFloorLevel < (oWorld.FogHeight << 1) + 1 ) {  // allowing more "fade-out"
				if ( !RenderHashFloorLevel<RenderingFlags>(Origin, uiFloorLevel, HeightRemaining, descHashPlot.descFloor) ) {
					// Floor Not visible, stop drawing more floors
					break;
				}
			}
			else {
				if ( !RenderHashFloorLevel<RenderingFlags & (~OLED::FOG_ENABLE)>(Origin, uiFloorLevel, HeightRemaining, descHashPlot.descFloor) ) {
					// Floor Not visible, stop drawing more floors
					break;
				}
			}
			HeightRemaining -= WorldEntity::FLOOR_HEIGHT_PIXELS;
		}
	}
}
// static width/depth using single grid radii unit (good for ground, water etc)
template<uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE|OLED::FOG_ENABLE)>
static void RenderBox_AO(uint32_t const OcclusionBits, point2D_t const Origin, uint32_t const uiHeight, Lighting::Material const& mMaterial)
{
	// get plot corner extents (bottom/top diamond)
	halfdiamond2D_t cornersB;
		
	// get corner extents (bottom diamond)
	{
		point2D_t const ptExtents(0, Iso::GRID_RADII>>1);
		cornersB.pts.t = p2D_sub(Origin, ptExtents);
		cornersB.pts.b = p2D_add(Origin, ptExtents);
	}

	// Set Depth Level per/object - being the minimum distance for the whole building to near origin
	// the mid point(critical important) on the building object will represent the depth
	// if not the midpoint adjacent buildings fight for order
	OLED::setDepthLevel( p2D_sub(cornersB.pts.b, p2D_subh(cornersB.pts.b, cornersB.pts.t)).pt.y );
		
	// get corner extents (top diamond)
	halfdiamond2D_t cornersT;
	
	{
		point2D_t const ptExtents(0, uiHeight);
		cornersT.pts.t = p2D_sub(cornersB.pts.t, ptExtents);
		cornersT.pts.b = p2D_sub(cornersB.pts.b, ptExtents);
	}
	
	// Draw Vertical Lines Filling Left side & front side of building
	// starting from bottom work to left & RIGHT
	
	point2D_t const xPoint(cornersT.pts.b.pt.x, cornersB.pts.b.pt.x);
	point2D_t const yPoint(cornersT.pts.b.pt.y, cornersB.pts.b.pt.y);

	uint32_t LumaRooftop(0);
	
	if constexpr ( OLED::SHADE_ENABLE & RenderingFlags )
		LumaRooftop = Lighting::Shade<ISFOGENABLED(RenderingFlags)>(vec3_t(Origin.pt.x, uiHeight, Origin.pt.y), Lighting::NORMAL_TOPFACE, mMaterial);

	OLED::MaskDesc descMask = { nullptr, 0, Origin, point2D_t(Iso::GRID_RADII - 1, Iso::GRID_RADII >> 1), 0 };
	
	for (int iDx = Iso::GRID_RADII - 1, iD = 0; iDx >= 0; iDx--, iD++)
	{
		point2D_t curXPoints, curYPoints;
		point2D_t const ptIncrement(iD >> 1, iD >> 1);
		
		curXPoints = p2D_add(xPoint, point2D_t(-iD, iD));
		curYPoints = p2D_sub(yPoint, point2D_t(ptIncrement));

		{
			// Draw Rooftop
			point2D_t yPointTop(cornersT.pts.t.pt.y, cornersT.pts.t.pt.y);

			yPointTop = p2D_add(yPointTop, ptIncrement);
			yPointTop = p2D_saturate<OLED::Height_SATBITS>(yPointTop);		// yaxis must be saturated for length calculation, actual culling is performed
																																		// only on xaxis for flat roof of ground

			// Leverage left and front(right) x point iterations, and leverage y point aswell for length
			int32_t const iLength = (curYPoints.pt.x - yPointTop.pt.x);
			if (likely(iLength > 0)) {
				
				descMask.CurOffset = uiHeight;
				descMask.Stride = top_ao_stride;

				// both
				if ( (Iso::OCCLUSION_SHADING_TOP_LEFT | Iso::OCCLUSION_SHADING_TOP_RIGHT) == 
						 ((Iso::OCCLUSION_SHADING_TOP_LEFT | Iso::OCCLUSION_SHADING_TOP_RIGHT) & OcclusionBits) )
				{
					descMask.Mask = DTCM::_sram_top_ao_bothsides;
					
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) {
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x, yPointTop.pt.x), iLength, LumaRooftop, descMask);
					}
					if ( 1 == iD  ) { // fill in middle line exception case - this works (hack)
						if ( OLED::CheckVLine_XAxis(curXPoints.pt.x + 1) ) { 
							OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x + 1, yPointTop.pt.x), iLength, LumaRooftop, descMask);
						}
					}
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.y, yPointTop.pt.x), iLength, LumaRooftop, descMask);
					}
				}
				else if ( Iso::OCCLUSION_SHADING_TOP_LEFT & OcclusionBits ) 
				{
					descMask.Mask = DTCM::_sram_top_ao_oneside;
					
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) {
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x, yPointTop.pt.x), iLength, LumaRooftop, descMask);
					}
					if ( 1 == iD  ) { // fill in middle line exception case - this works (hack)
						if ( OLED::CheckVLine_XAxis(curXPoints.pt.x + 1) ) { 
							OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x + 1, yPointTop.pt.x), iLength, LumaRooftop, descMask);
						}
					}
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.y, yPointTop.pt.x), iLength, LumaRooftop, descMask);
					}
				}
				else if ( Iso::OCCLUSION_SHADING_TOP_RIGHT & OcclusionBits ) 
				{
					descMask.Mask = DTCM::_sram_top_ao_oneside_mirrored;
	
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) {
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x, yPointTop.pt.x), iLength, LumaRooftop, descMask);
					}
					if ( 1 == iD  ) { // fill in middle line exception case - this works (hack)
						if ( OLED::CheckVLine_XAxis(curXPoints.pt.x + 1) ) { 
							OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x + 1, yPointTop.pt.x), iLength, LumaRooftop, descMask);
						}
					}
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.y, yPointTop.pt.x), iLength, LumaRooftop, descMask);
					}
				}
				else { // Occlusion bits are not for the top
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) {
						OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.x, yPointTop.pt.x, iLength, LumaRooftop);
					}
					if ( 1 == iD  ) { // fill in middle line exception case - this works (hack)
						if ( OLED::CheckVLine_XAxis(curXPoints.pt.x + 1) ) { 
							OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.x + 1, yPointTop.pt.x, iLength, LumaRooftop);
						}
					}
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
						OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.y, yPointTop.pt.x, iLength, LumaRooftop);
					}
				}
			}
		} // scope rooftop
					
		if ( Iso::OCCLUSION_SIDES_NOT_VISIBILE & OcclusionBits ) // skip rendering sides ?
			continue;
		
		int32_t LumaLeft(-1), LumaFront(-1);
		if ( OLED::CheckVLine_YAxis(curYPoints.pt.x, (curYPoints.pt.y - curYPoints.pt.x)) ) 
		{
			vec3_t vLineMidpoint;
			if ( OLED::SHADE_ENABLE & RenderingFlags ) {
				// Get midpoint of line as world poistion for light incident ray
				vLineMidpoint = vec3_t(0.0f, uiHeight >> 1, curYPoints.pt.y - ((curYPoints.pt.y - curYPoints.pt.x) >> 1));
			}
			// Calculate lighting per vertical line
			// incident light vector
			if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) { // only for single diamond, or left most diamond
				
				if ( OLED::SHADE_ENABLE & RenderingFlags ) {
						// Left Face
						vLineMidpoint.x = curXPoints.pt.x;
						LumaLeft = Lighting::Shade<ISFOGENABLED(RenderingFlags)>(vLineMidpoint, Lighting::NORMAL_LEFTFACE, oWorld.mBuilding);
				}
				else
					LumaLeft = 0;
			}
			
			if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
				
				if ( OLED::SHADE_ENABLE & RenderingFlags ) {
					// Front Face
					vLineMidpoint.x = curXPoints.pt.y;
					LumaFront = Lighting::Shade<ISFOGENABLED(RenderingFlags)>(vLineMidpoint, Lighting::NORMAL_FRONTFACE, oWorld.mBuilding);
				}
				else
					LumaFront = 0;
			}
		}
		
		if ( (LumaLeft | LumaFront) >= 0 )  // early rejection of both sides
		{
			// saturate y axis only to clip lines, x axis bounds are fully checked, y axis must be clipped to zero for line length calculation
			curYPoints = p2D_saturate<OLED::Height_SATBITS>(curYPoints);

			int32_t const iLength = (curYPoints.pt.y - curYPoints.pt.x);
			
			if (likely(iLength > 0))
			{
				descMask.CurOffset = uiHeight;// - (Iso::GRID_RADII >> 1) - 1;
				descMask.Stride = side_ao_stride;
				
				// both sides + bottom
				if ( (Iso::OCCLUSION_SHADING_SIDE_LEFT | Iso::OCCLUSION_SHADING_SIDE_RIGHT) & OcclusionBits )
				{	
					if ( LumaLeft >= 0 ) {
						descMask.Mask = DTCM::_sram_side_7_both_ao;	
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x, curYPoints.pt.x), iLength, LumaLeft, descMask);
					}
					if ( LumaFront >= 0 ) {
						descMask.Mask = DTCM::_sram_side_7_both_ao_mirrored;	
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.y, curYPoints.pt.x), iLength, LumaFront, descMask);
					}
				}
				else if ( Iso::OCCLUSION_SHADING_SIDE_LEFT & OcclusionBits ) // one side + bottom
				{
					if ( LumaLeft >= 0 ) {
						descMask.Mask = DTCM::_sram_side_7_both_ao;
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x, curYPoints.pt.x), iLength, LumaLeft, descMask);
					}
					if ( LumaFront >= 0 ) {
						descMask.Mask = DTCM::_sram_side_7_botom_ao_mirrored;
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.y, curYPoints.pt.x), iLength, LumaFront, descMask);
					}
				}
				else if ( Iso::OCCLUSION_SHADING_SIDE_RIGHT & OcclusionBits ) // one side + bottom
				{
					if ( LumaLeft >= 0 ) {
						descMask.Mask = DTCM::_sram_side_7_botom_ao;
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x, curYPoints.pt.x), iLength, LumaLeft, descMask);
					}
					if ( LumaFront >= 0 ) {
						descMask.Mask = DTCM::_sram_side_7_both_ao_mirrored;
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.y, curYPoints.pt.x), iLength, LumaFront, descMask);
					}
				}
				else { // bottom only, always at least bottom AO
					
					if ( LumaLeft >= 0 ) {
						descMask.Mask = DTCM::_sram_side_7_botom_ao;
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x, curYPoints.pt.x), iLength, LumaLeft, descMask);
					}
					if ( LumaFront >= 0 ) {
						descMask.Mask = DTCM::_sram_side_7_botom_ao_mirrored;
						OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.y, curYPoints.pt.x), iLength, LumaFront, descMask);
					}
				}
			} //if length
		} // if valid graylevels
	} // for
}

template<uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE|OLED::FOG_ENABLE)>
static void RenderFlatForcedGround( point2D_t const Origin )
{
	// get corner extents (bottom diamond)
	halfdiamond2D_t corners;

	{
		point2D_t const ptExtents(0, Iso::GRID_RADII>>1);
		corners.pts.t = p2D_sub(Origin, ptExtents);
		corners.pts.b = p2D_add(Origin, ptExtents);
	}
	
	// Set Depth Level per/object - being the minimum distance for the whole building to near origin
	// the mid point(critical important) on the building object will represent the depth
	// if not the midpoint adjacent buildings fight for order
	//OLED::setDepthLevel( p2D_sub(corners.pts.b, p2D_subh(corners.pts.b, corners.pts.t)).pt.y >> 3 ); //typically used as background, ensure z depth is ok for that
	OLED::setDepthLevel_ToFarthest();
	// Draw Vertical Lines Filling diamond
	
	point2D_t const xPoint(corners.pts.b.pt.x, corners.pts.b.pt.x);		// bottom
	point2D_t const yPoint(corners.pts.b.pt.y, corners.pts.b.pt.y);

	uint32_t Luma(0);
	
	if constexpr ( OLED::SHADE_ENABLE & RenderingFlags )
		Luma = Lighting::Shade<ISFOGENABLED(RenderingFlags)>(vec3_t(Origin.pt.x, 0.0f, Origin.pt.y), Lighting::NORMAL_TOPFACE, oWorld.mGround);
	
	for (int iDx = Iso::GRID_RADII - 1, iD = 0; iDx >= 0; iDx--, iD++)
	{
		point2D_t curXPoints, curYPoints;
		point2D_t const ptIncrement(iD >> 1, iD >> 1);
		
		curXPoints = p2D_add(xPoint, point2D_t(-iD, iD));
		curYPoints = p2D_sub(yPoint, ptIncrement);

		point2D_t yPointTop(corners.pts.t.pt.y, corners.pts.t.pt.y);		// top

		yPointTop = p2D_add(yPointTop, ptIncrement);
		yPointTop = p2D_saturate<OLED::Height_SATBITS>(yPointTop);		// yaxis must be saturated for length calculation, actual culling is performed
																																	// only on xaxis for flat ground

		// Leverage left and front(right) x point iterations, and leverage y point aswell for length
		int32_t const iLength = (curYPoints.pt.x - yPointTop.pt.x);
		if (likely(iLength > 0)) {
			// Draws Ground Symmetrically on both sides at the same time
			if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) {
				OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.x, yPointTop.pt.x, iLength, Luma);
			}
			if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
				OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.y, yPointTop.pt.x, iLength, Luma);
			}
		}
	}
}
template<uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE|OLED::FOG_ENABLE)>
static void RenderGround( Iso::Voxel const oVoxel, point2D_t const Origin )
{
	uint32_t const uiHeightStep( Iso::getHeightStep(oVoxel) );
	
	// Render flat quad....
	if ( Iso::DESC_HEIGHT_STEP_0 == uiHeightStep )
	{
		// get corner extents (bottom diamond)
		halfdiamond2D_t corners;

		{
			point2D_t const ptExtents(0, Iso::GRID_RADII>>1);
			corners.pts.t = p2D_sub(Origin, ptExtents);
			corners.pts.b = p2D_add(Origin, ptExtents);
		}
		
		// Set Depth Level per/object - being the minimum distance for the whole building to near origin
		// the mid point(critical important) on the building object will represent the depth
		// if not the midpoint adjacent buildings fight for order
		OLED::setDepthLevel( p2D_sub(corners.pts.b, p2D_subh(corners.pts.b, corners.pts.t)).pt.y );
		
		// Draw Vertical Lines Filling diamond
		
		point2D_t const xPoint(corners.pts.b.pt.x, corners.pts.b.pt.x);		// bottom
		point2D_t const yPoint(corners.pts.b.pt.y, corners.pts.b.pt.y);

		uint32_t Luma(0);
		
		if constexpr ( OLED::SHADE_ENABLE & RenderingFlags )
			Luma = Lighting::Shade<ISFOGENABLED(RenderingFlags)>(vec3_t(Origin.pt.x, 0.0f, Origin.pt.y), Lighting::NORMAL_TOPFACE, oWorld.mGround);

		OLED::MaskDesc descMask = { nullptr, 0, Origin, point2D_t(Iso::GRID_RADII - 1, Iso::GRID_RADII >> 1), 0 };
		uint32_t const OcclusionBits(Iso::getOcclusion(oVoxel));
		
		for (int iDx = Iso::GRID_RADII - 1, iD = 0; iDx >= 0; iDx--, iD++)
		{
			point2D_t curXPoints, curYPoints;
			point2D_t const ptIncrement(iD >> 1, iD >> 1);
			
			curXPoints = p2D_add(xPoint, point2D_t(-iD, iD));
			curYPoints = p2D_sub(yPoint, ptIncrement);

			point2D_t yPointTop(corners.pts.t.pt.y, corners.pts.t.pt.y);		// top

			yPointTop = p2D_add(yPointTop, ptIncrement);
			yPointTop = p2D_saturate<OLED::Height_SATBITS>(yPointTop);		// yaxis must be saturated for length calculation, actual culling is performed
																																		// only on xaxis for flat ground

			// Leverage left and front(right) x point iterations, and leverage y point aswell for length
			int32_t const iLength = (curYPoints.pt.x - yPointTop.pt.x);
			if (likely(iLength > 0)) {
				// Draws Ground Symmetrically on both sides at the same time
				
				if ( Iso::OCCLUSION_SHADING_NONE == OcclusionBits ) {
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) {
						OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.x, yPointTop.pt.x, iLength, Luma);
					}
					if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
						OLED::DrawVLine<OLED::BACK_BUFFER, RenderingFlags>(curXPoints.pt.y, yPointTop.pt.x, iLength, Luma);
					}
				}
				else {
					
					descMask.CurOffset = 0;
					descMask.Stride = top_ao_stride;
					
					// both
					if ( (Iso::OCCLUSION_SHADING_TOP_LEFT | Iso::OCCLUSION_SHADING_TOP_RIGHT) == 
						   ((Iso::OCCLUSION_SHADING_TOP_LEFT | Iso::OCCLUSION_SHADING_TOP_RIGHT) & OcclusionBits) )
					{
						descMask.Mask = DTCM::_sram_top_ao_bothsides;

						if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) {
							OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x, yPointTop.pt.x), iLength, Luma, descMask);
						}
						if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
							OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.y, yPointTop.pt.x), iLength, Luma, descMask);
						}
					}
					else if ( Iso::OCCLUSION_SHADING_TOP_LEFT & OcclusionBits ) 
					{
						descMask.Mask = DTCM::_sram_top_ao_oneside;

						if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) {
							OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x, yPointTop.pt.x), iLength, Luma, descMask);
						}
						if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
							OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.y, yPointTop.pt.x), iLength, Luma, descMask);
						}
					}
					else if ( Iso::OCCLUSION_SHADING_TOP_RIGHT & OcclusionBits ) 
					{
						descMask.Mask = DTCM::_sram_top_ao_oneside_mirrored;

						if ( OLED::CheckVLine_XAxis(curXPoints.pt.x) ) {
							OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.x, yPointTop.pt.x), iLength, Luma, descMask);
						}
						if ( OLED::CheckVLine_XAxis(curXPoints.pt.y) ) {
							OLED::DrawVLine_Masked<RenderingFlags>(point2D_t(curXPoints.pt.y, yPointTop.pt.x), iLength, Luma, descMask);
						}
					}
					
				}
			}
		}
	}
	else // render complete voxel
	{
		uint32_t const uiHeightPixels =  oWorld.groundHeightPixels[uiHeightStep];
		if ( uiHeightPixels < oWorld.FogHeight )
			RenderBox_AO<RenderingFlags>(Iso::getOcclusion(oVoxel), Origin, uiHeightPixels, oWorld.mGround);
		else
			RenderBox_AO<RenderingFlags & (~OLED::FOG_ENABLE)>(Iso::getOcclusion(oVoxel), Origin, uiHeightPixels, oWorld.mGround);
		
		// Iso::OCCLUSION_SHADING_NONE does not apply here
		// instead of using an extra bit
		// because the bottom will always have occlusion
	}
}
template<uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE|OLED::FOG_ENABLE)>
static void RenderStructure( Iso::Voxel const oVoxel, point2D_t const voxelIndex, point2D_t const Origin )
{
	// Set hash seed specific to structures
	HashSetSeed(oWorld.HashSeedStructure);
	
	// Get our Psuedo Seed from hashing the unique world grid coordinates of this voxel

	// Set the psuedo rng seed to our deterministic seed
	PsuedoSetSeed( /*DeterministicSeed = */ Hash( voxelIndex.pt.x ^ Hash( voxelIndex.pt.x ) )  );
	
	RenderHashPlot<RenderingFlags>(oVoxel, Origin, oWorld.mBuilding);
}
template<uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE|OLED::FOG_ENABLE)>
static void RenderDestruction( Iso::Voxel const oVoxel, point2D_t const voxelIndex, point2D_t const Origin )
{
	if ( isCollapsed(oVoxel) ) {	
		// Set hash seed specific to structures
		HashSetSeed(oWorld.HashSeedDestruction);
		
		// Get our Psuedo Seed from hashing the unique world grid coordinates of this voxel

		// Set the psuedo rng seed to our deterministic seed
		PsuedoSetSeed( /*DeterministicSeed = */ Hash( voxelIndex.pt.x ^ Hash( voxelIndex.pt.x ) )  );
		
		RenderHashPlot_Collapsed<RenderingFlags>(oVoxel, Origin, oWorld.mBuilding);
	}
	else if ( isCollapsing(oVoxel) ) {
		
		// Set hash seed specific to structures - still used while collapsing, switches when finally collapsed
		HashSetSeed(oWorld.HashSeedStructure);
		
		// Get our Psuedo Seed from hashing the unique world grid coordinates of this voxel

		// Set the psuedo rng seed to our deterministic seed
		PsuedoSetSeed( /*DeterministicSeed = */ Hash( voxelIndex.pt.x ^ Hash( voxelIndex.pt.x ) )  );
		
		RenderHashPlot_Collapsing<RenderingFlags>(oVoxel, Origin, oWorld.mBuilding);
	}
}
template<uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE)>
static void RenderEffect( Iso::Voxel const oVoxel, point2D_t const Origin )
{
	
}

//#define DEBUG_RENDER
template<uint32_t const RenderingFlags = (OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE|OLED::FOG_ENABLE)>
static void RenderGrid()
{
	point2D_t const voxelStart(oWorld.oCamera.voxelIndex_TopLeft);
	point2D_t voxelIndex( voxelStart );	
	
	// Traverse Grid
	Iso::Voxel const* theGrid;	

	uint32_t yCount(WorldEntity::CameraEntity::GRID_VOXELS_MAXVISIBLE_Y);
	while ( 0 != yCount )
	{
		if ( unlikely(voxelIndex.pt.y < 0) ) {
			++voxelIndex.pt.y;
			--yCount;
			continue; // skip 
		}
		else if ( unlikely(voxelIndex.pt.y >= Iso::WORLD_GRID_SIZE) )
			break; // done past end of world grid
		
		uint32_t xCount(WorldEntity::CameraEntity::GRID_VOXELS_MAXVISIBLE_X);
		
		// Reset voxel index to start for x axis
		voxelIndex.pt.x = voxelStart.pt.x;
		
		// draw order to TL => BR //
		theGrid = oWorld.theGrid + ((voxelIndex.pt.y * Iso::WORLD_GRID_SIZE) + voxelIndex.pt.x);
		
		while ( 0 != xCount ) 
		{
			if ( unlikely(voxelIndex.pt.x < 0) ) {
				++theGrid; // the only place we traverse the grid like this
				++voxelIndex.pt.x;
				--xCount;
				continue; // skip over
			}
			else if ( unlikely(voxelIndex.pt.x >= Iso::WORLD_GRID_SIZE) )
				break; // donem past end of world grid
		
			Iso::Voxel const oVoxel(*theGrid);
			
			// Make index (row,col)relative to starting index voxel
			// calculate origin from relative row,col
			// Add the offset of the world origin calculated
			// Now inside screenspace coordinates
			point2D_t const voxelOrigin( p2D_add( Iso::p2D_GridToIso( p2D_sub(voxelIndex, voxelStart) ), oWorld.oCamera.voxelOffset ) );
			
			if ( isGround(oVoxel) ) {
				
				if ( isVoxelVisible(oVoxel) ) { // pre-processing of occlusion rejection test
																	// of completely occlulded ground voxel					
					// Early rejection test
					if ( false == TestPoint_Not_OnScreen(voxelOrigin) ) {

						RenderGround<RenderingFlags>( oVoxel, voxelOrigin );
					} // onscreen visibility
				} // isvisible
			} // isground
			else 
			{				
				// Early rejection test
				if ( false == TestPoint_Not_OnScreen(voxelOrigin, oWorld.buildingHeightPixels[BuildingGen::NUM_DISTINCT_BUILDING_HEIGHTS]) ) {
					
					if ( isStructure(oVoxel) ) {
#ifndef DEBUG_NORENDER_STRUCTURES												
						if ( isRootPlotPart(oVoxel) ) {
							RenderStructure<RenderingFlags>( oVoxel, voxelIndex, voxelOrigin );
						}
#endif
						// always render background ground for root + siblings
						// rendering after to leverage z-buffer, drawing only pixels to fill in background (speedup)
#ifndef DEBUG_RENDER
						RenderFlatForcedGround<RenderingFlags>(voxelOrigin);
#endif
					}
					else if ( isDestruction(oVoxel) ) {
						RenderDestruction<RenderingFlags>( oVoxel, voxelIndex, voxelOrigin );
					}
					else if ( isEffect(oVoxel) ) {
						RenderEffect<RenderingFlags>( oVoxel, voxelOrigin );
					}
					
				} // onscreen visibility
			} // else not ground
			
			++theGrid; // the only place we traverse the grid like this
			++voxelIndex.pt.x;
			--xCount;
		}
		++voxelIndex.pt.y;
		--yCount;
	}
	
#ifdef DEBUG_RENDER
	static constexpr uint32_t const tUpdateDebugInterval = 150; //ms
	static uint32_t tLastDebug;
	
	if (millis() - tLastDebug > tUpdateDebugInterval) {
		DebugMessage( "(%d,%d) to (%d,%d)", voxelStart.pt.x, voxelStart.pt.y, voxelIndex.pt.x, voxelIndex.pt.y);
		tLastDebug = millis();
	}
#endif
}

/* Proof of concept simplex 3d noise blended with fog on frontbuffer - works but damn is it not practical
__ramfunc static void PostProcess_Fog( uint32_t const tNow )
{
	static constexpr float const NOISE_SCALAR_HEIGHT = 60.0f;
	
	// Find the min/max in [0.0..1.0] space
	vec2_t vBeginPt, vEndPt;
	
	{
		// convert grid to isometric coordinates
		point2D_t const voxelMinOrigin( ( oWorld.oCamera.voxelCurVisibilityMin ) ),  // these have already been pushed to (0,09) => (WORLD_GRID_SIZE,WORLD_GRID_SIZE)
										voxelMaxOrigin( ( oWorld.oCamera.voxelCurVisibilityMax ) );
		
		vBeginPt.x = (float)voxelMinOrigin.pt.x * Iso::INVERSE_WORLD_GRID_FSIZE;
		vBeginPt.y = (float)voxelMinOrigin.pt.y * Iso::INVERSE_WORLD_GRID_FSIZE;
		
		vEndPt.x = (float)voxelMaxOrigin.pt.x * Iso::INVERSE_WORLD_GRID_FSIZE;
		vEndPt.y = (float)voxelMaxOrigin.pt.y * Iso::INVERSE_WORLD_GRID_FSIZE;
	}
	
	OLED::RenderScreenLayer_DownSampleBy2<uint32_t>(OLED::getEffectFrameBuffer32bit_0_HalfSize(), DTCM::getFrontBuffer());

	
	float const fTNow( (float)tNow * 0.001f );
	int32_t yPixel(OLED::HALF_HEIGHT-1); // inverted y-axis

	vec2_t uv;
	while (yPixel >= 0)
	{
		int32_t xPixel(OLED::HALF_WIDTH-1);

		uint32_t * __restrict UserFrameBuffer = OLED::getEffectFrameBuffer32bit_0_HalfSize() + (yPixel << OLED::HalfWidth_SATBITS);

		uv.y = lerp(vBeginPt.y, vEndPt.y, static_cast<float>(yPixel) * OLED::INV_HALF_HEIGHT_MINUS1);

		while (xPixel >= 0)
		{
			uv.x = lerp(vBeginPt.x, vEndPt.x, static_cast<float>(xPixel) * OLED::INV_HALF_WIDTH_MINUS1);

			uint32_t const uiARGB = *UserFrameBuffer;
			
			// Get current Alpha and Shade of fog on front buffer [0.0=>255.0]
			uint32_t const Alpha = (0xFF000000 & uiARGB);
			
			float const fShade = (0x000000FF & uiARGB);
						
			// Get local noise [0.0=>1.0]
			float const fNoiseHeight = Noise::getSimplexNoise3D( NOISE_SCALAR_HEIGHT * uv.x, NOISE_SCALAR_HEIGHT * uv.y, fTNow );
			//float const fNoiseHeight = Noise::getBlueNoiseSample<float>( vec2_t(uv.x, uv.y) );//, fTNow );
			
			// Change fog....
			
			uint32_t const uiNewShade = __USAT( int32::__roundf( fNoiseHeight * fShade ), Constants::SATBIT_256 ); 
			*UserFrameBuffer = Alpha | (uiNewShade << 16) | (uiNewShade << 8) | (uiNewShade);
			++UserFrameBuffer;
			
			--xPixel;
		}
		
		--yPixel;
	}
	
	// Noise is generated equal in size of the grid dimensions,
	// however we only care about the noise from the voxels that are currently onscreen and visible
	// this localized visibilty of the world fog noise
	// is whhat we blend into the fog render target
	
	OLED::RenderScreenLayer_UpSampleBy2<uint32_t>(DTCM::getFrontBuffer(), OLED::getEffectFrameBuffer32bit_0_HalfSize());
}
*/


//#define DEBUG_RENDER

static void UpdateLighting( uint32_t const tNow ) 
{
	static uint32_t tLastLightRotation;
	
	Lighting::ActiveLighting.UpdateActiveLighting(tNow);
	
	if (tNow - tLastLightRotation > 16) {
		
		// Rotate light around
		
		{
		point2D_t const posLight = p2D_rotate(oWorld.lPrimary.Angle, v3_to_p2D_iso(vec3_t(129.0f, 90.0f, 90.0f)), point2D_t(128,32));
		oWorld.lPrimary.Position = vec3_t(posLight.pt.x, oWorld.lPrimary.Position.y, posLight.pt.y);
		/*if (nullptr != oWorld.m_pCurMissile ) {
			vec2_t vGridToScreen = oWorld.m_pCurMissile->vLoc;
			vGridToScreen = world::v2_GridToScreen(vGridToScreen);
			oWorld.lPrimary.Position = vec3_t(vGridToScreen.x, oWorld.lPrimary.Position.y, vGridToScreen.y);
		}*/
		}
		
		{
		//point2D_t const posLight = p2D_rotate(oWorld.lSecondary.Angle, v3_to_p2D_iso(vec3_t(128.0f, 90.0f, 64.0f)), point2D_t(128,32));
		//oWorld.lSecondary.Position = vec3_t(posLight.pt.x, oWorld.lSecondary.Position.y, posLight.pt.y);
		if (nullptr != oWorld.m_pCurExplosion ) { // must maintain light position if explosion active (eg.) camera pans)
			vec2_t vGridToScreen = oWorld.m_pCurExplosion->getOrigin();
			vGridToScreen = world::v2_GridToScreen(vGridToScreen);
			oWorld.lSecondary.Position = vec3_t(vGridToScreen.x, oWorld.lPrimary.Position.y, vGridToScreen.y);
		}
		}

#ifdef DEBUG_RENDER			
		{	
		
		static constexpr uint32_t const tUpdateDebugInterval = 44; //ms
			
		static uint32_t tLastChangeHighlight, uiAdjacentIndex(0);
			static Iso::Voxel* pLast(nullptr);
			static uint8_t LastOcclusionBits(0);
			
		if ( tNow - tLastChangeHighlight > tUpdateDebugInterval) {
			
			// Restore Original Occlusion bits
			if (nullptr != pLast)
				Iso::setOcclusion(*pLast, LastOcclusionBits);
			
			if ( ++uiAdjacentIndex == ADJACENT_NEIGHBOUR_COUNT )
				uiAdjacentIndex = 0;
			
			pLast = const_cast<Iso::Voxel* const>(getNeighbour( getScreenCenter_VoxelIndex(), ADJACENT[uiAdjacentIndex]));
			if (nullptr != pLast) {
				// save
				LastOcclusionBits = Iso::getOcclusion(*pLast);
				
				// set not visible
				Iso::setNotVisible(*pLast);
				
				point2D_t const voxelAdjacentToCenter( p2D_add(getScreenCenter_VoxelIndex(), ADJACENT[uiAdjacentIndex]) );
				point2D_t const voxelOrigin( p2D_add( Iso::p2D_GridToIso( p2D_sub(voxelAdjacentToCenter, oWorld.oCamera.voxelIndex_TopLeft) ), oWorld.oCamera.voxelOffset ) );
					
				oWorld.lSmallPoint.Position = vec3_t(voxelOrigin.pt.x, oWorld.lSmallPoint.Position.y, voxelOrigin.pt.y);
	
		static uint32_t tLastDebug, iterNum(0);

		char const* const szNBR[] = { "TL", "T", "TR", "R", "BR", "B", "BL", "L" };
		if (tNow - tLastDebug > tUpdateDebugInterval) {
			DebugMessage( "( %d, %d ), ( %s ) %d", voxelOrigin.pt.x, voxelOrigin.pt.y, 
										 szNBR[uiAdjacentIndex], iterNum++);
			tLastDebug = tNow;
		}

			}
			else {
				DebugMessage("Error Skipped");
			}
			tLastChangeHighlight = tNow;
		}
		
		}
#endif		
		oWorld.lPrimary.Angle += (tNow - tLastLightRotation) * (Constants::ConvTimeToFloat * 0.02f);
		oWorld.lSecondary.Angle -= (tNow - tLastLightRotation) * (Constants::ConvTimeToFloat * 0.03f);

		tLastLightRotation = tNow;
	}
}

static void UpdateFollow( uint32_t const tNow )
{
	static constexpr float const FOLLOW_SPEED = 0.0003f,
															 FOLLOW_DAMPING = 0.01f;
	
	// sample target position only every n ms defined by user in follow func
	if ( (tNow - oWorld.oCamera.tLastFollowUpdate) > oWorld.oCamera.tFollowUpdateInterval ) {
		oWorld.oCamera.vTarget = *oWorld.oCamera.FollowTarget;
		oWorld.oCamera.tLastFollowUpdate = tNow;
	}
	vec2_t const vDelta( v2_sub(oWorld.oCamera.vTarget, oWorld.oCamera.Origin) );
	
	vec2_t vVelocity( v2_fmas( FOLLOW_SPEED, vDelta, oWorld.oCamera.vFollowVelocity) );
	
	vVelocity = v2_muls( vVelocity, 1.0f - FOLLOW_DAMPING );
	
	oWorld.oCamera.Origin = v2_add( oWorld.oCamera.Origin, vVelocity );
}

static void UpdateCamera( uint32_t const tNow )
{
	static constexpr float const GRID_VOXELS_START_X_OFFSET = 7.0f * Iso::GRID_FRADII,	// in pixels units
															 GRID_VOXELS_START_Y_OFFSET = -GRID_VOXELS_START_X_OFFSET * 0.5f;	
		
	// if following a target, check if updating the camera's
	// heading should be updated. tFollowUpdateInterval defines the duration for the move itself and the 
	// interval in which the move target is updated. Provides a camera that can lag behind an object
	// and smoothly move to the points sampled from that objects location every interval.
	if ( nullptr != oWorld.oCamera.FollowTarget ) {
		UpdateFollow(tNow);
	}
	// outside of function, state is tested beforehand
	else if ( PENDING == oWorld.oCamera.State ) {

		uint32_t const tDelta = tNow - oWorld.oCamera.tMovePositionStart;
		float const tRelativeNormalized = (float)tDelta * oWorld.oCamera.tInvMoveTotalPosition;
		//if ( tTransition < oWorld.oCamera.tMoveTotalPosition ) {
		if ( (tRelativeNormalized - 1.0f) < 0.0f ) {
			oWorld.oCamera.Origin.x = smoothstep( oWorld.oCamera.PrevPosition.x, oWorld.oCamera.TargetPosition.x, 
																						tRelativeNormalized );
			oWorld.oCamera.Origin.y = smoothstep( oWorld.oCamera.PrevPosition.y, oWorld.oCamera.TargetPosition.y, 
																						tRelativeNormalized );
		}
		else {
			oWorld.oCamera.Origin = oWorld.oCamera.TargetPosition;
			oWorld.oCamera.State = LOADED; // flag as loaded for one frame
			//return;
		}
	}
	else if ( unlikely(LOADED == oWorld.oCamera.State) ){
		oWorld.oCamera.State = UNLOADED; // reset, was flag as loaded for one frame
		return;
	}
	else
		return;
	
	// get Origin's location in voxel units
	// range is (-144,-144) TopLeft, to (144,144) BottomRight
	//
	point2D_t voxelIndex( int32::__floorf(oWorld.oCamera.Origin.x), int32::__floorf(oWorld.oCamera.Origin.y) );
	
	// get starting voxel in TL corner of screen
	voxelIndex = p2D_sub(voxelIndex, point2D_t(WorldEntity::CameraEntity::GRID_VOXELS_MAXVISIBLE_X>>1, WorldEntity::CameraEntity::GRID_VOXELS_MAXVISIBLE_Y>>1));
	
	// ensure min and maximum bounds of world grid
	//voxelIndex.pt.x = min(voxelIndex.pt.x, Iso::MAX_VOXEL_COORD);
	//voxelIndex.pt.y = min(voxelIndex.pt.y, Iso::MAX_VOXEL_COORD);
	
	//voxelIndex.pt.x = max(voxelIndex.pt.x, Iso::MIN_VOXEL_COORD);
	//voxelIndex.pt.y = max(voxelIndex.pt.y, Iso::MIN_VOXEL_COORD);	
	
	// Change from(-x,-y) => (x,y)  to (0,0) => (x,y)
	oWorld.oCamera.voxelIndex_TopLeft = p2D_add(voxelIndex, point2D_t(Iso::WORLD_GRID_HALFSIZE, Iso::WORLD_GRID_HALFSIZE));
	oWorld.oCamera.voxelIndex_Center  = p2D_add(oWorld.oCamera.voxelIndex_TopLeft, 
																							point2D_t((WorldEntity::CameraEntity::GRID_VOXELS_MAXVISIBLE_X>>1) - 1, 
																												(WorldEntity::CameraEntity::GRID_VOXELS_MAXVISIBLE_Y>>1) - 1) );
	
	oWorld.oCamera.voxelIndex_vector_TopLeft = p2D_to_v2(oWorld.oCamera.voxelIndex_TopLeft);
	oWorld.oCamera.voxelIndex_vector_Center = p2D_to_v2(oWorld.oCamera.voxelIndex_Center);
	
	// Convert Fractional component from GridSpace
	vec2_t vFract( v2_sfract(oWorld.oCamera.Origin) );

	// was used with absolute fract
	//vFract.x = copysignf(vFract.x, oWorld.oCamera.Origin.x);
	//vFract.y = copysignf(vFract.y, oWorld.oCamera.Origin.y);
		
	vFract = v2_negate(vFract); // negation to offset in the correct direction of camera movement
	
	vFract = Iso::v2_GridToIso(vFract);
	
	// subtract static pixel offset (alignment of screen with visible voxels)
	vFract = v2_sub( vFract, vec2_t(GRID_VOXELS_START_X_OFFSET, GRID_VOXELS_START_Y_OFFSET) );
	 
	// Store as voxel pixel offset
	oWorld.oCamera.voxelOffset = v2_to_p2D( vFract );
	oWorld.oCamera.voxelOffset_vector = vFract;  // **** may need to be floored to be same as p2D offset
	
/*
	static uint32_t tLastUpdate;
	static bool bSkipOne(true);
	
	if ( false == bSkipOne ) {
	if ( tNow - tLastUpdate > 300 ) {
	DebugMessage("i(%d,%d) o(%d,%d)", oWorld.oCamera.voxelIndex.pt.x, oWorld.oCamera.voxelIndex.pt.y,
																									 oWorld.oCamera.voxelOffset.pt.x, oWorld.oCamera.voxelOffset.pt.y);
																									 //oWorld.oCamera.Origin.x, oWorld.oCamera.Origin.y );
		tLastUpdate = tNow;
	}
}

	bSkipOne = false;
*/
}

static void UpdateAI( uint32_t const tNow )
{
	// Run thru all bots for decision making
	for ( int32_t iDx = NUM_AIBOTS - 1 ; iDx >= 0 ; --iDx )
	{
		oWorld.m_bot[iDx].Update(tNow);
	}
	
	// If a turn is pending, do that bots turn
	if (tNow - oWorld.tLastTurn > TURN_INTERVAL )   /// every TURN_INTERVAL interval do a turn
	{
		oWorld.m_bot[oWorld.BotIndexPendingTurn].DoTurn(tNow);
		
		if ( ++oWorld.BotIndexPendingTurn == NUM_AIBOTS ) {
			oWorld.BotIndexPendingTurn = 0;
		}
		
		++oWorld.tLastTurn;
	}
}



// ####### Public Update & Render merthods
namespace world
{
uint32_t const getGroundHeight_InPixels(uint32_t const HeightStep)
{
	return(oWorld.groundHeightPixels[HeightStep]);
}
uint32_t const getBuildingHeight_InPixels(uint32_t const HeightStep)
{
	return(oWorld.buildingHeightPixels[HeightStep]);
}

Volumetric::voxB::voxelModelInstance_Dynamic* const __restrict getActiveMissile()
{
	return(oWorld.m_pCurMissile);
}
Volumetric::voxB::voxelModelInstance_Dynamic* const __restrict CreateMissile( uint32_t const tNow, vec2_t const WorldCoord, vec2_t const WorldCoordTarget )
{
	if ( oWorld.bDeferredInitComplete ) 
	{		
		if (nullptr != oWorld.m_pCurMissile) {
			// RESET FOLLOWING FIRST SO THAT DELETED MISSILE MEMORY IS NOT REFERENCED 
			resetFollow();
			delete oWorld.m_pCurMissile; oWorld.m_pCurMissile = nullptr;
		}
		
		oWorld.m_pCurMissile = new VoxMissile(tNow, WorldCoord, WorldCoordTarget);
		return(oWorld.m_pCurMissile);
	}
	return(nullptr);
}
void CreateExplosion( uint32_t const tNow, vec2_t const WorldCoord, bool const bShockwave ) 
{
	if (nullptr != oWorld.m_pCurExplosion) {
		
		if ( oWorld.m_pCurExplosion->isDead() ) {
			delete oWorld.m_pCurExplosion; oWorld.m_pCurExplosion = nullptr;
		}
		else
			return; // don't create until expiired
	}
	if ( bShockwave ) {
		if (nullptr != oWorld.m_pCurShockwave) {
			
			if ( oWorld.m_pCurShockwave->isDead() ) {
				delete oWorld.m_pCurShockwave; oWorld.m_pCurShockwave = nullptr;
			}
			else
				return; // don't create until expiired
			
		}
		
		oWorld.m_pCurShockwave = new ShockwaveInstance(WorldCoord, RandomNumber(5, 6));
	}
	oWorld.m_pCurExplosion = new ExplosionInstance(WorldCoord, RandomNumber(3, 4));
	
	static constexpr uint32_t const LightActivateDuration(2500), LightActivateEaseOutDuration(LightActivateDuration-250);
	Lighting::ActiveLighting.ActivateLight(WorldCoord, 1, tNow, LightActivateDuration, LightActivateEaseOutDuration);
	//Lighting::ActiveLighting.DeActivateLight(0, tNow, (LightActivateDuration) - Lighting::ActiveLighting.REACTIVATION_DURATION - Lighting::ActiveLighting.LIGHTING_UPDATE_RATE - 1);
	
	// the radius here corresponds to the maximum height could be used for normalization
}
__attribute__((pure)) Iso::Voxel const * const __restrict getVoxelAt( point2D_t voxelIndex )
{
	// Change from(-x,-y) => (x,y)  to (0,0) => (x,y)
	voxelIndex = p2D_add(voxelIndex, point2D_t(Iso::WORLD_GRID_HALFSIZE, Iso::WORLD_GRID_HALFSIZE));
	
	// Check bounds
	if ( (voxelIndex.pt.x | voxelIndex.pt.y) >= 0 ) {
		
		if ( voxelIndex.pt.x < Iso::WORLD_GRID_SIZE && voxelIndex.pt.y < Iso::WORLD_GRID_SIZE ) {
			
			return( (oWorld.theGrid + ((voxelIndex.pt.y * Iso::WORLD_GRID_SIZE) + voxelIndex.pt.x)) );
		}
	}
	
	return( nullptr );
}
__attribute__((pure)) Iso::Voxel const * const __restrict getVoxelAt( vec2_t const Location )
{
	point2D_t const voxelIndex( v2_to_p2D(Location) );				// this always floors, fractional part between 0.00001 and 0.99999 would
																																																		//          	equal same voxel	
	// still in Grid Space (-x,-y) to (X, Y) Coordinates 
	return(getVoxelAt(voxelIndex));
}

bool const setVoxelAt( point2D_t voxelIndex, Iso::Voxel const newData )
{
	// Change from(-x,-y) => (x,y)  to (0,0) => (x,y)
	voxelIndex = p2D_add(voxelIndex, point2D_t(Iso::WORLD_GRID_HALFSIZE, Iso::WORLD_GRID_HALFSIZE));
	
	// Check bounds
	if ( (voxelIndex.pt.x | voxelIndex.pt.y) >= 0 ) {
		
		if ( voxelIndex.pt.x < Iso::WORLD_GRID_SIZE && voxelIndex.pt.y < Iso::WORLD_GRID_SIZE ) {
			
			// Update Voxel
			*(oWorld.theGrid + ((voxelIndex.pt.y * Iso::WORLD_GRID_SIZE) + voxelIndex.pt.x)) = newData;
			return(true);
		}
	}
	
	return(false);
}
bool const setVoxelAt( vec2_t const Location, Iso::Voxel const newData )
{
	point2D_t const voxelIndex( v2_to_p2D(Location) );		// this always floors, fractional part between 0.00001 and 0.99999 would
																																																		//          	equal same voxel	
	// still in Grid Space (-x,-y) to (X, Y) Coordinates 
	return( setVoxelAt(voxelIndex, newData) );
}

__attribute__((pure)) Iso::Voxel const* const __restrict getNeighbour(point2D_t voxelIndex, point2D_t const relativeOffset)
{
	// Change from(-x,-y) => (x,y)  to (0,0) => (x,y)
	voxelIndex = p2D_add(voxelIndex, point2D_t(Iso::WORLD_GRID_HALFSIZE, Iso::WORLD_GRID_HALFSIZE));
	
	return( ::getNeighbour(voxelIndex, relativeOffset) );
}
__attribute__((pure)) uint32_t const& __restrict getCameraState()
{
	return(oWorld.oCamera.State);
}
__attribute__((pure)) point2D_t const p2D_GridToScreen(point2D_t thePt)
{
	// Change from(-x,-y) => (x,y)  to (0,0) => (x,y)
	thePt = p2D_add(thePt, point2D_t(Iso::WORLD_GRID_HALFSIZE, Iso::WORLD_GRID_HALFSIZE));
	
	return( p2D_add( Iso::p2D_GridToIso( p2D_sub(thePt, oWorld.oCamera.voxelIndex_TopLeft) ), oWorld.oCamera.voxelOffset ) );			
}
__attribute__((pure)) vec2_t const    v2_GridToScreen(vec2_t thePt)
{
	// Change from(-x,-y) => (x,y)  to (0,0) => (x,y)
	thePt = v2_add(thePt, vec2_t((float)Iso::WORLD_GRID_HALFSIZE, (float)Iso::WORLD_GRID_HALFSIZE));
	
	return( v2_add( Iso::v2_GridToIso( v2_sub(thePt, oWorld.oCamera.voxelIndex_vector_TopLeft) ), oWorld.oCamera.voxelOffset_vector ) );	
}

// Grid Space (-x,-y) to (X, Y) Coordinates Only
__attribute__((pure)) bool const isPointNotInVisibleVoxel( vec2_t const Location )
{
	// Whole part in (-x,-y)=>(x,y) GridSpace
	
	// Same Test that RenderGrid Uses, simpler
	return( TestPoint_Not_OnScreen( p2D_GridToScreen(v2_to_p2D(Location)) ) );
}

// Grid Space (-x,-y) to (X, Y) Coordinates Only
__attribute__((pure)) bool const getVoxelDepth( vec2_t const Location, 			// this function has not been fully tested but should work
																								Iso::Voxel const* __restrict& __restrict pVoxelOut, int32_t& __restrict Depth )
{
	point2D_t const voxelIndex( v2_to_p2D(Location) );		// Whole part in (-x,-y)=>(x,y) GridSpace
	
	Iso::Voxel const * const pVoxel = getVoxelAt(voxelIndex);
	
	pVoxelOut = pVoxel;		// Output the pointer to const Voxel (want nullptr aswell)
	
	if ( nullptr != pVoxel ) // Test
	{		
		point2D_t pixel;
		
		{
			point2D_t pixelOffset;							// Fractional part
			{
				// Get number of pixels to offset whole part in screenspace coordinates
				vec2_t vFract = v2_mul( v2_sfract(Location), vec2_t(Iso::GRID_FRADII_X2, Iso::GRID_FRADII) );
				// round off and convert to pixelOffset
				pixelOffset = point2D_t( int32::__roundf(vFract.x), int32::__roundf(vFract.y) );
			}
			
			// Get the screen space coordinates for this voxel (-x,-y)=>(x,y) GridSpace in for whole part, out ScreenSpace
			pixel = p2D_add( p2D_GridToScreen(voxelIndex), pixelOffset );
		}
		
		if ( false == OLED::TestStrict_Not_OnScreen(pixel) ) {
			// Test that point is on screen / within bounds of screen buffer (strict)
			int32_t const SampledDepth( DTCM::getDepthBuffer()[pixel.pt.x][pixel.pt.y] );	// depth buffer is rotated 90 in memory
			
			Depth = SampledDepth;
			return(true);
		}
		
	}
	
	return(false);
}

__attribute__((pure)) vec2_t const& __restrict getOrigin()
{
	return(oWorld.oCamera.Origin); // center of screen in (-x,-y) => (x,y) world gridspace coordinates
}

bool const moveTo( vec2_t const Dest, uint32_t const tMove )
{
	if ( Dest.x != oWorld.oCamera.Origin.x || Dest.y != oWorld.oCamera.Origin.y )
	{
		oWorld.oCamera.vFollowVelocity = vec2_t(0.0f); // only reset velocity here so we get spring bounce effect normally
		resetFollow(); // turn off following camera mode....
		
		// Save current position
		oWorld.oCamera.PrevPosition = oWorld.oCamera.Origin;
		
		// Save start time
		oWorld.oCamera.tMovePositionStart = millis();
		
		// Save new target position 
		oWorld.oCamera.TargetPosition = Dest;
		
		// Save amount of time for movement to transition
		oWorld.oCamera.tMoveTotalPosition = tMove;
		
		// Save pre-calculated (inverse) of total time
		oWorld.oCamera.tInvMoveTotalPosition = 1.0f / (float)oWorld.oCamera.tMoveTotalPosition;
		
		// Flag new camera state so it starts to change
		oWorld.oCamera.State = PENDING;
	}
	else
	{
		// Flag loaded for one frame
		oWorld.oCamera.State = LOADED;
		return(false); // let caller know that destination is same as current camera origin
	}
	return(true);
}

void follow( vec2_t const* const __restrict pvTarget, uint32_t const tUpdate )
{
	oWorld.oCamera.FollowTarget = pvTarget;
	oWorld.oCamera.tFollowUpdateInterval = tUpdate;
}
void resetFollow()
{
	oWorld.oCamera.FollowTarget = nullptr;
	oWorld.oCamera.tFollowUpdateInterval = 0;
}
__attribute__((pure)) cAIBot const& getPendingTurnAIBot()
{
	return(oWorld.m_bot[oWorld.BotIndexPendingTurn]);
}

__attribute__((pure)) uint32_t const getFogLevel()
{
	return(oWorld.FogHeight);
}
void setFogLevel(uint32_t const uiFogHeightInPixels)
{
	oWorld.FogHeight = uiFogHeightInPixels;
}

void Update( uint32_t const tNow )
{
	UpdateCamera(tNow);
	
	UpdateLighting(tNow);
	
	if (tNow - oWorld.tLastUpdateAI > WorldEntity::UPDATE_AI_INTERVAL) {
		UpdateAI(tNow);
		oWorld.tLastUpdateAI = tNow;
	}
	
	if (tNow - oWorld.tLastUpdatedTracking > WorldEntity::UPDATE_TRACKING_INTERVAL) {
		BuildingGen::UpdateTracking(tNow);
		oWorld.tLastUpdatedTracking = tNow;
	}
	
	if (nullptr != oWorld.m_pCurMissile ) {
		if ( false == oWorld.m_pCurMissile->Update( tNow ) ) {
	
			// RESET FOLLOWING FIRST SO THAT DELETED MISSILE MEMORY IS NOT REFERENCED 
			resetFollow();
			delete oWorld.m_pCurMissile; oWorld.m_pCurMissile = nullptr;

			world::CreateExplosion(tNow, world::getOrigin(), true);
		}
	}
	if (nullptr != oWorld.m_pCurExplosion) {
		if ( false == UpdateExplosion( tNow, oWorld.m_pCurExplosion ) ) {   // is alive(true), dead(false)
			delete oWorld.m_pCurExplosion; oWorld.m_pCurExplosion = nullptr;
		}
	}
	else {
//		world::CreateExplosion(world::getOrigin(), true);
	}
	
	if (nullptr != oWorld.m_pCurShockwave) {
		if ( false == UpdateShockwave( tNow, oWorld.m_pCurShockwave ) ) {   // is alive(true), dead(false)
			delete oWorld.m_pCurShockwave; oWorld.m_pCurShockwave = nullptr;
		}
	}
	
#ifdef ENABLE_SKULL
	SDF_Viewer::Update(tNow);
#endif
}

static void RenderFullScreenFire( uint32_t const tNow )
{
	static constexpr uint32_t const MAX_RANGE = 4096,
																	SEED_INTERVAL = 150;
	static constexpr float const INV_MAX_RANGE = 1.0f / (float)MAX_RANGE,
															 INV_SEED_INTERVAL = 1.0f / (float)SEED_INTERVAL,
															 SEED_LOWER_BOUND = 0.1f,
															 SEED_UPPER_BOUND = 1.0f;
	
	static float fCurSeed(0.0f), fNextSeed(1.0f);
	static uint32_t tLastChange(0);

	//static uint32_t tLastDebug(0);

	if ( tNow - tLastChange >= SEED_INTERVAL ) {
		// Time to change Next Seed
		
		fCurSeed = fNextSeed;
		fNextSeed = lerp(SEED_LOWER_BOUND, SEED_UPPER_BOUND, ((float)RandomNumber(0, MAX_RANGE)) * INV_MAX_RANGE);
		
		//if ( 0 != tLastDebug )
		//	DebugMessage(" %.04f (%.04f => %.04f) %.04f", fSeed, fCurSeed, fNextSeed, fSeed - fCurSeed);
		//tLastDebug = tNow;
		
		tLastChange = tNow;
	}

	RenderFire((float)tNow * 0.001f, lerp(fCurSeed, fNextSeed, clampf( (float)(tNow - tLastChange) * INV_SEED_INTERVAL )));
}

static void RenderFullscreenFractalBoxes( uint32_t const tNow )
{
	
	static constexpr uint32_t const UV_INTERVAL = 25,
																  TRANSITION_TIME = 120 * 1000;			// ms
	static constexpr float const MAX_RANGE = 512.0f, MIN_RANGE = 256.0f,
															 INV_TRANSITION_TIME = 1.0f / ((float)TRANSITION_TIME); // 1 / ms
																	
	static float fUVScalar;
	static uint32_t tLastChange(0), tStart;
	static bool bIncreasing(false);
	
	if ( tNow - tLastChange > UV_INTERVAL ) {
		
		uint32_t const tDelta = tNow - tStart;
		if ( tDelta >= TRANSITION_TIME) {
			bIncreasing = !bIncreasing;
			tStart = tNow;
		}
		else {
			fUVScalar = lerp(MIN_RANGE, MAX_RANGE, (bIncreasing ? (float)(tDelta) * INV_TRANSITION_TIME
																													: 1.0f - ((float)(tDelta) * INV_TRANSITION_TIME)));
			
		}
		tLastChange = tNow;
	}
	
	RenderFractalBoxes((float)tNow * 0.001f, fUVScalar);
}

static void RenderFullscreenFog( uint32_t const tNow )
{	
	RenderFogSprite((float)tNow * 0.001f);
}
static void RenderFullscreenShockwave( uint32_t const tNow )
{	
//	RenderShockwavePP((float)tNow * 0.001f);
}

void Render( uint32_t const tNow )
{

	/*
	if (UNIT_EFFECT == oWorld.iCurEffect)
	{
		oWorld.iCurEffect = STARTING_EFFECT;
		oWorld.tStartEffect = tNow;
	}
	else if (tNow - oWorld.tStartEffect > EFFECT_DURATION ) {
		// change effect
		if ( ++oWorld.iCurEffect == NUM_EFFECTS ) {
			oWorld.iCurEffect = FIRE_EFFECT; // reset
		}
		
		oWorld.tStartEffect = tNow;
	}
	
	switch( oWorld.iCurEffect ) {
		case FIRE_EFFECT:
			RenderFullScreenFire(tNow);
			break;
		case FRACTALBOXES_EFFECT:
			RenderFullscreenFractalBoxes(tNow);
			break;
		case FOG_EFFECT:
			RenderFullscreenFog(tNow);
			break;
		case SHOCKWAVE_EFFECT:
			RenderFullscreenShockwave(tNow);
			break;
		default:
			oWorld.iCurEffect = FIRE_EFFECT; // reset
			oWorld.tStartEffect = tNow;
			break;
	}
	*/
//	RenderFullscreenFog(tNow);
	
#ifndef ENABLE_SKULL

	RenderGrid<OLED::SHADE_ENABLE|OLED::Z_ENABLE|OLED::ZWRITE_ENABLE|OLED::FOG_ENABLE>();

	if (nullptr != oWorld.m_pCurExplosion) {
		
		//PROFILE_START();
		if (nullptr != oWorld.m_pCurShockwave) {
			
			if ( isShockwaveVisible( oWorld.m_pCurShockwave ) ) {
				RenderShockwave( oWorld.m_pCurShockwave );
			}
		}
		if ( isExplosionVisible( oWorld.m_pCurExplosion ) ) {
			RenderExplosion( oWorld.m_pCurExplosion );
		}
		
		//PROFILE_END(600);
	}
	
	if (nullptr != oWorld.m_pCurMissile) {
		
		if ( oWorld.m_pCurMissile->isVisible() ) {
			oWorld.m_pCurMissile->Render();
		}
	}
	
#ifdef RENDER_DEPTH
	static uint32_t tLastRenderedDepthToggle;
	static bool bDepthRender(true);
	
	if (tNow - tLastRenderedDepthToggle > 33000) {
		bDepthRender = !bDepthRender;
		tLastRenderedDepthToggle = tNow;
	}

	if (bDepthRender)
		OLED::RenderDepthBuffer();
#endif
	
#ifdef RENDER_BLOOMHDRSOURCE
	static uint32_t tLastRenderedBloomHDRToggle;
	static bool bBloomHDRSourceRender(true);
	
	if (tNow - tLastRenderedBloomHDRToggle > 33000) {
		bBloomHDRSourceRender = !bBloomHDRSourceRender;
		tLastRenderedBloomHDRToggle = tNow;
	}

	if (bBloomHDRSourceRender)
		OLED::RenderBloomHDRBuffer();
#endif
#endif // skull
	//OLED::Effects::GaussianBlur_32bit(OLED::getEffectFrameBuffer32bit(), DTCM::getFrontBuffer(), 1);
	// overwrite frontbuffer
	//OLED::Render32bitScreenLayer_Front( OLED::getEffectFrameBuffer32bit() );
	
	//xDMA2D::ClearBuffer_32bit< (OLED::SCREEN_WIDTH>>1), (OLED::SCREEN_WIDTH>>1) >(OLED::getEffectFrameBuffer32bit_0_HalfSize());
	//xDMA2D::ClearBuffer_32bit< (OLED::SCREEN_WIDTH>>1), (OLED::SCREEN_WIDTH>>1) >(OLED::getEffectFrameBuffer32bit_1_HalfSize()); 
	//OLED::RenderScreenLayer_DownSampleBy2<uint32_t>(OLED::getEffectFrameBuffer32bit_0_HalfSize(), DTCM::getFrontBuffer());
	//OLED::Effects::GaussianBlur_32bit(OLED::getEffectFrameBuffer32bit_1_HalfSize(), OLED::getEffectFrameBuffer32bit_0_HalfSize(), 3);
  //OLED::RenderScreenLayer_UpSampleBy2<uint32_t>(DTCM::getFrontBuffer(), OLED::getEffectFrameBuffer32bit_1_HalfSize());
	//OLED::Render32bitScreenLayer_Front( OLED::getEffectFrameBuffer32bit_0_HalfSize() );
	//OLED::RenderScreenLayer_Upscale<uint32_t, Constants::SATBIT_128, Constants::SATBIT_32>(DTCM::getFrontBuffer(), OLED::getEffectFrameBuffer32bit_HalfSize());
	//OLED::Effects::GaussianBlur_8bit(OLED::getEffectFrameBuffer_1(), DTCM::getBackBuffer(), 3);
	// copy back to backbuffer
	//OLED::Render8bitScreenLayer_Back( OLED::getEffectFrameBuffer_1() );
	
	//OLED::Effects::KawaseBlur_8bit(OLED::getEffectFrameBuffer_1(), DTCM::getBackBuffer(), 1);
	// copy back to backbuffer
	//OLED::Render8bitScreenLayer_Back( OLED::getEffectFrameBuffer_1() );
	
#ifdef ENABLE_SKULL

	xDMA2D::Wait_DMA2D<false>();
	SDF_Viewer::Render(tNow);

#endif

}

} // endnamespace

