
#include "PriceReporterManager.h"
#include "commodity-price-server.h"

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::CommodityPrice;
using namespace chip::app::Clusters::CommodityPrice::Attributes;
using namespace chip::app::Clusters::CommodityPrice::Structs;

namespace{
    chip::EndpointId price_reporter_endpoint = 1;
    
    std::unique_ptr<chip::app::Clusters::CommodityPrice::Instance> price_cluster_instance;
    std::unique_ptr<chip::app::Clusters::CommodityPrice::Delegate> price_cluster_delegate;
}

namespace PriceReporterManager{
    CHIP_ERROR Init(void){
        if (price_cluster_instance || price_cluster_delegate) {
            ChipLogError(AppServer, "Commodity Price Instance or Delegate already exist.");
            return CHIP_ERROR_INCORRECT_STATE;
        }

        price_cluster_delegate = std::make_unique<CommodityPrice::Delegate>();
        if (!price_cluster_delegate) {
            ChipLogError(AppServer, "Failed to allocate memory for Commodity Price Delegate");
            return CHIP_ERROR_NO_MEMORY;
        }

        price_cluster_instance = std::make_unique<CommodityPrice::Instance>(
                price_reporter_endpoint,
                *price_cluster_delegate,
                BitMask<CommodityPrice::Feature, uint32_t>());

        CHIP_ERROR err = price_cluster_instance->Init();
        if (err != CHIP_NO_ERROR){
            ChipLogError(AppServer, "Init failed on price_cluster_instance");
            price_cluster_instance.reset();
            price_cluster_delegate.reset();
            return err;
        }

        ChipStackLock chip_stack_lock;
        Globals::Structs::CurrencyStruct::Type currency {kCurrencyEURO, 2};
        err = price_cluster_instance->SetCurrency(currency);
        if (err != CHIP_NO_ERROR){
            ChipLogError(AppServer, "Setting currency to EUR failed");
            price_cluster_instance.reset();
            price_cluster_delegate.reset();
            return err;
        }

        return CHIP_NO_ERROR;
    }

    void Shutdown(void){
        if (price_cluster_instance){
            price_cluster_instance->Shutdown();
            price_cluster_instance.reset();
        }
        if (price_cluster_delegate){
            price_cluster_delegate.reset();
        }
    }

    void UpdatePriceInEuroCents(int price_cents){
        CommodityPriceStruct::Type price_struct;
        price_struct.price = chip::Optional<long long int>(price_cents);
        price_cluster_instance->SetCurrentPrice(price_struct);
    }

}
