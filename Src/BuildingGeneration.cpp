#include "globals.h"
#include "BuildingGeneration.h"
#include "world.h"
#include <bitset>
#include "noise.h"
#include <list>

#include "debug.cpp"

//#define DEBUG_TRACKING
         
static constexpr float const PLOT_SIZE_NOISE_SCALE = 72.0f, // may be too high for got result
														 PLOT_BUILDING_HEIGHT_NOISE_SCALE = 96.0f;

static uint8_t PerlinPermutationsBuildingHeight[Noise::PERMUTATION_STORAGE_SIZE]			// 512 bytes
		__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
		__attribute__((section (".dtcm")));

namespace BuildingGen
{

NOINLINE void Init( uint32_t const tNow )
{
	// Initialize noise but revert pointer to default native permutation storage
	Noise::NewNoisePermutation(PerlinPermutationsBuildingHeight);
	Noise::SetDefaultNativeDataStorage(); // above is now filled with data, just don't need it quite yet
}

uint32_t const GenPlotSizeForLocation( point2D_t voxelIndex )
{
	uint32_t uiReturnPlotSize(0);
	
	// ensure min and maximum bounds of world grid
	voxelIndex.pt.x = min(voxelIndex.pt.x, Iso::MAX_VOXEL_COORD);
	voxelIndex.pt.y = min(voxelIndex.pt.y, Iso::MAX_VOXEL_COORD);
	
	voxelIndex.pt.x = max(voxelIndex.pt.x, Iso::MIN_VOXEL_COORD);
	voxelIndex.pt.y = max(voxelIndex.pt.y, Iso::MIN_VOXEL_COORD);	
	
	// Change from(-x,-y) => (x,y)  to (0,0) => (x,y)
	voxelIndex = p2D_add(voxelIndex, point2D_t(Iso::WORLD_GRID_HALFSIZE, Iso::WORLD_GRID_HALFSIZE));
	// normalize range aswell
	vec2_t vPosition( p2D_to_v2(voxelIndex) );
	vPosition = v2_mul(vPosition, vec2_t(Iso::INVERSE_WORLD_GRID_FSIZE, Iso::INVERSE_WORLD_GRID_FSIZE));
	vPosition = v2_muls( vPosition, PLOT_SIZE_NOISE_SCALE );
	
	// Based off of value noise equal to the world grid in size
	Noise::SetDefaultNativeDataStorage();
	float const fNoisePlot = Noise::getValueNoise( vPosition.x, vPosition.y, Noise::interpolator::Fade() );
	
	if ( fNoisePlot > 0.75f ) {
		uiReturnPlotSize = PLOT_SIZE_4x4;
	}
	else if ( fNoisePlot > 0.5f ) {
		uiReturnPlotSize = PLOT_SIZE_3x3;
	}
	else if ( fNoisePlot > 0.16f ) {
		uiReturnPlotSize = PLOT_SIZE_2x2;
	}
	else {
		uiReturnPlotSize = PLOT_SIZE_1x1;
	}
	
	return(uiReturnPlotSize);
}

NOINLINE bool const GenPlot( point2D_t& minIndex, point2D_t& maxIndex, point2D_t const closestToIndex, point2D_t const plotSize )
{
	static constexpr uint32_t const NUM_ROWS = MAX_PLOT_XY + MAX_PLOT_XY,
																	NUM_COLS = NUM_ROWS,
																	BITSET_HEIGHT = NUM_ROWS,
																	BITSET_WIDTH = NUM_COLS,
																	OFFSET = MATRIX_RADII;
																	
	
	// Bitset matrix is based on maximum space taken up by
	// 1.) the source (closest) plot		MAX_PLOT_X
	// 2.) the "road" in betweeb space	1
	// 3.) the maximum plot size for the new plot
	// Matrix is an array of bitsets n rows in height, each bitset is n cols in width
	
	static std::bitset<BITSET_WIDTH> matrixAdjacency[BITSET_HEIGHT],
																	 matrixTest
	__attribute__((aligned(ARMV7M_DCACHE_LINESIZE)))
	__attribute__((section (".dtcm")));
	
	/*
		# # # # | # # # #
		# # # # | # # # #
		# # # # | # # # #
		# # # # | # # # #
		- - - - # - - - -
		# # # # | # # # #
		# # # # | # # # #
		# # # # | # # # #
		# # # # | # # # #
		
		The middle is the closest point paased in arguments 
	*/
	
		
	// Build Matrix, cuurrently looking for tiles that are ground, and zero height
	for ( int32_t iDy = BITSET_HEIGHT - 1 ; iDy >= 0 ; --iDy ) {
		
		int32_t const yDistance = iDy - OFFSET;
		
		for ( int32_t iDx = BITSET_WIDTH - 1 ; iDx >= 0 ; --iDx ) {
			
			int32_t const xDistance = iDx - OFFSET;
			
			// Get "Neighbour" or IsoVoxel that is this distance from current point
			Iso::Voxel const* const __restrict pNeighbour = world::getNeighbour( closestToIndex, point2D_t(xDistance, yDistance) );
			if ( nullptr != pNeighbour ) // neighbour is within bounds of world grid
			{
				Iso::Voxel const oNeighbour(*pNeighbour);	// copy out to register
				
				if ( isVoxelVisible(oNeighbour) && Iso::DESC_HEIGHT_STEP_0 == Iso::getHeightStep(oNeighbour) ) {
					// set the bit in the matrix as a compatible tile for the desired plot
					matrixAdjacency[iDy][iDx] = true;
				}
				else {
					matrixAdjacency[iDy][iDx] = false;
				}
			}
			else {
				matrixAdjacency[iDy][iDx] = false;
			}
		}
	} // for build matrix
	
	uint32_t uiRowSize(0),
					 uiBeginiDx(0), uiEndiDx(0),
					 uiReturnSize(0);

	// Test nxn //
	for ( uint32_t iDz = 0 ; iDz < BITSET_HEIGHT - (plotSize.pt.y - 1) ; ++iDz )
	{		
		// Test Number of Consecutive Bits //
		for ( uint32_t iDx = 0 ; iDx < BITSET_WIDTH ; ++iDx )
		{
			if ( matrixAdjacency[iDz][iDx] )
			{
				++uiRowSize;
				if ( plotSize.pt.x == uiRowSize )
					break;
			}
			else
				uiRowSize = 0;
		}

		if ( plotSize.pt.x == uiRowSize )
		{
			uiRowSize = 0;

			// And Next n Rows
			matrixTest = matrixAdjacency[iDz];
			for ( uint32_t iDzTest = 1 ; iDzTest < plotSize.pt.y ; ++iDzTest )
			{
				matrixTest &= matrixAdjacency[iDz + iDzTest];
			}

			// Test Number of Consecutive Bits //
			for ( uint32_t iDx = 0 ; iDx < BITSET_WIDTH ; ++iDx )
			{
				if ( matrixTest[iDx] )
				{
					++uiRowSize;
					if ( plotSize.pt.x == uiRowSize )
						break;
				}
				else
					uiRowSize = 0;
			}

			// Do we have enough Matching Rows ?
			if ( uiRowSize >= plotSize.pt.x )
			{
				uiBeginiDx = 0;
				uiEndiDx = 0;
					
				for ( uint32_t iDx = 0 ; iDx < BITSET_WIDTH ; ++iDx )
				{
					if ( matrixTest[iDx] )
					{
						++uiEndiDx;
					}
					else
					{
						if ( uiEndiDx >= plotSize.pt.x )
						{
							// Done //
							break;
						}
						else
						{
							uiBeginiDx = iDx + 1;
							uiEndiDx = 0;
						}
					}
				}

				// Return //
				point2D_t const beginPoint(uiBeginiDx, iDz),
												endPoint( p2D_add(beginPoint, plotSize) );
				
				// Apply Offset from matrix layout
				// Add back in the voxelIndex of the middle of the matrix layout
				minIndex = p2D_add(closestToIndex, p2D_subs(beginPoint, OFFSET));
				maxIndex = p2D_add(closestToIndex, p2D_subs(endPoint, OFFSET));	

				return(true);
			}
		}
		uiRowSize = 0;
	}

	return(false);
}

} // end namespace

