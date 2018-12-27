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
#include "AiBot.h"
#include "rng.h"
#include "BuildingGeneration.h"
#include "world.h"

#include "debug.cpp"

static constexpr uint32_t const VAL_STEP_SMALL = 10,
																VAL_STEP_MEDIUM = 40,
																VAL_STEP_LARGE = 80;

// random number levels / also weighting
static constexpr uint32_t const RAND_NORMAL_MAX = 1000,
																RAND_WEIGHT_MORE_MAX = 1333,
																RAND_WEIGHT_LESS_MAX = 666;
// Frustration levels
static constexpr uint32_t const NOT_FRUSTRATED = 0,
																GETTING_PISSED_OFF = 100,
																FUCKING_PISSED_OFF = 200,
																LOOSING_IT_TOTALLY = 300,
																MY_NAME_IS_SATAN   = 400,
																WTF_BLOW_SHIT_FU_MOTHERFUCKER = 500;
// Root descisions
static constexpr uint32_t const DECIDE_DEFAULT = 0,
																DECIDE_BUILD = (1 << 1),
																DECIDE_DEFEND = (1 << 2),
																DECIDE_ATTACK = (1 << 3),
																DECIDE_SKIPTURN = (1 << 4);

// sub-root descisions


cAIBot::cAIBot( point2D_t const StartLocation )
	: m_Start(StartLocation), m_CurBuildPoint(StartLocation), m_lastPlotSizeAttempted(0, 0),
m_Frustration(0), m_rootDescision(0), m_subDescision(0),
m_lastRootDescision(0), m_lastSubDescision(0), m_DescisionStrength(0),
m_bDescisionSuccessful(false), m_bTurnSuccessful(false),
m_BuildLayers(0), m_nextMajorNeighbour(-1)
{	
}

uint32_t const& cAIBot::IncreaseDescisionStrength( uint32_t const Amount ) 
{
	m_DescisionStrength += Amount;
	return(m_DescisionStrength);
}
uint32_t const& cAIBot::IncreaseFrustration( uint32_t const Amount ) 
{
	m_Frustration += Amount;
	return(m_Frustration);
}
uint32_t const& cAIBot::DecreaseFrustration( uint32_t const Amount ) 
{
	int32_t iFrustration(m_Frustration);
	// clamp
	iFrustration -= (int32_t)Amount;
	iFrustration = max(0, iFrustration);
	
	m_Frustration = iFrustration;
	
	return(m_Frustration);
}

