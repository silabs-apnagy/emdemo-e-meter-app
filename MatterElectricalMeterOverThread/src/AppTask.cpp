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
#include "SimulatedMeter.h"

#include <app/clusters/electrical-power-measurement-server/CodegenIntegration.h>
#include <app/clusters/electrical-energy-measurement-server/CodegenIntegration.h>
#include <app/clusters/mode-select-server/supported-modes-manager.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app/data-model/List.h>
#include <app/reporting/reporting.h>
#include <app/server/Server.h>
#include <app/util/attribute-storage.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>
#include <memory>
#include <chrono>
#include <cstring>
#include "FreeRTOS.h"
#include "timers.h"

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

constexpr EndpointId kMeterEndpoint = 1;
constexpr uint32_t kMeterUpdatePeriodMs = 5000;
SimulatedMeter sMeter;
TimerHandle_t sMeterTimer = nullptr;

using MeterModeOption = ModeSelect::Structs::ModeOptionStruct::Type;
using MeterSemanticTag = ModeSelect::Structs::SemanticTagStruct::Type;
const MeterModeOption kMeterModes[] = {
    { .label = "Night"_span, .mode = 0, .semanticTags = DataModel::List<const MeterSemanticTag>{} },
    { .label = "Day"_span, .mode = 1, .semanticTags = DataModel::List<const MeterSemanticTag>{} },
    { .label = "Peak"_span, .mode = 2, .semanticTags = DataModel::List<const MeterSemanticTag>{} },
};

class MeterModesManager final : public ModeSelect::SupportedModesManager
{
public:
    ModeOptionsProvider getModeOptionsProvider(EndpointId endpoint) const override
    {
        return endpoint == kMeterEndpoint ? ModeOptionsProvider{ kMeterModes, kMeterModes + 3 } : ModeOptionsProvider{};
    }
    chip::Protocols::InteractionModel::Status getModeOptionByMode(EndpointId endpoint, uint8_t mode,
                                                                   const MeterModeOption ** option) const override
    {
        using Status = chip::Protocols::InteractionModel::Status;
        if (option == nullptr) return Status::Failure;
        *option = nullptr;
        if (endpoint != kMeterEndpoint) return Status::UnsupportedCluster;
        for (const auto & candidate : kMeterModes)
        {
            if (candidate.mode == mode) { *option = &candidate; return Status::Success; }
        }
        return Status::InvalidCommand;
    }
};
MeterModesManager sMeterModesManager;

class MeterPowerDelegate final : public ElectricalPowerMeasurement::Delegate
{
public:
    ElectricalPowerMeasurement::PowerModeEnum GetPowerMode() override { return ElectricalPowerMeasurement::PowerModeEnum::kAc; }
    uint8_t GetNumberOfMeasurementTypes() override { return 0; }
    CHIP_ERROR StartAccuracyRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetAccuracyByIndex(uint8_t, ElectricalPowerMeasurement::Structs::MeasurementAccuracyStruct::Type &) override { return CHIP_ERROR_NOT_FOUND; }
    CHIP_ERROR EndAccuracyRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR StartRangesRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetRangeByIndex(uint8_t, ElectricalPowerMeasurement::Structs::MeasurementRangeStruct::Type &) override { return CHIP_ERROR_NOT_FOUND; }
    CHIP_ERROR EndRangesRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR StartHarmonicCurrentsRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetHarmonicCurrentsByIndex(uint8_t, ElectricalPowerMeasurement::Structs::HarmonicMeasurementStruct::Type &) override { return CHIP_ERROR_NOT_FOUND; }
    CHIP_ERROR EndHarmonicCurrentsRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR StartHarmonicPhasesRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetHarmonicPhasesByIndex(uint8_t, ElectricalPowerMeasurement::Structs::HarmonicMeasurementStruct::Type &) override { return CHIP_ERROR_NOT_FOUND; }
    CHIP_ERROR EndHarmonicPhasesRead() override { return CHIP_NO_ERROR; }
    DataModel::Nullable<int64_t> GetVoltage() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetActiveCurrent() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetReactiveCurrent() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetApparentCurrent() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetActivePower() override { return DataModel::MakeNullable(static_cast<int64_t>(mPowerMilliW)); }
    DataModel::Nullable<int64_t> GetReactivePower() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetApparentPower() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetRMSVoltage() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetRMSCurrent() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetRMSPower() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetFrequency() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetPowerFactor() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetNeutralCurrent() override { return DataModel::NullNullable; }
    void SetPower(uint32_t watts) { mPowerMilliW = static_cast<int64_t>(watts) * 1000; }
private:
    int64_t mPowerMilliW = 0;
};
MeterPowerDelegate sPowerDelegate;
std::unique_ptr<ElectricalPowerMeasurement::Instance> sPowerInstance;
std::unique_ptr<ElectricalEnergyMeasurement::ElectricalEnergyMeasurementAttrAccess> sEnergyAccess;

