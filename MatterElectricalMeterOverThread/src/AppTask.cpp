#include "AppTask.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include "LEDWidget.h"
#include <app/server/Server.h>
#include <app/util/attribute-storage.h>
#include <assert.h>
#include <cmsis_os2.h>
#include <lib/support/CodeUtils.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/silabs/platformAbstraction/SilabsPlatform.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>
#include <sl_cmsis_os2_common.h>

#include "PriceReporterManager.h"



#ifdef SL_CATALOG_SIMPLE_LED_LED1_PRESENT
#define LIGHT_LED 1
#else
#define LIGHT_LED 0
#endif

#define APP_FUNCTION_BUTTON 0
#define APP_SWITCH_BUTTON 1
 
using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace ::chip::DeviceLayer;
using namespace ::chip::DeviceLayer::Silabs;

namespace {
LEDWidget sPriceLED; // Use LEDWidget for basic LED functionality
} // namespace

using namespace chip::TLV;
using namespace ::chip::DeviceLayer;

AppTask AppTask::sAppTask;
CHIP_ERROR AppTask::AppInit()
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    chip::DeviceLayer::Silabs::GetPlatform().SetButtonsCb(AppTask::ButtonEventHandler);

    err = PriceReporterManager::Init();
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "PriceReporterManager::Init failed");
        appError(err);
    }


    sPriceLED.Init(LIGHT_LED);
    sPriceLED.Set(false);
    //sPriceLED.Set(LightMgr().IsLightOn());

// Update the LCD with the Stored value. Show QR Code if not provisioned
#ifdef DISPLAY_ENABLED
    //GetLCD().WriteDemoUI(LightMgr().IsLightOn());
#ifdef QR_CODE_ENABLED
#ifdef SL_WIFI
    if (!ConnectivityMgr().IsWiFiStationProvisioned())
#else
    if (!ConnectivityMgr().IsThreadProvisioned())
#endif /* !SL_WIFI */
    {
        GetLCD().ShowQRCode(true);
    }
#endif // QR_CODE_ENABLED
#endif

    BaseApplication::InitCompleteCallback(err);
    return err;
}

CHIP_ERROR AppTask::StartAppTask()
{
    return BaseApplication::StartAppTask(AppTaskMain);
}

void AppTask::AppTaskMain(void * pvParameter)
{
    AppEvent event;
    osMessageQueueId_t sAppEventQueue = *(static_cast<osMessageQueueId_t *>(pvParameter));

    CHIP_ERROR err = sAppTask.Init();
    if (err != CHIP_NO_ERROR)
    {
        SILABS_LOG("AppTask.Init() failed");
        appError(err);
    }

#if !(defined(CHIP_CONFIG_ENABLE_ICD_SERVER) && CHIP_CONFIG_ENABLE_ICD_SERVER)
    sAppTask.StartStatusLEDTimer();
#endif

    SILABS_LOG("App Task started");

    while (true)
    {
        osStatus_t eventReceived = osMessageQueueGet(sAppEventQueue, &event, nullptr, osWaitForever);
        while (eventReceived == osOK)
        {
            sAppTask.DispatchEvent(&event);
            eventReceived = osMessageQueueGet(sAppEventQueue, &event, nullptr, 0);
        }
    }
}

#ifdef DISPLAY_ENABLED
void AppTask::UpdateDisplay()
{

}
#endif // DISPLAY_ENABLED

void AppTask::ButtonEventHandler(uint8_t button, uint8_t btnAction)
{
    AppEvent button_event           = {};
    button_event.Type               = AppEvent::kEventType_Button;
    button_event.ButtonEvent.Action = btnAction;

    if (button == APP_SWITCH_BUTTON && btnAction == static_cast<uint8_t>(SilabsPlatform::ButtonAction::ButtonPressed))
    {
        button_event.Handler = ProccessButtonEvent;
        AppTask::GetAppTask().PostEvent(&button_event);
    }
    else if (button == APP_FUNCTION_BUTTON)
    {
        button_event.Handler = BaseApplication::ButtonHandler;
        AppTask::GetAppTask().PostEvent(&button_event);
    }
}

enum PricesEurocent {
	LOW = 113,
    MID = 399,
	HIGH = 765
};

static int price_eurocent = PricesEurocent::LOW;


void AppTask::ProccessButtonEvent(AppEvent * event)
{
    VerifyOrReturn(event != nullptr);
    VerifyOrReturn(event->Type == AppEvent::kEventType_Button);

    switch(price_eurocent){
	case PricesEurocent::LOW:
		price_eurocent = PricesEurocent::MID;
		sPriceLED.Blink(500,500);
		break;
    case PricesEurocent::MID:
        price_eurocent = PricesEurocent::HIGH;
        sPriceLED.Blink(true);
        break;
	case PricesEurocent::HIGH:
	default:
		price_eurocent = PricesEurocent::LOW;
		sPriceLED.Set(false);
		break;
	}

    ChipStackLock chip_stack_lock;
    PriceReporterManager::UpdatePriceInEuroCents(price_eurocent);
}
