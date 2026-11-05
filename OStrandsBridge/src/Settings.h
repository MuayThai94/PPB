#pragma once

namespace PPBOS
{
	// PPB_OStrands.ini - see the shipped file for what every key does.
	struct Settings
	{
		// [General]
		bool enabled = true;
		bool debugLog = false;
		bool notifications = true;
		bool givePower = true;  // add the 'Climax' lesser power to the player (PPB_OStrands.esp)
		int  testKey = 0;       // DirectInput scan code; pressing it climaxes now (bypasses the stroke meter)
		int  testClimax = 0;    // set to 1 in the INI while playing = one climax now

		// [Trigger]
		float gripRadiusU = 7.0f;
		float minStrokeSpeedU = 4.0f;
		float maxStrokeSpeedU = 400.0f;
		float climaxStrokeU = 150.0f;
		float decayPerSec = 15.0f;
		float decayDelaySec = 3.0f;
		bool  requireErect = true;
		float erectMaxDownDot = 0.5f;
		float cooldownSec = 30.0f;
		bool  triggerOnPPBRelease = false;
		float tickHz = 30.0f;

		// [OStrands]
		std::int32_t threadId = 7340;
		float        collisionRangeU = 400.0f;

		// [Decals]
		bool  decals = true;
		float decalRangeU = 350.0f;
		float launchSpeedU = 260.0f;
		float pulseFalloff = 0.18f;
		int   pulses = 0;         // 0 = OStrands' own [Semen] Pulses
		float pulseIntervalSec = 0.45f;
		float gravity = 0.0f;     // 0 = OStrands' own [Physics] Gravity
		int   raysPerPulse = 3;
		float spreadDeg = 7.0f;
		float bodyRadiusScale = 1.0f;
		bool  includeDead = true;
		int   maxDecalsPerPulse = 3;

		// read from OStrands.ini + OStrands_User.ini (the user file wins)
		struct OStrandsView
		{
			std::string semenEnabled = "auto";  // true / false / auto
			bool        enabled = true;         // [General] Enabled - off = OStrands drops every queued climax
			bool        debugLog = false;       // [General] DebugLog - OStrands only logs a REFUSED climax when on
			int         pulses = 5;
			float       gravity = 350.0f;
		} ostrands;

		static Settings& Get()
		{
			static Settings s;
			return s;
		}

		void Load();
	};
}
