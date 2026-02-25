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

#ifdef DISPLAY_ENABLED
#include "demo-ui.h"
#include "glib.h"
#endif

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

#ifdef DISPLAY_ENABLED
#define LCD_CTX() \
    static_cast<GLIB_Context_t *>(AppTask::GetLCD().Context())
namespace {
// Custom UI callback used by SilabsLCD::WriteDemoUI().
void DrawCustomDemoUI(GLIB_Context_t * context)
{
    uint8_t lineNb = 0;
    char str[20];


    GLIB_clear(LCD_CTX());
    sprintf(str, "# E-Meter Status");
    GLIB_drawStringOnLine(LCD_CTX(), str, lineNb++, GLIB_ALIGN_LEFT, 0, 0, true);
    sprintf(str, "Price:%ld", static_cast<long>(AppTask::GetAppTask().GetPriceEuroCents()));
    GLIB_drawStringOnLine(LCD_CTX(), str, lineNb++, GLIB_ALIGN_LEFT, 0, 0, true);

    updateDisplay();
}
} // namespace
#endif // DISPLAY_ENABLED

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

    // Publish initial price at boot.
    ApplyPricePreset(mPricePreset);

#ifdef DISPLAY_ENABLED
    // Install a custom UI renderer for the Demo screen.
    GetLCD().SetCustomUI(DrawCustomDemoUI);

    // Force an initial paint of the Demo screen content once the screen is shown.
    RequestDemoScreenRefresh();

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
#endif // DISPLAY_ENABLED

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
    UpdateLCDStatusScreen();
    GetLCD().CycleScreens();

   
    SilabsLCD::Screen_e screen;
    GetLCD().GetScreen(screen);
    if (screen == SilabsLCD::Screen_e::DemoScreen)
    {
        GetLCD().WriteDemoUI(); 
    }
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

// ---- Price preset helpers ----

AppTask::PricePreset AppTask::NextPreset(PricePreset current)
{
    switch (current)
    {
    case PricePreset::Low:
        return PricePreset::Mid;
    case PricePreset::Mid:
        return PricePreset::High;
    case PricePreset::High:
    default:
        return PricePreset::Low;
    }
}

AppTask::PricePreset AppTask::AdvancePricePreset()
{
    mPricePreset = NextPreset(mPricePreset);
    return mPricePreset;
}

void AppTask::ApplyPricePreset(PricePreset preset)
{
    mPricePreset    = preset;
    mPriceEuroCents = static_cast<int32_t>(preset);

    switch (preset)
    {
    case PricePreset::Low:
        sPriceLED.Set(false);       
        break;
    case PricePreset::Mid:
        sPriceLED.Set(false);
        break;
    case PricePreset::High:
    default:
        sPriceLED.Blink(true);
        break;
    }

    ChipStackLock chip_stack_lock;
    PriceReporterManager::UpdatePriceInEuroCents(mPriceEuroCents);

    RequestDemoScreenRefresh();
}

void AppTask::RequestDemoScreenRefresh()
{
#ifdef DISPLAY_ENABLED
    SilabsLCD::Screen_e screen;
    GetLCD().GetScreen(screen);
    if (screen == SilabsLCD::Screen_e::DemoScreen)
    {
        BaseApplication::PostUpdateDisplayEvent(SilabsLCD::Screen_e::DemoScreen);
    }
#endif
}

void AppTask::ProccessButtonEvent(AppEvent * event)
{
    VerifyOrReturn(event != nullptr);
    VerifyOrReturn(event->Type == AppEvent::kEventType_Button);

    AppTask::GetAppTask().ApplyPricePreset(AppTask::GetAppTask().AdvancePricePreset());
}