void cAIBot::MoveBuildPoint()
{
	// Find valid neighbours from the start position (which is in the corner of the map)
	/*

											NBR_TL
							NBR_L						NBR_T
		 NRB_BL						VOXEL						NBR_TR
							NBR_B						NRB_R		
											NBR_BR
											
*/	
	if ( -1 == m_nextMajorNeighbour ) // Move Up to next layer 
	{
		int32_t NextNeighbourDirection(m_nextMajorNeighbour);
		{
			Iso::Voxel const* pNeighbour(nullptr);
			do
			{
				if ( world::ADJACENT_NEIGHBOUR_COUNT == ++NextNeighbourDirection )
					return; // no neighbours are valid quit
				
				pNeighbour = world::getNeighbour( m_Start, world::ADJACENT[NextNeighbourDirection] );
				
			} while( nullptr == pNeighbour );
		}
		
		{
			// Save Direction as first step into the next layer
			point2D_t const vDir( world::ADJACENT[NextNeighbourDirection] );
			
			uint32_t const pendingBuildLayers = m_BuildLayers + 1;
			point2D_t const pendingBuildPoint = p2D_muls( vDir, (BuildingGen::MATRIX_RADII << 1) * pendingBuildLayers );
				
			// Bounds Test
			if (pendingBuildPoint.pt.x < Iso::MIN_VOXEL_COORD || pendingBuildPoint.pt.y < Iso::MIN_VOXEL_COORD ||
					pendingBuildPoint.pt.x > Iso::MAX_VOXEL_COORD || pendingBuildPoint.pt.y > Iso::MAX_VOXEL_COORD) {
						
				// Special Case, have hit world bounds area, reset build point to starting location
				m_CurBuildPoint = m_Start.v;
				m_nextMajorNeighbour = -1; // reset
				return;
			}
						
			// Setup initial new build point
			m_CurBuildPoint = pendingBuildPoint.v;
			m_BuildLayers = pendingBuildLayers;
		}
		// This represents the "average" value for this entire plot, weighting each building's desired plot size
		m_CurPlotAvgDesiredPlotSize = BuildingGen::GenPlotSizeForLocation(m_CurBuildPoint); 
		
		// Change Direction to be inline, snaking around in clockwise order
		if ( ++NextNeighbourDirection < world::ADJACENT_NEIGHBOUR_COUNT )
		{
			m_nextMajorNeighbour = NextNeighbourDirection;		
		}
		else
			m_nextMajorNeighbour = -1;
		
		//DebugMessage("New Build Point & Laxer ( %d, %d ) %d", m_CurBuildPoint.pt.x, m_CurBuildPoint.pt.y, m_BuildLayers );
	}
	else
	{
		// Advance Current Build point, to next neighbour in direction denoted by m_lastBuildPointMoveDirection
		uint32_t SpecialCaseLastCorner(0);
		int32_t NextNeighbourDirection(m_nextMajorNeighbour);
		point2D_t vDir(0,0);
		
		switch(NextNeighbourDirection)
		{
			case world::NBR_T:
			case world::NBR_TR:	
				// push x
				vDir.pt.x = 1;
				break;
			case world::NBR_R:
			case world::NBR_BR:
				// pull y
				vDir.pt.y = -1;
				break;
			case world::NBR_B:
			case world::NBR_BL:
				// pull x
				vDir.pt.x = -1;
				break;
			case world::NBR_L:
			case world::NBR_TL:
				// push y
				vDir.pt.y = 1;
				SpecialCaseLastCorner = 1;
				break;
		}
		
		{
			point2D_t const pendingBuildPoint = p2D_add( m_CurBuildPoint, p2D_muls( vDir, (BuildingGen::MATRIX_RADII << 1) ) );
			
			// Bounds Test
			if (pendingBuildPoint.pt.x < Iso::MIN_VOXEL_COORD || pendingBuildPoint.pt.y < Iso::MIN_VOXEL_COORD ||
				  pendingBuildPoint.pt.x > Iso::MAX_VOXEL_COORD || pendingBuildPoint.pt.y > Iso::MAX_VOXEL_COORD) {
						
				// Special Case, have hit world bounds area, reset build point to starting location
				m_CurBuildPoint = m_Start.v;
				m_nextMajorNeighbour = -1; // reset
				return;
			}

			// Save the new build point
			m_CurBuildPoint = pendingBuildPoint.v;
			// This represents the "average" value for this entire plot, weighting each building's desired plot size
			m_CurPlotAvgDesiredPlotSize = BuildingGen::GenPlotSizeForLocation(m_CurBuildPoint); 
		}
		
		point2D_t MajorNeighbourDest = p2D_muls( world::ADJACENT[NextNeighbourDirection], (BuildingGen::MATRIX_RADII << 1) * (m_BuildLayers)  );
		if (0 != SpecialCaseLastCorner) {
			MajorNeighbourDest.pt.y += ((BuildingGen::MATRIX_RADII << 1) * m_BuildLayers);
		}
		// if equal, advance next neighbour
		if ( (MajorNeighbourDest.v == m_CurBuildPoint.v) )
		{
			if ( ++NextNeighbourDirection < world::ADJACENT_NEIGHBOUR_COUNT )
			{
				m_nextMajorNeighbour = NextNeighbourDirection;
			}
			else
				m_nextMajorNeighbour = -1;
		}
		
		//DebugMessage("New Build Point  ( %d, %d ) to ( %d, %d )", m_CurBuildPoint.pt.x, m_CurBuildPoint.pt.y, MajorNeighbourDest.pt.x, MajorNeighbourDest.pt.y );
	}
}
void cAIBot::MakeSubDescision_Build()
{
	point2D_t desiredSize(BuildingGen::MIN_PLOT_XY, BuildingGen::MIN_PLOT_XY); // init with maximum possibilty for successfuly finding plot
	
	if ( BuildingGen::MIN_PLOT_XY == m_lastPlotSizeAttempted.pt.x && BuildingGen::MIN_PLOT_XY == m_lastPlotSizeAttempted.pt.y )
	{
		// Change build point .....
		MoveBuildPoint();
	}
	else
	{
		// Set to reset value, or if after multiple attempts the current plot size
		if ( 0 == m_lastPlotSizeAttempted.v ) {
			uint32_t RandomPlotVariance = RandomNumber( BuildingGen::MIN_PLOT_XY, BuildingGen::MAX_PLOT_XY );
			RandomPlotVariance = (RandomPlotVariance + m_CurPlotAvgDesiredPlotSize) >> 1;
			desiredSize = point2D_t( RandomPlotVariance, RandomPlotVariance );
		}
		else {
			desiredSize = p2D_subs( m_lastPlotSizeAttempted, 1 );
			// Clamp at minimum plot size to be safe
			desiredSize.pt.x = max(desiredSize.pt.x, BuildingGen::MIN_PLOT_XY);
			desiredSize.pt.y = max(desiredSize.pt.y, BuildingGen::MIN_PLOT_XY);
		}
	}
	
	m_bDescisionSuccessful = BuildingGen::GenPlot( m_CurPlotMin, m_CurPlotMax, m_CurBuildPoint, desiredSize );
	
	if (!m_bDescisionSuccessful) {
		m_lastPlotSizeAttempted = desiredSize.v;
		IncreaseFrustration(VAL_STEP_SMALL);
	}
	else { // reset
		m_lastPlotSizeAttempted = point2D_t(0).v; // reset important
		
		IncreaseDescisionStrength(VAL_STEP_LARGE);
	}
}
void cAIBot::MakeSubDescision_Defend()
{
	DecreaseFrustration(VAL_STEP_SMALL);			// placeholder
	m_bDescisionSuccessful = true;
}
void cAIBot::MakeSubDescision_Attack()
{
	DecreaseFrustration(VAL_STEP_SMALL);			// placeholder
	m_bDescisionSuccessful = true;
}

