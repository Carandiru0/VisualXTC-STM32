/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License 
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License. 
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef AIBOT_H
#define AIBOT_H

#include "globals.h"
#include "point2D.h"

class cAIBot
{
public:
	static constexpr uint32_t const MAX_LENGTH_TURN_TEXT = 128;
public:
	point2D_t const& getStartLocation() const { return(m_Start); }
	point2D_t const& getCurrentBuildLocation() const { return(m_CurBuildPoint); }
	void getTurnText( char* szBuffer ) const;
	
public:
	void Update(uint32_t const tNow);		// Deciding what to do on the turn when it comes up
	void DoTurn(uint32_t const tNow);		// Following thru on decisions

	cAIBot( point2D_t const StartLocation );

private:
	void UpdateTurnText();
	void MoveBuildPoint();

	uint32_t const& IncreaseDescisionStrength( uint32_t const Amount );
	uint32_t const& IncreaseFrustration( uint32_t const Amount );
	uint32_t const& DecreaseFrustration( uint32_t const Amount );

	void MakeSubDescision_Build();
	void MakeSubDescision_Defend();
	void MakeSubDescision_Attack();

	void Build(uint32_t const tNow);
	void Defend(uint32_t const tNow);
	void Attack(uint32_t const tNow);
private:
	point2D_t const 	m_Start;
	point2D_t					m_CurPlotMin,
										m_CurPlotMax,
										m_CurBuildPoint,
										m_lastPlotSizeAttempted;
	
	uint32_t					m_nextMajorNeighbour, m_BuildLayers;
	uint32_t					m_CurPlotAvgDesiredPlotSize;
	uint32_t 					m_rootDescision,
										m_subDescision;
	uint32_t					m_lastRootDescision,
										m_lastSubDescision;
	uint32_t					m_DescisionStrength;
	uint32_t 					m_Frustration;
	bool							m_bDescisionSuccessful,
										m_bTurnSuccessful;
	
	char							m_szTurnText[MAX_LENGTH_TURN_TEXT];
public:
	cAIBot(cAIBot&& rhs) = default;
private:
	cAIBot() = delete;
	void operator=(cAIBot&&) = delete;
	//void operator=(cAIBot const&) = delete;
	cAIBot(cAIBot const&) = delete;
};

#endif


