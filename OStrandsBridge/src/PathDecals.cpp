#include "PathDecals.h"
#include "Body.h"
#include "Settings.h"

// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
// OStrands' fluid lands on the bodies we hand it, but it is a simulation that fades after [Fluid] Lifetime and
// exposes nothing to other plugins. So the bridge predicts the same spurts itself - a ballistic arc out of the
// tip, OStrands' own gravity, one sweep per pulse, weaker each time - and wherever an arc meets an NPC body
// capsule it fires a decal impact at that spot. Decals go through the engine's own path
// (BGSImpactManager::PlayImpactEffect = Papyrus ObjectReference.PlayImpactEffect), so they behave like blood:
// skinned onto the body, limited and faded by the game's decal settings.
// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

namespace PPBOS::PathDecals
{
	namespace
	{
		constexpr RE::FormID      kImpactSetLocal = 0x802;
		constexpr RE::FormID      kImpactLocal = 0x801;
		constexpr std::string_view kPlugin = "PPB_OStrands.esp"sv;

		RE::BGSImpactDataSet* g_set = nullptr;
		std::vector<std::pair<float, int>> g_due;  // (time, pulse index)
		std::mt19937 g_rng{ 0x5050424Fu };

		struct Hit
		{
			RE::Actor*   actor;
			RE::NiPoint3 point;  // on the capsule surface
			float        along;  // arc time of the hit, to keep the first one
			const char*  node;
		};

		RE::NiPoint3 Jitter(const RE::NiPoint3& a_dir, float a_spreadDeg)
		{
			if (a_spreadDeg <= 0.0f) {
				return a_dir;
			}
			// a random direction inside a cone around a_dir
			RE::NiPoint3 up = std::fabs(a_dir.z) < 0.95f ? RE::NiPoint3{ 0, 0, 1 } : RE::NiPoint3{ 1, 0, 0 };
			auto         u = a_dir.UnitCross(up);
			auto         v = a_dir.Cross(u);
			std::uniform_real_distribution<float> ang(0.0f, 6.2831853f);
			std::uniform_real_distribution<float> rad(0.0f, std::tan(a_spreadDeg * 0.0174533f));
			const float  a = ang(g_rng), r = rad(g_rng);
			auto         d = a_dir + u * (std::cos(a) * r) + v * (std::sin(a) * r);
			d.Unitize();
			return d;
		}

		// Where the engine will start the pick: the named node in the player's CURRENT 3D, or that 3D's root if
		// the node is not in it (the engine's own fallback). Aiming from the same origin keeps the pick on target.
		std::optional<RE::NiPoint3> PickOrigin(const char* a_node)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* root = player ? player->Get3D() : nullptr;
			if (!root) {
				return std::nullopt;
			}
			if (auto* n = Body::Find(root, a_node)) {
				return n->world.translate;
			}
			return root->world.translate;
		}

		void Decal(const Hit& a_hit, const char* a_tipNode)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			const auto origin = PickOrigin(a_tipNode);
			if (!player || !origin || !g_set) {
				return;
			}
			auto       dir = a_hit.point - *origin;
			const float dist = dir.Unitize();
			if (dist < 1.0f) {
				return;
			}
			// the pick runs from the tip straight at the landing spot and a little past it, so it ends inside her
			RE::BGSImpactManager::GetSingleton()->PlayImpactEffect(player, g_set, a_tipNode, dir, dist + 12.0f, false, false);

