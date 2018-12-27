#include "BackupSRAM.h"
#include "Globals.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_rcc.h"
#include "stm32f7xx_ll_pwr.h"

static void EnableAccess()
{
	// PWR Clock already enabled //
	//
	LL_PWR_EnableBkUpAccess();
	
	// Enable backup SRAM Clock
	if (!LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_BKPSRAM))
		LL_APB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_BKPSRAM);
	
	// Enable the Backup SRAM low power Regulator to retain it's content in VBAT mode
	if (!LL_PWR_IsEnabledBkUpRegulator())
		LL_PWR_EnableBkUpRegulator();
}
static void DisableAccess()
{
	LL_PWR_DisableBkUpAccess();
}

int32_t const getBackUpValue(int32_t const iD)
{
	int32_t iReturnedValue(-1);
	
	//EnableAccess();
	/* backup sram not available on nucleo devices, need to use FRAM!!!
	__IO int32_t const* const numValues =  (__IO int32_t *) (BKPSRAM_BASE + 0);
	
	if ( 0 != (*numValues) && (*numValues) >= iD )
	{
		iReturnedValue = *((__IO uint32_t *) (BKPSRAM_BASE + iD + 2));
	}
	(/
	//DisableAccess();
	*/
	return(iReturnedValue);
}
int32_t const BackupValue(int32_t const Value, int32_t const oldPosition )
{
	int32_t iDPosition(-1);
	
	//EnableAccess();
	/* backup sram not available on nucleo devices, need to use FRAM!!!
	__IO int32_t* const numValues =  (__IO int32_t *) (BKPSRAM_BASE + 0);
	
	if ( -1 != oldPosition && (*numValues) >= oldPosition )
		iDPosition = oldPosition;
	else
		iDPosition = ++(*numValues);
	
	*((__IO uint32_t *) (BKPSRAM_BASE + iDPosition + 2)) = Value;
	*/
	//DisableAccess();
	
	return(iDPosition);
}