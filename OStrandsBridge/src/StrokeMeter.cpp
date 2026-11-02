#include "StrokeMeter.h"
#include "Body.h"
#include "Settings.h"

namespace PPBOS::StrokeMeter
{
	namespace
	{
		struct HandTrack
		{
			bool  gripping = false;
			float along = 0.0f;  // position of the hand along the shaft, units from the base
		};

		float     g_meter = 0.0f;
		float     g_lastStroke = -1000.0f;
		float     g_cooldownUntil = 0.0f;
		float     g_lastLog = 0.0f;
		HandTrack g_hand[2];  // [0] right, [1] left
		std::vector<Body::HandPoint> g_points;
	}

	void Reset()
	{
		g_meter = 0.0f;
		g_hand[0] = g_hand[1] = {};
	}

	void StartCooldown(float a_now) { g_cooldownUntil = a_now + Settings::Get().cooldownSec; }
	bool CoolingDown(float a_now) { return a_now < g_cooldownUntil; }
	float Meter() { return g_meter; }

	void Tick(float a_now, float a_dt, void (*a_onClimax)(float))
	{
		const auto& cfg = Settings::Get();

		const auto s = Body::PlayerSchlong();
		if (!s) {
			Reset();
			return;
		}

		// Erect = pointing away from the floor. A hanging schlong has dir.z ~ -1 (dot with straight down ~ 1);
		// PPB's touch-driven bend lifts it, and only then do strokes count.
		const float downDot = -s->dir.z;
		const bool  erect = downDot <= cfg.erectMaxDownDot;

		Body::PlayerHandPoints(g_points);

		// The gripping zone: the shaft segment, extended a little past both ends so a hand sliding over the tip
		// or pressing at the base still counts as on it.
		const float margin = std::min(2.0f, s->length * 0.2f);
		const auto  segA = s->base - s->dir * margin;
		const auto  segB = s->tip + s->dir * margin;
		const float grip2 = cfg.gripRadiusU * cfg.gripRadiusU;

		float travel = 0.0f;
		for (int side = 0; side < 2; ++side) {
			const bool left = side == 1;
			bool       grips = false;
			float      bestD2 = grip2;
			float      along = 0.0f;
			for (const auto& p : g_points) {
				if (p.left != left) {
					continue;
				}
				float      t;
				const float d2 = Body::PointSegment(p.pos, segA, segB, t);
				if (d2 <= bestD2) {
					bestD2 = d2;
					grips = true;
					along = (p.pos - s->base).Dot(s->dir);  // signed distance along the shaft
				}
			}

			auto& h = g_hand[side];
			if (grips && h.gripping && a_dt > 0.0f) {
				const float moved = std::fabs(along - h.along);
				const float speed = moved / a_dt;
				// below min = tremor / VR jitter; above max = a node teleport (3D rebuild, skeleton swap)
				if (speed >= cfg.minStrokeSpeedU && speed <= cfg.maxStrokeSpeedU && (erect || !cfg.requireErect)) {
					travel += moved;
				}
			}
			h.gripping = grips;
			h.along = along;
		}

		if (travel > 0.0f) {
			g_meter += travel;
			g_lastStroke = a_now;
		} else if (a_now - g_lastStroke > cfg.decayDelaySec) {
			g_meter = std::max(0.0f, g_meter - cfg.decayPerSec * a_dt);
		}

		if (cfg.debugLog && a_now - g_lastLog >= 1.0f && (g_meter > 0.0f || g_hand[0].gripping || g_hand[1].gripping)) {
			g_lastLog = a_now;
			logger::info("STROKE meter={:.0f}/{:.0f} grip R={} L={} erect={} (down {:.2f}) len={:.1f}u",
				g_meter, cfg.climaxStrokeU, g_hand[0].gripping, g_hand[1].gripping, erect, downDot, s->length);
		}

		if (g_meter >= cfg.climaxStrokeU && !CoolingDown(a_now)) {
			const float m = g_meter;
			Reset();
			StartCooldown(a_now);
			a_onClimax(m);
		} else if (CoolingDown(a_now)) {
			g_meter = std::min(g_meter, cfg.climaxStrokeU * 0.5f);  // stroking during the refractory period builds slowly
		}
	}
}
