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
#include "voxelModelsFRAM.h"
#include "FLASH\imports.h"
#include "VoxBinary.h"
#include "quadspi.h"

#include "debug.cpp"

typedef struct __attribute__((packed)) voxelModelsHeader
{
	uint8_t numModels;
	
} voxelModelsHeader;
	
namespace Volumetric 
{
	namespace voxFRAM
	{
		//todo: load palette, modify stored index to palette value in grayscale
		// see if loaded model, once in sram can be then programmed to FRAM using the same runtime structure, then all models are "read" from sram in the state for rendering...
		// and freeing up SRAM and allowing many voxel models to be used! 
		// FLASH .VOX -> LOAD MODEL (SRAM) -> PROGRAM FRAM with copy of SRAM -> DELETE MODEL FROM SRAM -> USE FRAM MODEL FOR RENDERING
		Volumetric::voxB::voxelModel<Volumetric::voxB::DYNAMIC> voxelModelBlackBird(1.0f);
		Volumetric::voxB::voxelModel<Volumetric::voxB::DYNAMIC> voxelModelF2A(1.0f);
		Volumetric::voxB::voxelModel<Volumetric::voxB::DYNAMIC> voxelModelSidewinder(1.0f);
		
		static Volumetric::voxB::voxelModel<Volumetric::voxB::DYNAMIC>* const voxelModels[] =  { &voxelModelBlackBird, 
																																														 &voxelModelF2A,
																																														 &voxelModelSidewinder
																																													 };
		static constexpr uint32_t NUM_MODELS = countof(voxelModels);
	} // end namespace
	
	/*
	static bool const VerifyData( Volumetric::voxB::voxelModel<Volumetric::voxB::DYNAMIC> const& Model )
	{
		uint32_t uiNumVoxels = Model.numVoxels;
		
		Volumetric::voxB::voxelDescPacked const* pFRAMVoxel;
		Volumetric::voxB::voxelDescPacked const* pSRAMVoxel;
		
		pFRAMVoxel = Model.VoxelsFRAM;
		pSRAMVoxel = Model.Voxels.data();
		
		while ( uiNumVoxels-- ) {
			
			if ( *pSRAMVoxel++ != *pFRAMVoxel++ )
				return(false);
			
		}
		return(true);
	}
	*/
	
	Volumetric::voxB::voxelDescPacked const* const ProgramModelData(uint8_t*& FRAMWritePointer, 
																																	Volumetric::voxB::voxelModelDescHeader const& descModel,
																																	Volumetric::voxB::voxelDescPacked const* const pVoxels)
	{
		Volumetric::voxB::voxelDescPacked const* pVoxelsInFRAM(nullptr);
		
		// wait for completion of previous model data programming
		//while( !QuadSPI_FRAM::IsWriteMemoryComplete() ) { __WFI(); }
		
		// write model header //
		if ( QuadSPI_FRAM::QSPI_OP_OK == QuadSPI_FRAM::WriteMemory((uint8_t* const)&descModel, 
																														 ((uint32_t)FRAMWritePointer) - ((uint32_t)QuadSPI_FRAM::QSPI_Address), 
																														 sizeof(Volumetric::voxB::voxelModelDescHeader)) ) {
			// wait for completion is neccessary passed in data residing on stack
			while( !QuadSPI_FRAM::IsWriteMemoryComplete() ) { __WFI(); }
			
			FRAMWritePointer += sizeof(Volumetric::voxB::voxelModelDescHeader);
			
			// write model voxels //
			uint32_t const uiVoxelDataSize = sizeof(voxB::voxelDescPacked) * descModel.numVoxels;
			if ( QuadSPI_FRAM::QSPI_OP_OK == QuadSPI_FRAM::WriteMemory((uint8_t* const)pVoxels, 
																														 ((uint32_t)FRAMWritePointer) - ((uint32_t)QuadSPI_FRAM::QSPI_Address), 
																														 uiVoxelDataSize) ) {
																															 
				// wait for completion is neccessary passed in data residing on heap, however dtor will be called
			  while( !QuadSPI_FRAM::IsWriteMemoryComplete() ) { __WFI(); }	

				pVoxelsInFRAM = (voxB::voxelDescPacked const* const)FRAMWritePointer;
				FRAMWritePointer += uiVoxelDataSize;
			}
			else
				DebugMessage("Write model Data for Voxel Load FAIL");
		}
		else
			DebugMessage("Write model Header for Voxel Load FAIL");
	
		return(pVoxelsInFRAM);
	}
} // end namespace

// ###### private functions:
static bool const WriteHeader(uint8_t*& __restrict FRAMWritePointer)
{
	voxelModelsHeader header;
	header.numModels = Volumetric::voxFRAM::NUM_MODELS;
	
	if ( QuadSPI_FRAM::QSPI_OP_OK == QuadSPI_FRAM::WriteMemory((uint8_t* const)&header, 
																														 ((uint32_t)FRAMWritePointer) - ((uint32_t)QuadSPI_FRAM::QSPI_Address), 
																														 sizeof(voxelModelsHeader)) ) {
		// wait for completion is neccessary passed in data residing on stack
		while( !QuadSPI_FRAM::IsWriteMemoryComplete() ) { __WFI(); }
		
		FRAMWritePointer += sizeof(voxelModelsHeader);
		
		return(true);
	}
	else
			DebugMessage("WriteHeader for Voxel Load FAIL");
	
	return(false);
}

