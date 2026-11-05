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

		// ── always-on diagnostics: one log line per STATE CHANGE, never per tick ──
		int   g_seenSchlong = -1;  // -1 unknown, 0 no, 1 third person, 2 first person
		int   g_seenErect = -1;
		int   g_seenHands = -1;
		bool  g_seenGrip[2] = { false, false };
		int   g_milestone = 0;     // last quarter of the meter announced (0..3)
		float g_lastHeartbeat = 0.0f;
		std::uint64_t g_ticks = 0;
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

		++g_ticks;
		const auto s = Body::PlayerSchlong();
		const int  schlongState = !s ? 0 : (s->firstPerson ? 2 : 1);
		if (schlongState != g_seenSchlong) {
			g_seenSchlong = schlongState;
			if (!s) {
				logger::info("DIAG schlong: NOT FOUND on the player (no 'NPC Genitals01 [Gen01]' + Gen02..06 on either skeleton) - nothing can trigger");
			} else {
				logger::info("DIAG schlong: found on the {} skeleton, tip '{}', length {:.1f}u",
					s->firstPerson ? "FIRST-person" : "third-person", s->tipNode, s->length);
			}
		}
		if (a_now - g_lastHeartbeat >= 30.0f) {
			g_lastHeartbeat = a_now;
			logger::info("DIAG alive: {} ticks, schlong={}, meter={:.0f}/{:.0f}{}", g_ticks, schlongState ? "yes" : "no",
				g_meter, cfg.climaxStrokeU, CoolingDown(a_now) ? " (cooling down)" : "");
		}
		if (!s) {
			Reset();
			return;
		}

		// Erect = pointing away from the floor. A hanging schlong has dir.z ~ -1 (dot with straight down ~ 1);
		// PPB's touch-driven bend lifts it, and only then do strokes count.
		const float downDot = -s->dir.z;
		const bool  erect = downDot <= cfg.erectMaxDownDot;
		if (static_cast<int>(erect) != g_seenErect) {
			g_seenErect = erect;
			logger::info("DIAG erect: {} (shaft points {:.2f} toward the floor; counts as erect at <= {:.2f}){}", erect ? "YES" : "no",
				downDot, cfg.erectMaxDownDot, cfg.requireErect ? "" : " - RequireErect=0, ignored");
		}

		Body::PlayerHandPoints(g_points);
		if (static_cast<int>(g_points.size()) != g_seenHands) {
			g_seenHands = static_cast<int>(g_points.size());
			logger::info("DIAG hands: {} grip point(s) found on the player's skeletons{}", g_seenHands,
				g_seenHands == 0 ? " - NO HAND NODES, strokes can never count" : "");
		}

		// The gripping zone: the shaft segment, extended a little past both ends so a hand sliding over the tip
		// or pressing at the base still counts as on it.
		const float margin = (std::min)(2.0f, s->length * 0.2f);
		const auto  segA = s->base - s->dir * margin;
		const auto  segB = s->tip + s->dir * margin;
		const float grip2 = cfg.gripRadiusU * cfg.gripRadiusU;

		float travel = 0.0f;
		for (int side = 0; side < 2; ++side) {
			const bool left = side == 1;
			bool       grips = false;
			float      alongSum = 0.0f;
			int        alongN = 0;
			float      nearest2 = 1e9f;
			for (const auto& p : g_points) {
				if (p.left != left) {
					continue;
				}
				float      t;
				const float d2 = Body::PointSegment(p.pos, segA, segB, t);
				nearest2 = (std::min)(nearest2, d2);
				if (d2 <= grip2) {
					grips = true;
					// average every gripping joint: one joint alone flickers in and out of range, which would
					// make the hand's position along the shaft jump
					alongSum += (p.pos - s->base).Dot(s->dir);  // signed distance along the shaft
					++alongN;
				}
			}
			const float along = alongN ? alongSum / static_cast<float>(alongN) : 0.0f;
			if (grips != g_seenGrip[side]) {
				g_seenGrip[side] = grips;
				logger::info("DIAG {} hand: {} the shaft (nearest joint {:.1f}u, GripRadiusU {:.1f})", left ? "left" : "right",
					grips ? "GRIPPING" : "let go of", std::sqrt(nearest2), cfg.gripRadiusU);
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
			g_meter = (std::max)(0.0f, g_meter - cfg.decayPerSec * a_dt);
		}

		if (cfg.debugLog && a_now - g_lastLog >= 1.0f && (g_meter > 0.0f || g_hand[0].gripping || g_hand[1].gripping)) {
			g_lastLog = a_now;
			logger::info("STROKE meter={:.0f}/{:.0f} grip R={} L={} erect={} (down {:.2f}) len={:.1f}u",
				g_meter, cfg.climaxStrokeU, g_hand[0].gripping, g_hand[1].gripping, erect, downDot, s->length);
		}

		const int quarter = (std::min)(3, static_cast<int>(4.0f * g_meter / cfg.climaxStrokeU));
		if (quarter > g_milestone) {
			logger::info("DIAG meter: {}% ({:.0f}/{:.0f}u)", quarter * 25, g_meter, cfg.climaxStrokeU);
		}
		g_milestone = quarter;

		if (g_meter >= cfg.climaxStrokeU && !CoolingDown(a_now)) {
			const float m = g_meter;
			Reset();
			StartCooldown(a_now);
			a_onClimax(m);
		} else if (CoolingDown(a_now)) {
			g_meter = (std::min)(g_meter, cfg.climaxStrokeU * 0.5f);  // stroking during the refractory period builds slowly
		}
	}
}