void MeterTimerCallback(TimerHandle_t)
{
    const CHIP_ERROR err = DeviceLayer::PlatformMgr().ScheduleWork(AppTask::UpdateMeterMeasurements, 0);
    if (err != CHIP_NO_ERROR) ChipLogError(AppServer, "Could not schedule meter measurement update: %s", ErrorStr(err));
}

CHIP_ERROR InitializeMeterMatter()
{
    sPowerInstance = std::make_unique<ElectricalPowerMeasurement::Instance>(
        kMeterEndpoint, sPowerDelegate,
        BitMask<ElectricalPowerMeasurement::Feature, uint32_t>(ElectricalPowerMeasurement::Feature::kAlternatingCurrent),
        BitMask<ElectricalPowerMeasurement::OptionalAttributes, uint32_t>{});
    ReturnErrorOnFailure(sPowerInstance->Init());

    sEnergyAccess = std::make_unique<ElectricalEnergyMeasurement::ElectricalEnergyMeasurementAttrAccess>(
        BitMask<ElectricalEnergyMeasurement::Feature, uint32_t>(ElectricalEnergyMeasurement::Feature::kImportedEnergy,
            ElectricalEnergyMeasurement::Feature::kCumulativeEnergy, ElectricalEnergyMeasurement::Feature::kPeriodicEnergy),
        BitMask<ElectricalEnergyMeasurement::OptionalAttributes, uint32_t>{}, kMeterEndpoint);
    ReturnErrorOnFailure(sEnergyAccess->Init());

    ModeSelect::setSupportedModesManager(&sMeterModesManager);
    ModeSelect::Attributes::CurrentMode::Set(kMeterEndpoint, static_cast<uint8_t>(sMeter.GetProfile()));
    sPowerDelegate.SetPower(sMeter.Tick(0).activePowerW);
    return CHIP_NO_ERROR;
}
} // namespace

using namespace chip::TLV;
using namespace ::chip::DeviceLayer;

AppTask AppTask::sAppTask;

#ifdef DISPLAY_ENABLED
#define LCD_CTX() \
    static_cast<GLIB_Context_t *>(AppTask::GetLCD().Context())

#define APP_BITMAP_WIDTH 64
#define APP_BITMAP_HEIGHT 64

#define APP_X_POSITION ((LCD_CTX()->pDisplayGeometry->xSize - APP_BITMAP_WIDTH) / 2)
#define APP_Y_POSITION (LCD_CTX()->pDisplayGeometry->ySize - APP_BITMAP_HEIGHT - 5)

#define PROT2_X_POSITION 104
#define PROT2_Y_POSITION (APP_Y_POSITION + (APP_Y_POSITION / 2))

#define THREAD_BITMAP_WIDTH 16
#define THREAD_BITMAP_HEIGHT 18
#define PROT1_BITMAP_WIDTH THREAD_BITMAP_WIDTH
#define PROT1_BITMAP_HEIGHT THREAD_BITMAP_HEIGHT
#define PROT1_X_POSITION 8
#define PROT1_Y_POSITION (APP_Y_POSITION + (APP_Y_POSITION / 2))

#define MATTER_LOGO_WIDTH 18
#define MATTER_LOGO_HEIGHT 17

#define SILICONLABS_BITMAP_WIDTH 128
#define SILICONLABS_BITMAP_HEIGHT 45

