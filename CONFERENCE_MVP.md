# E-meter firmware — conference 2.0 preview

Checkpoint: **2026-10-20**. Feature scope is frozen; bug fixes/validation only.
Working branch: `ElectricalMeter2.0`. Committed base: `7ec2337`; mode-linked pricing
and LCD work remain local changes to commit before updating the super-repo pin.

## Current local behavior

- xG24 / BRD4187C Matter-over-Thread simulated **house load**, not measured grid import.
- Separate `SimulatedMeter` model; AppTask handles CHIP/Matter interaction.
- Mode Select Night / Day / Peak with varying load in configured profile bands:
  0–0.2 kW, 0.5–1.0 kW, and 1.5–2.5 kW respectively. These are power units, not kWh.
- Each profile selects the matching Commodity Price preset: Night Low (€1.13/kWh),
  Day Medium (€3.99/kWh), Peak High (€7.65/kWh). Demo prices are intentionally large.
- Button and Matter mode changes use the same profile/price path.
- LCD prints `Mode:` and its value on separate lines, like the price display.
- The dashboard estimates grid import separately from EVSE + house − solar.

## Verification / next handoff

- App builds succeeded earlier in this work; not rebuilt for this documentation audit.
- Verify the flashed revision, button/Matter consistency, price normalization, and
  full-loop behavior before packaging the conference kit.
- External price APIs, price forecasts/tariff scheduling, physical net-grid metering,
  and direct meter-to-EVSE communication remain outside this preview.

Full-system status and pinning handoff: `energy-management-demo/docs/conference-status.md`.
