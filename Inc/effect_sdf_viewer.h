/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef EFFECT_SDF_VIEWER
#define EFFECT_SDF_VIEWER
#include "PreprocessorCore.h"

namespace SDF_Viewer
{
	
uint8_t const getShadeByIndex( uint32_t const iDx );
	
void Initialize();
	
void Update(uint32_t const tNow);
void Render(uint32_t const tNow);
	
}

#endif

