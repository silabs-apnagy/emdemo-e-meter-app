#include "SimulatedMeter.h"

namespace {
struct Range { uint16_t minimumW; uint16_t maximumW; };
constexpr Range kRanges[] = { { 0, 200 }, { 500, 1000 }, { 1500, 2500 } };
}

SimulatedMeter::SimulatedMeter() : mProfile(Profile::Night), mPowerW(100), mCumulativeMWh(0),
                                   mFractionalMilliWhSeconds(0), mSample(0) {}

bool SimulatedMeter::SetProfile(uint8_t profile)
{
    if (profile > static_cast<uint8_t>(Profile::Peak)) return false;
    mProfile = static_cast<Profile>(profile);
    return true;
}

SimulatedMeter::Snapshot SimulatedMeter::Tick(uint32_t elapsedSeconds)
{
    const Range range = kRanges[static_cast<uint8_t>(mProfile)];
    // Bounded repeating variation gives the demo plausible readings without an RNG dependency.
    constexpr uint8_t kPattern[] = { 0, 3, 1, 4, 2, 5, 1, 3 };
    const uint32_t span = range.maximumW - range.minimumW;
    mPowerW = range.minimumW + (span == 0 ? 0 : (span * kPattern[mSample++ % 8]) / 7);

    // W * seconds / 3.6 is mWh. Retain the remainder to prevent rounding drift.
    const uint64_t scaled = static_cast<uint64_t>(mPowerW) * elapsedSeconds * 10 + mFractionalMilliWhSeconds;
    const uint64_t incrementMWh = scaled / 36;
    mFractionalMilliWhSeconds = scaled % 36;
    mCumulativeMWh += incrementMWh;
    return { mProfile, mPowerW, mCumulativeMWh, incrementMWh };
}
