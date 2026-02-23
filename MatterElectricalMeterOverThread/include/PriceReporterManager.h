
#pragma once

#include "DataModelTypes.h"
#include "PlatformManager.h"

struct ChipStackLock{
	ChipStackLock(void){chip::DeviceLayer::PlatformMgr().LockChipStack();}
	~ChipStackLock(void){chip::DeviceLayer::PlatformMgr().UnlockChipStack();}
};

namespace PriceReporterManager{
    CHIP_ERROR Init(void);
    void Shutdown(void);
    void UpdatePriceInEuroCents(int price_cents);
}