#define SILABS_BITMAP                                                                                                              \
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff,    \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    \
        0xfd, 0xff, 0xff, 0x01, 0x00, 0x00, 0xc0, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0x3f, 0x00,    \
        0x00, 0x00, 0x00, 0xff, 0xff, 0x1f, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0xfe,    \
        0xff, 0x1f, 0xf8, 0xff, 0xff, 0xff, 0x7f, 0xfc, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x1f, 0xf0, 0xff,    \
        0xff, 0xff, 0x1f, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x1f, 0xe0, 0xff, 0xff, 0xff, 0xc3, 0xff,    \
        0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x0f, 0xc0, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0x07, 0x00, 0x00,    \
        0x00, 0x00, 0x00, 0xff, 0xff, 0x07, 0xc0, 0xff, 0xff, 0x1f, 0xf8, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff,    \
        0xff, 0x01, 0xc0, 0xff, 0xff, 0x07, 0xfc, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x7f, 0x00, 0x80, 0xff,    \
        0xff, 0x01, 0xfe, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x0f, 0x00, 0x80, 0xff, 0x7f, 0x00, 0xff, 0xff,    \
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x3f, 0x80, 0xff, 0xff, 0xff, 0x00, 0x00, 0xf0,    \
        0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f, 0x80, 0xff, 0xff, 0xff, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xff, 0x01,    \
        0x00, 0x00, 0xc0, 0xff, 0x03, 0xc0, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x00, 0xe0, 0xff,    \
        0x01, 0xc0, 0xff, 0xff, 0xff, 0xf1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xe0, 0xff, 0x00, 0xc0, 0xff, 0xff,    \
        0xff, 0xc3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0xf0, 0x7f, 0x00, 0x80, 0xff, 0xff, 0xff, 0x0f, 0xfc, 0xff,    \
        0xff, 0xff, 0xff, 0xff, 0x07, 0x00, 0xf8, 0x3f, 0x00, 0x80, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff,    \
        0x0f, 0x00, 0xfc, 0x1f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0xc0, 0xff, 0xff, 0xff, 0x0f, 0x00, 0xfe, 0x0f,    \
        0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xff, 0x1f, 0x00, 0xff, 0x07, 0x00, 0x00, 0xf0, 0xff,    \
        0xff, 0xff, 0xff, 0xff, 0x3f, 0xfc, 0xff, 0xff, 0x1f, 0x80, 0xff, 0x07, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff,    \
        0x3f, 0xf8, 0xff, 0xff, 0x1f, 0xc0, 0xff, 0x03, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xff, 0x00, 0xf0, 0xff, 0xff,    \
        0x0f, 0xf0, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x3f, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x0f, 0xfc, 0xff, 0x03,    \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x07, 0xfe, 0xff, 0x01, 0x00, 0x00, 0x3f, 0x00,    \
        0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x83, 0xff, 0xff, 0x01, 0x00, 0xc0, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00,    \
        0x00, 0xfc, 0xff, 0xff, 0xe1, 0xff, 0xff, 0x03, 0x00, 0xf0, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff,    \
        0xf8, 0xff, 0xff, 0x03, 0x00, 0xf8, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 0x03,    \
        0x00, 0xfe, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xdf, 0xff, 0xff, 0xff, 0x07, 0x00, 0xff, 0xff, 0x01,    \
        0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x80, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00,    \
        0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0xc0, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff,    \
        0xff, 0xff, 0xff, 0x3f, 0xc0, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    \
        0xc0, 0xff, 0xff, 0xff, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x81, 0xff, 0xff, 0xff,    \
        0x1f, 0x00, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff

#define THREAD_BITMAP                                                                                                              \
    0x1F, 0xF8, 0x07, 0xE0, 0x03, 0xCE, 0x01, 0x9F, 0x01, 0xB3, 0x00, 0x33, 0xF0, 0x3F, 0xF8, 0x1F, 0x0C, 0x03, 0x0C, 0x03, 0x18,  \
        0x03, 0x11, 0x83, 0x01, 0x83, 0x03, 0xC3, 0x07, 0xE3, 0x1F, 0xFB, 0x7f, 0xff, 0xff, 0xff

#define MATTER_LOGO_BITMAP                                                                                                         \
    0xFF, 0xFF, 0xFF, 0xF3, 0xFF, 0xCF, 0xFF, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0x12, 0xFF, 0x01, 0xFE, 0xFF, 0xFF, 0xF3, 0x3F, 0x8F,  \
        0x7F, 0xFC, 0xFC, 0xFC, 0xE3, 0xF1, 0x87, 0x87, 0xC7, 0xDE, 0x88, 0x33, 0xC7, 0xCF, 0xFC, 0xFF, 0xFF, 0x03

#define SILICONLABS_X_POSITION ((LCD_CTX()->pDisplayGeometry->xSize - SILICONLABS_BITMAP_WIDTH) / 2)
#define SILICONLABS_Y_POSITION 0

