#pragma once

#include <cstdint>

// CHIP-independent model of the demo household load and imported energy.
class SimulatedMeter
{
public:
    enum class Profile : uint8_t { Night = 0, Day = 1, Peak = 2 };

    struct Snapshot
    {
        Profile profile;
        uint32_t activePowerW;
        uint64_t cumulativeImportedEnergyMWh;
        uint64_t periodicImportedEnergyMWh;
    };

    SimulatedMeter();
    bool SetProfile(uint8_t profile);
    Snapshot Tick(uint32_t elapsedSeconds);
    Profile GetProfile() const { return mProfile; }

private:
    Profile mProfile;
    uint32_t mPowerW;
    uint64_t mCumulativeMWh;
    uint64_t mFractionalMilliWhSeconds;
    uint32_t mSample;
};
