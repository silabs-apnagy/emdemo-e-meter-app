#pragma once

/**********************************************************
 * Includes
 *********************************************************/

#include "AppEvent.h"
#include "BaseApplication.h"
#include "SimulatedMeter.h"
#include <lib/core/CHIPError.h>
#include <platform/CHIPDeviceLayer.h>
#include <stdbool.h>
#include <stdint.h>

/**********************************************************
 * Defines
 *********************************************************/

// Application-defined error codes in the CHIP_ERROR space.
#define APP_ERROR_EVENT_QUEUE_FAILED CHIP_APPLICATION_ERROR(0x01)
#define APP_ERROR_CREATE_TASK_FAILED CHIP_APPLICATION_ERROR(0x02)
#define APP_ERROR_UNHANDLED_EVENT CHIP_APPLICATION_ERROR(0x03)
#define APP_ERROR_CREATE_TIMER_FAILED CHIP_APPLICATION_ERROR(0x04)
#define APP_ERROR_START_TIMER_FAILED CHIP_APPLICATION_ERROR(0x05)
#define APP_ERROR_STOP_TIMER_FAILED CHIP_APPLICATION_ERROR(0x06)

/**********************************************************
 * AppTask Declaration
 *********************************************************/

class AppTask : public BaseApplication
{

public:
    AppTask() = default;

    static AppTask & GetAppTask() { return sAppTask; }

    static void AppTaskMain(void * pvParameter);

    CHIP_ERROR StartAppTask();

    static void ButtonEventHandler(uint8_t button, uint8_t btnAction);

    static void ProccessButtonEvent(AppEvent * event);

    static void SensorAttributeUpdateEvent(AppEvent * event);
    static void OccupancyAttributeUpdateEvent(AppEvent * event);

    // Current tariff price cached in the app (in euro cents).
    int32_t GetPriceEuroCents() const { return mPriceEuroCents; }
    static void ApplyMeterProfile(uint8_t profile);
    static void UpdateMeterMeasurements(intptr_t context);

#ifdef DISPLAY_ENABLED
    void UpdateDisplay() override;
#endif // DISPLAY_ENABLED

private:
    static AppTask sAppTask;

    // ---- Price preset handling (kept local to this app; no globals) ----
    enum class PricePreset : int32_t
    {
        Low  = 113,
        Mid  = 399,
        High = 765,
    };

    static PricePreset NextPreset(PricePreset current);
    void ApplyPricePreset(PricePreset preset);
    PricePreset AdvancePricePreset();
    void RequestDemoScreenRefresh();

    PricePreset mPricePreset = PricePreset::Low;
    int32_t mPriceEuroCents  = static_cast<int32_t>(PricePreset::Low);

    CHIP_ERROR AppInit() override;

    static void ButtonHandler(AppEvent * aEvent);
};