void cAIBot::Update(uint32_t const tNow) // Deciding what to do on the turn when it comes up
{
	bool bWasAttackingOrDefending(false), bCanChangeRootDescision(false);
	
	// CHECK CURRENT DESCISION
	switch(m_rootDescision) 
	{
		case DECIDE_BUILD:
			
				if (m_bDescisionSuccessful)
				{
					// Only change if frustration greater than descision strength
					if (m_Frustration > m_DescisionStrength) {
						bCanChangeRootDescision = true;
						IncreaseFrustration(VAL_STEP_SMALL);
					}
					else
						IncreaseDescisionStrength(VAL_STEP_LARGE);
				}
				else
					bCanChangeRootDescision = true;
			break;
		case DECIDE_DEFEND:
				if (m_Frustration < FUCKING_PISSED_OFF) {
					bWasAttackingOrDefending = true;
					bCanChangeRootDescision = true;
				}
			break;
		case DECIDE_ATTACK:
				if (m_Frustration < LOOSING_IT_TOTALLY) {
					bWasAttackingOrDefending = true;
					bCanChangeRootDescision = true;
				}
			break;
		//  CASE DECIDE_SKIPTURN cannot be changed for the upcoming turn
		default:
			// No descision has yet been made
			bCanChangeRootDescision = true;
		break;
	} // switch
	
	if ( bCanChangeRootDescision ) {
		// base changing descision on frustration, and the current strength of the already made descision
		if ( m_Frustration >= WTF_BLOW_SHIT_FU_MOTHERFUCKER )
		{
			DecreaseFrustration(VAL_STEP_SMALL);
		}
		else {
			uint32_t const RandFrustration( RandomNumber(m_Frustration, RAND_NORMAL_MAX) + m_Frustration );
			uint32_t const RandDescisionStrength( RandomNumber(m_DescisionStrength, RAND_NORMAL_MAX) + m_DescisionStrength );
			
			uint32_t const RandChangeDescision( RandomNumber(0, RAND_NORMAL_MAX) );
			
			// change root descision ?
			int32_t iDelta = (int32_t)RandChangeDescision - (int32_t)RandDescisionStrength;
			if ( iDelta > (RAND_NORMAL_MAX >> 1) ) {					// chance to change descision based on random number, minus strength
				if ( RandFrustration < RandDescisionStrength )  // only changes if frustration level large enough
					bCanChangeRootDescision = false;
			}
			else {
				bCanChangeRootDescision = false; // Strength is too high for descision to be changed
						 // Strength of descision increases on succesive attempts to change
				IncreaseDescisionStrength(VAL_STEP_MEDIUM);
				DecreaseFrustration(VAL_STEP_MEDIUM);
										 // Frustration lowers as a result of increased descision strength
			}
		}
	} // if bCanChangeRootDescision
	
	if ( bCanChangeRootDescision ) {  // Initial + successive descisions are initialized here
		
		uint32_t RandBuildWeightMax(RAND_NORMAL_MAX),
		         RandDefendWeightMax( (m_Frustration > FUCKING_PISSED_OFF && m_Frustration < MY_NAME_IS_SATAN) ? RAND_WEIGHT_MORE_MAX : RAND_NORMAL_MAX ),
		         RandAttackWeightMax( (m_Frustration > MY_NAME_IS_SATAN) ? RAND_WEIGHT_MORE_MAX : RAND_NORMAL_MAX );
		
		if ( bWasAttackingOrDefending ) {
			// Weight more to building
			RandBuildWeightMax = RAND_WEIGHT_MORE_MAX;
		}
		
		uint32_t const RandomBuilding = RandomNumber(0, RandBuildWeightMax),
									 RandomDefending = m_Frustration > GETTING_PISSED_OFF ? RandomNumber(0, RandDefendWeightMax) : 0,
									 RandomAttacking = m_Frustration > GETTING_PISSED_OFF ? RandomNumber(0, RandAttackWeightMax) : 0;
		
		// Find Largest Random Number
	//	uint32_t const RandMAX = max( RandomAttacking, max(RandomBuilding, RandomDefending) );
		uint32_t const RandMAX = RandomBuilding;
		
		m_lastRootDescision = m_rootDescision;
		m_lastSubDescision = m_subDescision;
		
		if ( RandMAX == RandomBuilding )
		{
			m_rootDescision = DECIDE_BUILD;
			if ( m_rootDescision == m_lastRootDescision ) {
				IncreaseDescisionStrength(VAL_STEP_LARGE);		// same descision made twice, reinforce strength and bonus frustration decrease
				DecreaseFrustration(VAL_STEP_MEDIUM);
			}
			else {
				// Reset Descision Strength to value representing some strength for a new descision
				m_DescisionStrength = VAL_STEP_LARGE;
				MakeSubDescision_Build();
			}
		}
		else if ( RandMAX == RandomDefending )
		{
			m_rootDescision = DECIDE_DEFEND;
			if ( m_rootDescision == m_lastRootDescision ) {
				IncreaseDescisionStrength(VAL_STEP_LARGE);		// same descision made twice, reinforce strength and bonus frustration decrease
				DecreaseFrustration(VAL_STEP_MEDIUM);
			}
			else {
				// Reset Descision Strength to value representing some strength for a new descision
				m_DescisionStrength = VAL_STEP_LARGE;
				MakeSubDescision_Defend();
			}
		}
		else if ( RandMAX == RandomAttacking )
		{
			m_rootDescision = DECIDE_ATTACK;
			if ( m_rootDescision == m_lastRootDescision ) {
				IncreaseDescisionStrength(VAL_STEP_LARGE);		// same descision made twice, reinforce strength and bonus frustration decrease
				DecreaseFrustration(VAL_STEP_MEDIUM);
			}
			else {
				// Reset Descision Strength to value representing some strength for a new descision
				m_DescisionStrength = VAL_STEP_LARGE;
				MakeSubDescision_Attack();
			}
		}
	} // if can change root descision
}