namespace Volumetric 
{
	bool const LoadAllModels()
	{
		uint8_t const*  FRAMReadPointer;
		uint8_t* 				FRAMWritePointer;
		
		bool bFRAMReProgramming(false);
		
		// Ensure MemoryMapped Mode is on
		if ( QuadSPI_FRAM::QSPI_OP_OK != QuadSPI_FRAM::MemoryMappedMode() ) {
			DebugMessage("Activate MMAP Mode for Voxel Load FAIL");
			return(false);
		}
		
		// Get first byte at beginning of FRAM address space, which contains the number of models
		FRAMReadPointer = QuadSPI_FRAM::QSPI_Address;
		
		voxelModelsHeader mainHeader;
		memcpy(&mainHeader, FRAMReadPointer, sizeof(voxelModelsHeader));
		uint32_t const numModelsInFRAM = mainHeader.numModels;
		
		// ReProgramming is required if number of models being loaded is different than the number
		// of models in FRAM. This automatically updates the FRAM based on that ONLY.
		// If the order changes, or same number of models but different model data that is unaccounted for
		// must be forced to reprogram under those circumstances
#if defined(VOX_FRAM_FORCE_REPROGRAMMING)
		bool const bForce = true;
#else
		bool const bForce = false;
#endif
		
		bFRAMReProgramming = ((numModelsInFRAM != Volumetric::voxFRAM::NUM_MODELS) | (0 == numModelsInFRAM) | bForce);
		
		if ( bFRAMReProgramming ) { // Rewrite header
			DebugMessage("FRAM Reprogramming...");
			
			// Enable Writes
			if ( QuadSPI_FRAM::QSPI_OP_OK != QuadSPI_FRAM::WriteEnable() ) {
				DebugMessage("WriteEnable for Voxel Load FAIL");
				return(false);
			}
			FRAMWritePointer = (uint8_t*)QuadSPI_FRAM::QSPI_Address;
			
			if ( WriteHeader(FRAMWritePointer) )
			{
				// at this point FRAMWritePointer is at the correct position for Model #1 's data to be written
			
				// Begin load of all models into SRAM one by one, then program relevant data to FRAM
				// the WritePointer is passed in by reference and passes thru to programming function once model is loaded
				if (!Volumetric::voxB::Load( &voxFRAM::voxelModelBlackBird, _vox_sr71, FRAMWritePointer))
					return(false);
				
				if (!Volumetric::voxB::Load( &voxFRAM::voxelModelF2A, _vox_f2a, FRAMWritePointer))
					return(false);
				
				if (!Volumetric::voxB::Load( &voxFRAM::voxelModelSidewinder, _vox_sidewinder, FRAMWritePointer))
					return(false);
				
				// Disable Writes
				if ( QuadSPI_FRAM::QSPI_OP_OK != QuadSPI_FRAM::WriteDisable() ) {
					DebugMessage("WriteDisable for Voxel Load FAIL");
					return(false);
				}
				
				// back to memory mapped mode!
				if ( QuadSPI_FRAM::QSPI_OP_OK != QuadSPI_FRAM::MemoryMappedMode() ) {
					DebugMessage("BackTo MMAP Mode for Voxel Load FAIL");
					return(false);
				}
				/*
				if ( VerifyData(voxFRAM::voxelModelBlackBird) ) {
					DebugMessage("FRAM Model Good");
				}
				else 
					DebugMessage("FRAM Model BAD");
				*/
			}
		}
		else { // Read header, getting address location for each model
			DebugMessage("FRAM Loading...");
			
			FRAMReadPointer += sizeof(voxelModelsHeader); // Advance from main header
			
			for (int32_t iDx = 0 ; iDx < Volumetric::voxFRAM::NUM_MODELS ; ++iDx)
			{
				// read model header
				Volumetric::voxB::voxelModelDescHeader descModel;
				memcpy(&descModel, FRAMReadPointer, sizeof(Volumetric::voxB::voxelModelDescHeader));
				
				// advance
				FRAMReadPointer += sizeof(Volumetric::voxB::voxelModelDescHeader);
				
				// read word
				uint32_t const modelVoxelsAddress = (uint32_t)FRAMReadPointer;
				
				// Set header data and address for the voxels of current model
				voxFRAM::voxelModels[iDx]->numVoxels = descModel.numVoxels;
				voxFRAM::voxelModels[iDx]->maxDimensions = vec3_t( descModel.dimensionX, descModel.dimensionY, descModel.dimensionZ );
				voxFRAM::voxelModels[iDx]->maxDimensionsInv = v3_inverse(voxFRAM::voxelModels[iDx]->maxDimensions);
				
				voxFRAM::voxelModels[iDx]->VoxelsFRAM = (voxB::voxelDescPacked* const __restrict)modelVoxelsAddress;	
				
				//advance to next model header
				FRAMReadPointer += (descModel.numVoxels * sizeof(voxB::voxelDescPacked));
			}
		}
		
		
		return(true);
	}
} // end namespace