			if (Settings::Get().debugLog) {
				logger::info("DECAL {:08X} '{}' at {} ({:.0f}u away)", a_hit.actor->GetFormID(),
					a_hit.actor->GetName(), a_hit.node ? a_hit.node : "?", dist);
			}
		}

		void Sweep(int a_pulse)
		{
			const auto& cfg = Settings::Get();
			const auto  s = Body::PlayerSchlong();
			if (!s) {
				return;
			}

			std::vector<RE::Actor*> actors;
			Body::Nearby(s->tip, cfg.decalRangeU, cfg.includeDead, actors);
			if (actors.empty()) {
				return;
			}
			std::vector<std::vector<Body::Capsule>> caps(actors.size());
			for (std::size_t i = 0; i < actors.size(); ++i) {
				Body::Capsules(actors[i], cfg.bodyRadiusScale, caps[i]);
			}

			const float gravity = cfg.gravity > 0.0f ? cfg.gravity : Settings::Get().ostrands.gravity;
			const float speed = cfg.launchSpeedU * (std::max)(0.25f, 1.0f - cfg.pulseFalloff * static_cast<float>(a_pulse));
			constexpr float step = 1.0f / 120.0f;
			const float range2 = cfg.decalRangeU * cfg.decalRangeU;

			std::vector<Hit> hits;
			for (int ray = 0; ray < cfg.raysPerPulse; ++ray) {
				auto p = s->tip;
				auto v = Jitter(s->dir, ray == 0 ? 0.0f : cfg.spreadDeg) * speed;
				std::optional<Hit> best;

				for (float t = 0.0f; t < 2.0f && !best; t += step) {
					auto p1 = p + v * step;
					v.z -= gravity * step;

					for (std::size_t i = 0; i < actors.size(); ++i) {
						for (const auto& c : caps[i]) {
							float sa, sb;
							const float d2 = Body::SegmentSegment(p, p1, c.a, c.b, sa, sb);
							if (d2 > c.radius * c.radius) {
								continue;
							}
							const float when = t + sa * step;
							if (best && best->along <= when) {
								continue;
							}
							// move the contact out onto the capsule surface, facing where the fluid came from
							const auto onAxis = c.a + (c.b - c.a) * sb;
							auto       out = (p + (p1 - p) * sa) - onAxis;
							if (out.Unitize() < 1e-3f) {
								out = -v;
								out.Unitize();
							}
							best = Hit{ actors[i], onAxis + out * c.radius, when, c.node };
						}
					}

					p = p1;
					if ((p - s->tip).SqrLength() > range2 || p.z < s->tip.z - 300.0f) {
						break;
					}
				}
				if (best) {
					hits.push_back(*best);
				}
			}

			int placed = 0;
			for (const auto& h : hits) {
				if (placed++ >= cfg.maxDecalsPerPulse) {
					break;
				}
				Decal(h, s->tipNode);
			}
			if (cfg.debugLog) {
				logger::info("PULSE {} speed {:.0f}u/s: {} ray(s) hit, {} decal(s), {} actor(s) in range",
					a_pulse, speed, hits.size(), (std::min<int>)(placed, cfg.maxDecalsPerPulse), actors.size());
			}
		}
	}

	bool Init()
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		g_set = dh ? dh->LookupForm<RE::BGSImpactDataSet>(kImpactSetLocal, kPlugin) : nullptr;
		auto* impact = dh ? dh->LookupForm<RE::BGSImpactData>(kImpactLocal, kPlugin) : nullptr;
		if (!g_set || !impact) {
			logger::warn("decals: {} is not active (impact set/data not found) - NPC decals are off", kPlugin);
			g_set = nullptr;
			return false;
		}
		// Map EVERY material to our impact: skin, clothing, armour, whatever the pick lands on gets the splat.
		std::size_t n = 0;
		for (auto* mat : dh->GetFormArray<RE::BGSMaterialType>()) {
			if (mat) {
				g_set->impactMap.emplace(mat, impact);
				++n;
			}
		}
		logger::info("decals: impact set {:08X} ready, {} material types mapped", g_set->GetFormID(), n);
		return true;
	}

	void Begin(float a_now)
	{
		const auto& cfg = Settings::Get();
		if (!cfg.decals || !g_set) {
			return;
		}
		const int pulses = cfg.pulses > 0 ? cfg.pulses : (std::max)(1, cfg.ostrands.pulses);
		for (int i = 0; i < pulses; ++i) {
			g_due.emplace_back(a_now + 0.15f + cfg.pulseIntervalSec * static_cast<float>(i), i);
		}
	}

	void Tick(float a_now)
	{
		for (auto it = g_due.begin(); it != g_due.end();) {
			if (it->first <= a_now) {
				Sweep(it->second);
				it = g_due.erase(it);
			} else {
				++it;
			}
		}
	}
}
