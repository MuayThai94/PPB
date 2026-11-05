#include "Settings.h"

namespace PPBOS
{
	namespace
	{
		// Minimal INI reader: [Section] / key = value, ';' and '#' start a comment anywhere on a line.
		// Keys are matched case-insensitively. Good enough for our own file and for OStrands'.
		using IniMap = std::unordered_map<std::string, std::string>;  // "section.key" -> value

		std::string Lower(std::string s)
		{
			std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		std::string Trim(std::string_view s)
		{
			const auto b = s.find_first_not_of(" \t\r\n");
			if (b == std::string_view::npos) {
				return {};
			}
			const auto e = s.find_last_not_of(" \t\r\n");
			return std::string(s.substr(b, e - b + 1));
		}

		void ReadInto(const std::filesystem::path& a_path, IniMap& a_out)
		{
			std::ifstream in(a_path);
			if (!in) {
				return;
			}
			std::string section, line;
			while (std::getline(in, line)) {
				if (const auto c = line.find_first_of(";#"); c != std::string::npos) {
					line.erase(c);
				}
				line = Trim(line);
				if (line.empty()) {
					continue;
				}
				if (line.front() == '[' && line.back() == ']') {
					section = Lower(Trim(std::string_view(line).substr(1, line.size() - 2)));
					continue;
				}
				const auto eq = line.find('=');
				if (eq == std::string::npos) {
					continue;
				}
				a_out[section + "." + Lower(Trim(std::string_view(line).substr(0, eq)))] = Trim(std::string_view(line).substr(eq + 1));
			}
		}

		void Read(const IniMap& a_ini, const char* a_key, float& a_out)
		{
			if (const auto it = a_ini.find(Lower(a_key)); it != a_ini.end()) {
				try {
					a_out = std::stof(it->second);
				} catch (...) {
					logger::warn("{} = '{}' is not a number; keeping {}", a_key, it->second, a_out);
				}
			}
		}

		void Read(const IniMap& a_ini, const char* a_key, int& a_out)
		{
			float f = static_cast<float>(a_out);
			Read(a_ini, a_key, f);
			a_out = static_cast<int>(f);
		}

		void Read(const IniMap& a_ini, const char* a_key, bool& a_out)
		{
			if (const auto it = a_ini.find(Lower(a_key)); it != a_ini.end()) {
				const auto v = Lower(it->second);
				if (v == "1" || v == "true" || v == "on") {
					a_out = true;
				} else if (v == "0" || v == "false" || v == "off") {
					a_out = false;
				}
			}
		}

		void Read(const IniMap& a_ini, const char* a_key, std::string& a_out)
		{
			if (const auto it = a_ini.find(Lower(a_key)); it != a_ini.end()) {
				a_out = Lower(it->second);
			}
		}
	}

	void Settings::Load()
	{
		const std::filesystem::path dir = "Data/SKSE/Plugins";

		IniMap ini;
		ReadInto(dir / "PPB_OStrands.ini", ini);

		Read(ini, "general.Enabled", enabled);
		Read(ini, "general.DebugLog", debugLog);
		Read(ini, "general.Notifications", notifications);
		Read(ini, "general.GivePower", givePower);
		Read(ini, "general.TestKey", testKey);
		Read(ini, "general.TestClimax", testClimax);

		Read(ini, "trigger.GripRadiusU", gripRadiusU);
		Read(ini, "trigger.MinStrokeSpeedU", minStrokeSpeedU);
		Read(ini, "trigger.MaxStrokeSpeedU", maxStrokeSpeedU);
		Read(ini, "trigger.ClimaxStrokeU", climaxStrokeU);
		Read(ini, "trigger.DecayPerSec", decayPerSec);
		Read(ini, "trigger.DecayDelaySec", decayDelaySec);
		Read(ini, "trigger.RequireErect", requireErect);
		Read(ini, "trigger.ErectMaxDownDot", erectMaxDownDot);
		Read(ini, "trigger.CooldownSec", cooldownSec);
		Read(ini, "trigger.TriggerOnPPBRelease", triggerOnPPBRelease);
		Read(ini, "trigger.TickHz", tickHz);

		int tid = threadId;
		Read(ini, "ostrands.ThreadId", tid);
		threadId = tid == 0 ? 7340 : tid;  // 0 is OStim's player thread - never borrow it
		Read(ini, "ostrands.CollisionRangeU", collisionRangeU);

		Read(ini, "decals.Enabled", decals);
		Read(ini, "decals.MaxRangeU", decalRangeU);
		Read(ini, "decals.LaunchSpeedU", launchSpeedU);
		Read(ini, "decals.PulseFalloff", pulseFalloff);
		Read(ini, "decals.Pulses", pulses);
		Read(ini, "decals.PulseIntervalSec", pulseIntervalSec);
		Read(ini, "decals.Gravity", gravity);
		Read(ini, "decals.RaysPerPulse", raysPerPulse);
		Read(ini, "decals.SpreadDeg", spreadDeg);
		Read(ini, "decals.BodyRadiusScale", bodyRadiusScale);
		Read(ini, "decals.IncludeDead", includeDead);
		Read(ini, "decals.MaxDecalsPerPulse", maxDecalsPerPulse);

		tickHz = std::clamp(tickHz, 10.0f, 90.0f);
		raysPerPulse = std::clamp(raysPerPulse, 1, 8);
		maxDecalsPerPulse = std::clamp(maxDecalsPerPulse, 1, 8);
		climaxStrokeU = (std::max)(climaxStrokeU, 1.0f);

		IniMap os;
		ReadInto(dir / "OStrands.ini", os);
		ReadInto(dir / "OStrands_User.ini", os);  // read second: the user's file wins, as in OStrands
		Read(os, "semen.Enabled", ostrands.semenEnabled);
		Read(os, "general.Enabled", ostrands.enabled);
		Read(os, "general.DebugLog", ostrands.debugLog);
		Read(os, "semen.Pulses", ostrands.pulses);
		Read(os, "physics.Gravity", ostrands.gravity);

		logger::info("settings: enabled={} grip={:.1f}u minSpeed={:.1f}u/s climaxAt={:.0f}u erect={} (dot<={:.2f}) cooldown={:.0f}s ppbRelease={} thread={} decals={}",
			enabled, gripRadiusU, minStrokeSpeedU, climaxStrokeU, requireErect, erectMaxDownDot, cooldownSec,
			triggerOnPPBRelease, threadId, decals);
		logger::info("OStrands settings seen: [Semen] Enabled={} Pulses={} [Physics] Gravity={:.0f}",
			ostrands.semenEnabled, ostrands.pulses, ostrands.gravity);
	}
}