typedef struct sBuildingTracker
{
	uint32_t 		    m_timeStamp;
	point2D_t const m_voxelIndex;
	uint8_t const		m_TargetHeightStep;
	
	explicit sBuildingTracker(point2D_t const voxelIndex, uint32_t const tStart, uint32_t const TargetHeightStep)
		: m_timeStamp(tStart), m_voxelIndex(voxelIndex), m_TargetHeightStep(TargetHeightStep)
	{}
	
	// Only allow move construcyot
	sBuildingTracker(sBuildingTracker&&) = default;
		
	sBuildingTracker() = delete;
	sBuildingTracker(sBuildingTracker const&) = delete;
	void operator=(sBuildingTracker const&) = delete;
	void operator=(sBuildingTracker&&) = delete;
} BuildingTracker;

static std::list<BuildingTracker> _vectorTracking
	__attribute__((section (".bss")));

NOINLINE static void TrackBuilding( point2D_t const rootVoxelIndex, uint32_t const tStartTracking, uint32_t const TargetHeightStep )
{
	_vectorTracking.push_back( sBuildingTracker(rootVoxelIndex, tStartTracking, TargetHeightStep) );
}

namespace BuildingGen
{
	
NOINLINE void UpdateTracking(uint32_t const tNow)
{
	static constexpr uint32_t const BUILDING_STEP_INCREASE_INTERVAL = 800, //ms should be a multiple of UPDATE_TRACKING_INTERVAL
																	BUILDING_STEP_DECREASE_INTERVAL = 400;
	{ // Tracking
		std::list<BuildingTracker>::iterator iter(_vectorTracking.begin());
		while ( iter != _vectorTracking.end() ) {
			
			BuildingTracker& rBuild(*iter);
			bool bDeleteTracker(false), bIsBuildingOrCollapsing(0 != rBuild.m_TargetHeightStep);	// true = building
			
			uint32_t const StepInterval = (bIsBuildingOrCollapsing ? BUILDING_STEP_INCREASE_INTERVAL :  BUILDING_STEP_DECREASE_INTERVAL);
			
			if ( tNow - rBuild.m_timeStamp > StepInterval ) {
				
				// Test
				Iso::Voxel const * const __restrict pVoxel( world::getVoxelAt( rBuild.m_voxelIndex ) );
				
				if ( nullptr != pVoxel )
				{
					// Read (Copy)
					Iso::Voxel oVoxel(*pVoxel);
						
					uint32_t HeightStep = Iso::getHeightStep( oVoxel );
					
					if ( bIsBuildingOrCollapsing ) {
						if ( HeightStep < rBuild.m_TargetHeightStep ) { // sanity check exception
						
							if ( ++HeightStep == rBuild.m_TargetHeightStep ) {
								bDeleteTracker = true; // delete tracking of voxel, target us reached, update target before deletion of tracking first
							}
							
							// either comment above or still in process of reaching height step
							
							// Modify
							Iso::setHeightStep(oVoxel, HeightStep);
							
							// Write
							world::setVoxelAt( rBuild.m_voxelIndex, oVoxel );
						}
						else {
							bDeleteTracker = true; // delete tracking of voxel who is already at target
						}
					}
					else { // collapsing
						if (0 == HeightStep ) { // sanity check exception
						
							if ( 0 == --HeightStep ) {
								bDeleteTracker = true; // delete tracking of voxel, target us reached, update target before deletion of tracking first
							}
							
							// either comment above or still in process of reaching height step
							
							// Modify
							Iso::setHeightStep(oVoxel, HeightStep);
							
							// Write
							world::setVoxelAt( rBuild.m_voxelIndex, oVoxel );
						}
						else {
							bDeleteTracker = true; // delete tracking of voxel who is already at target
						}
					}
					
				}
				else {
					bDeleteTracker = true; // delete tracking for voxel that does not exist
				}
				
				if ( bDeleteTracker ) { // remove trackers
					
					if ( !bIsBuildingOrCollapsing ) { // collapsing
						// Change from collapsing to collapsed flag
						// Read (Copy)
						Iso::Voxel oVoxel(*pVoxel);
						// Nodify
						Iso::setDestruction_Collapsed(oVoxel);
						// Write
						world::setVoxelAt( rBuild.m_voxelIndex, oVoxel );
					}
					
					iter = _vectorTracking.erase(iter);
				}
				else {
					rBuild.m_timeStamp = tNow;	// update time stamp, new interval to start from
					++iter;
				}
				
			} // test time interval
			else
				++iter;
		} // while
	} // end tracking
	
#ifdef DEBUG_TRACKING
	uint32_t const uiTotal = _vectorTracking.size();
#endif

#ifdef DEBUG_TRACKING
	static uint32_t tLastDebug;
	uint32_t const uiActive = _vectorTracking.size();
	if ( uiActive != uiTotal || (tNow - tLastDebug > 420) ) {
		DebugMessage("Tracking, Total( %d ), Active( %d )", uiTotal, uiActive);
		tLastDebug = tNow;
	}
#endif
}
// Generate Building :
NOINLINE void GenPlotStructure( uint32_t const tNow, point2D_t const minIndex, point2D_t const maxIndex )
{
	// currently only level zero height supported for building plots
	Noise::SetPermutationDataStorage(PerlinPermutationsBuildingHeight); // setup usage of unique permutations for height
		
	// "root" of building plot, is always BR corner (max x, max y)
	point2D_t const rootVoxelIndex(maxIndex.pt.x - 1, maxIndex.pt.y - 1);
	uint32_t const uiPlotSize( maxIndex.pt.x - minIndex.pt.x );
	
	uint32_t rootHeightStep(0);
	
	uint32_t uNoiseHeight;
	{
		// Change from(-x,-y) => (x,y)  to (0,0) => (x,y)
		// normalize range aswell
		vec2_t vPosition( p2D_to_v2(p2D_add(rootVoxelIndex, point2D_t(Iso::WORLD_GRID_HALFSIZE, Iso::WORLD_GRID_HALFSIZE))) );
		vPosition = v2_mul(vPosition, vec2_t(Iso::INVERSE_WORLD_GRID_FSIZE, Iso::INVERSE_WORLD_GRID_FSIZE));
		vPosition = v2_muls( vPosition, PLOT_BUILDING_HEIGHT_NOISE_SCALE );
		
		float const fNoisePlotBuilding = Noise::getValueNoise( vPosition.x, vPosition.y, Noise::interpolator::Fade() );
		
		uNoiseHeight = __USAT( fNoisePlotBuilding * Constants::nf255, Constants::SATBIT_256 );
		
		// Divide returned Noise evenly into heightsteps
		if (uNoiseHeight > BUILDING_HEIGHT_NOISE_7) {
			rootHeightStep = 7;
		}
		else if (uNoiseHeight > BUILDING_HEIGHT_NOISE_6) {
			rootHeightStep = 6;
		}
		else if (uNoiseHeight > BUILDING_HEIGHT_NOISE_5) {
			rootHeightStep = 5;
		}
		else if (uNoiseHeight > BUILDING_HEIGHT_NOISE_4) {
			rootHeightStep = 4;
		}
		else if (uNoiseHeight > BUILDING_HEIGHT_NOISE_3) {
			rootHeightStep = 3;
		}
		else if (uNoiseHeight > BUILDING_HEIGHT_NOISE_2) {
			rootHeightStep = 2;
		}
		else if (uNoiseHeight > BUILDING_HEIGHT_NOISE_1) {
			rootHeightStep = 1;
		}
		else if ( uiPlotSize <= 1 ) {
			rootHeightStep = 0;   // let there be small vacant plots
		}
	}
	
	for ( int32_t iDy = minIndex.pt.y ; iDy < maxIndex.pt.y ; ++iDy )
	{
		for ( int32_t iDx = minIndex.pt.x ; iDx < maxIndex.pt.x ; ++iDx )
		{
			point2D_t const voxelIndex = point2D_t(iDx, iDy);
			
			// Test
			Iso::Voxel const* const __restrict pVoxel = world::getVoxelAt( voxelIndex );
			
			if (nullptr != pVoxel)
			{
				// Read (Copy)
				Iso::Voxel oVoxel(*pVoxel);;
				
				// Modify
				Iso::setType(oVoxel, Iso::TYPE_STRUCTURE); 
				
				Iso::setPlotSize(oVoxel, uiPlotSize);
				
				if (rootVoxelIndex.pt.y == iDy && rootVoxelIndex.pt.x == iDx) {
					Iso::setPlotPartType( oVoxel, Iso::ROOT_PLOT_PART );
					Iso::setHeightStep(oVoxel, 0);
					TrackBuilding(rootVoxelIndex, tNow, rootHeightStep);
				}
				else {
					Iso::setPlotPartType( oVoxel, Iso::SIBLING_PLOT_PART );
					Iso::setHeightStep(oVoxel, rootHeightStep); // siblings already know the final height
				}			
				
				// Write
				world::setVoxelAt( voxelIndex, oVoxel );
			}
		}
	}
	
	// revert, play nice
	Noise::SetDefaultNativeDataStorage();
}

} // end namespace