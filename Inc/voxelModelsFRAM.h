/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef VOXELMODELSFRAM_H
#define VOXELMODELSFRAM_H
#include "voxelModel.h"

namespace Volumetric 
{
	namespace voxFRAM
	{
		extern Volumetric::voxB::voxelModel<Volumetric::voxB::DYNAMIC> voxelModelBlackBird;
		extern Volumetric::voxB::voxelModel<Volumetric::voxB::DYNAMIC> voxelModelF2A;
		extern Volumetric::voxB::voxelModel<Volumetric::voxB::DYNAMIC> voxelModelSidewinder;
	} // end namespace
	
	bool const LoadAllModels();
	Volumetric::voxB::voxelDescPacked const* const ProgramModelData(uint8_t*& FRAMWritePointer, 
																																	Volumetric::voxB::voxelModelDescHeader const& descModel,
																																	Volumetric::voxB::voxelDescPacked const* const pVoxels);
} // end namespace

#endif