void cAIBot::Build(uint32_t const tNow)
{
	BuildingGen::GenPlotStructure(tNow, m_CurPlotMin, m_CurPlotMax);
	m_bTurnSuccessful = true;
}

void cAIBot::Defend(uint32_t const tNow)
{
	DecreaseFrustration(VAL_STEP_MEDIUM); // placeholder
	m_bTurnSuccessful = true;
}
void cAIBot::Attack(uint32_t const tNow)
{
	DecreaseFrustration(VAL_STEP_LARGE); // placeholder
	m_bTurnSuccessful = true;
}

void cAIBot::DoTurn(uint32_t const tNow) // Following thru on decisions
{
	if (!m_bDescisionSuccessful)
		Update(tNow);
	
	m_bTurnSuccessful = false;
	
	if ( m_bDescisionSuccessful )
	{
		switch(m_rootDescision) 
		{
			case DECIDE_BUILD:
				Build(tNow);
				break;
			case DECIDE_DEFEND:
				Defend(tNow);
				break;
			case DECIDE_ATTACK:
				Attack(tNow);
				break;
			default:
				break;
		}
	}
	
	UpdateTurnText();
	
	// Reset applicable variables for next decision making progress
	m_bDescisionSuccessful = false;
	m_CurPlotMin = point2D_t(0);
	m_CurPlotMax = point2D_t(0);
	m_DescisionStrength = 0;
	m_rootDescision = DECIDE_DEFAULT;
	m_subDescision = DECIDE_DEFAULT;
	
	if (!m_bTurnSuccessful) {
		IncreaseFrustration(VAL_STEP_MEDIUM);
	}
}

void cAIBot::UpdateTurnText()
{
	snprintf(m_szTurnText, MAX_LENGTH_TURN_TEXT, "%s, %s/%s, %d, %d\n", 
	  (m_rootDescision == DECIDE_BUILD ? "Build" : (m_rootDescision == DECIDE_DEFEND ? "Defend" : (m_rootDescision == DECIDE_ATTACK ? "Attack" : "Skip"))),
		(m_bDescisionSuccessful ? "OK" : "FAIL"),
		(m_bTurnSuccessful ? "OK" : "FAIL"),
	   m_Frustration, m_DescisionStrength );
}
void cAIBot::getTurnText( char* szBuffer ) const
{
	strncpy( szBuffer, m_szTurnText, MAX_LENGTH_TURN_TEXT );
}