namespace {
// Custom UI callback used by SilabsLCD::WriteDemoUI().
static const uint8_t siliconlabsBitmap[] = { SILABS_BITMAP };
static const uint8_t matterLogoBitmap[]  = { MATTER_LOGO_BITMAP };

static const uint8_t networkBitMap[] = { THREAD_BITMAP };

void DrawCustomDemoUI(GLIB_Context_t * context)
{
    uint8_t lineNb = 5;
    char str[20];


    GLIB_clear(LCD_CTX());
    GLIB_drawBitmap(LCD_CTX(), SILICONLABS_X_POSITION, SILICONLABS_Y_POSITION, SILICONLABS_BITMAP_WIDTH,
                    SILICONLABS_BITMAP_HEIGHT, siliconlabsBitmap);
    GLIB_drawBitmap(LCD_CTX(), PROT1_X_POSITION, PROT1_Y_POSITION, PROT1_BITMAP_WIDTH, PROT1_BITMAP_HEIGHT, networkBitMap);
    GLIB_drawBitmap(LCD_CTX(), PROT2_X_POSITION, PROT2_Y_POSITION, MATTER_LOGO_WIDTH, MATTER_LOGO_HEIGHT, matterLogoBitmap);
    
    
    sprintf(str, "E-Meter");
    GLIB_drawStringOnLine(LCD_CTX(), str, lineNb++, GLIB_ALIGN_CENTER, 0, 0, true);
    lineNb++; //empty lines
    lineNb++; //empty lines
    sprintf(str, "%ld", static_cast<long>(AppTask::GetAppTask().GetPriceEuroCents()));
    GLIB_drawStringOnLine(LCD_CTX(), str, lineNb++, GLIB_ALIGN_CENTER, 0, 0, true);
    sprintf(str, "ct/kWh");
    GLIB_drawStringOnLine(LCD_CTX(), str, lineNb++, GLIB_ALIGN_CENTER, 0, 0, true);

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

    err = DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t) {
        const CHIP_ERROR initError = InitializeMeterMatter();
        if (initError != CHIP_NO_ERROR)
        {
            ChipLogError(AppServer, "Meter measurement initialization failed: %s", ErrorStr(initError));
            appError(initError);
        }
    }, 0);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Could not schedule meter measurement initialization: %s", ErrorStr(err));
        return err;
    }

    sMeterTimer = xTimerCreate("meter", pdMS_TO_TICKS(kMeterUpdatePeriodMs), pdTRUE, nullptr, MeterTimerCallback);
    if (sMeterTimer == nullptr || xTimerStart(sMeterTimer, 0) != pdPASS)
    {
        ChipLogError(AppServer, "Could not start simulated meter timer");
        return CHIP_ERROR_INTERNAL;
    }

    sPriceLED.Init(LIGHT_LED);
    sPriceLED.Set(false);

    // Publish initial price at boot.
    ApplyPricePreset(mPricePreset);

#ifdef DISPLAY_ENABLED
    // Install a custom UI renderer for the Demo screen.
    GetLCD().SetCustomUI(DrawCustomDemoUI);

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

void AppTask::ApplyMeterProfile(uint8_t profile)
{
    if (!sMeter.SetProfile(profile))
    {
        ChipLogError(AppServer, "Ignoring unsupported meter profile %u", profile);
    }
}

void AppTask::UpdateMeterMeasurements(intptr_t)
{
    const auto reading = sMeter.Tick(kMeterUpdatePeriodMs / 1000);
    sPowerDelegate.SetPower(reading.activePowerW);
    MatterReportingAttributeChangeCallback(kMeterEndpoint, ElectricalPowerMeasurement::Id,
        ElectricalPowerMeasurement::Attributes::ActivePower::Id);

    using namespace ElectricalEnergyMeasurement;
    Structs::EnergyMeasurementStruct::Type measurement{};
    measurement.startTimestamp.ClearValue();
    measurement.startSystime.ClearValue();
    measurement.energy = reading.cumulativeImportedEnergyMWh;
    const auto uptime = std::chrono::duration_cast<System::Clock::Milliseconds64>(Server::GetInstance().TimeSinceInit());
    measurement.endSystime.SetValue(static_cast<uint64_t>(uptime.count()));
    NotifyCumulativeEnergyMeasured(kMeterEndpoint, DataModel::MakeNullable(measurement), DataModel::NullNullable);
    MatterReportingAttributeChangeCallback(kMeterEndpoint, Id, Attributes::CumulativeEnergyImported::Id);

    measurement.energy = reading.periodicImportedEnergyMWh;
    NotifyPeriodicEnergyMeasured(kMeterEndpoint, DataModel::MakeNullable(measurement), DataModel::NullNullable);
    MatterReportingAttributeChangeCallback(kMeterEndpoint, Id, Attributes::PeriodicEnergyImported::Id);
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
