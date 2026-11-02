#pragma once

namespace PPBOS::PathDecals
{
	// Looks up the decal impact set in PPB_OStrands.esp and maps every material type to it (kDataLoaded).
	bool Init();

	// Schedules one decal sweep per semen pulse, starting now. Each sweep flies a few ballistic rays out of the
	// player's tip along the shaft and marks every NPC body one of them lands on.
	void Begin(float a_now);

	// Fires the sweeps that are due (main thread).
	void Tick(float a_now);
}
