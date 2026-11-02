#pragma once

namespace PPBOS::StrokeMeter
{
	// One step of the stroke meter (main thread). Measures how far a gripping hand travels ALONG the player's
	// schlong - relative to the shaft, so walking or turning with a still hand never counts - and calls
	// a_onClimax once the travel reaches [Trigger] ClimaxStrokeU.
	void Tick(float a_now, float a_dt, void (*a_onClimax)(float a_meter));

	void Reset();
	void StartCooldown(float a_now);
	[[nodiscard]] bool  CoolingDown(float a_now);
	[[nodiscard]] float Meter();
}
