#include "Body.h"

namespace PPBOS::Body
{
	namespace
	{
		constexpr std::array kGen{
			"NPC Genitals01 [Gen01]", "NPC Genitals02 [Gen02]", "NPC Genitals03 [Gen03]",
			"NPC Genitals04 [Gen04]", "NPC Genitals05 [Gen05]", "NPC Genitals06 [Gen06]"
		};

		struct Bone
		{
			const char* a;
			const char* b;
			float       radius;
		};

		// XP32 humanoid bones. Radii are generous on purpose: a spurt that grazes skin should still mark it.
		constexpr std::array kBones{
			Bone{ "NPC Pelvis [Pelv]", "NPC Spine [Spn0]", 10.0f },
			Bone{ "NPC Spine [Spn0]", "NPC Spine1 [Spn1]", 9.5f },
			Bone{ "NPC Spine1 [Spn1]", "NPC Spine2 [Spn2]", 9.5f },
			Bone{ "NPC Spine2 [Spn2]", "NPC Neck [Neck]", 8.5f },
			Bone{ "NPC Neck [Neck]", "NPC Head [Head]", 4.5f },
			Bone{ "NPC Head [Head]", "NPC Head [Head]", 6.5f },  // sphere
			Bone{ "NPC L Thigh [LThg]", "NPC L Calf [LClf]", 6.0f },
			Bone{ "NPC R Thigh [RThg]", "NPC R Calf [RClf]", 6.0f },
			Bone{ "NPC L Calf [LClf]", "NPC L Foot [Lft ]", 4.0f },
			Bone{ "NPC R Calf [RClf]", "NPC R Foot [Rft ]", 4.0f },
			Bone{ "NPC L UpperArm [LUar]", "NPC L Forearm [LLar]", 3.8f },
			Bone{ "NPC R UpperArm [RUar]", "NPC R Forearm [RLar]", 3.8f },
			Bone{ "NPC L Forearm [LLar]", "NPC L Hand [LHnd]", 3.2f },
			Bone{ "NPC R Forearm [RLar]", "NPC R Hand [RHnd]", 3.2f },
		};

		constexpr std::array kGripR{ "NPC R Hand [RHnd]", "NPC R Finger11 [RF11]", "NPC R Finger21 [RF21]", "NPC R Finger31 [RF31]", "NPC R Finger01 [RF01]" };
		constexpr std::array kGripL{ "NPC L Hand [LHnd]", "NPC L Finger11 [LF11]", "NPC L Finger21 [LF21]", "NPC L Finger31 [LF31]", "NPC L Finger01 [LF01]" };

		inline RE::NiPoint3 Lerp(const RE::NiPoint3& a, const RE::NiPoint3& b, float t) { return a + (b - a) * t; }
	}

	RE::NiAVObject* Find(RE::NiAVObject* a_root, const char* a_name)
	{
		return a_root ? a_root->GetObjectByName(RE::BSFixedString(a_name)) : nullptr;
	}

	std::optional<Schlong> PlayerSchlong()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return std::nullopt;
		}
		for (const bool firstPerson : { false, true }) {
			auto* root = player->Get3D(firstPerson);
			auto* base = Find(root, kGen[0]);
			if (!base) {
				continue;
			}
			RE::NiAVObject* tip = nullptr;
			const char*     tipName = nullptr;
			for (auto i = kGen.size(); i-- > 1;) {
				if ((tip = Find(root, kGen[i]))) {
					tipName = kGen[i];
					break;
				}
			}
			if (!tip) {
				continue;
			}
			Schlong s{ base->world.translate, tip->world.translate, {}, 0.0f, tipName, firstPerson };
			const auto axis = s.tip - s.base;
			s.length = axis.Length();
			if (s.length < 1.0f) {
				continue;  // nodes collapsed (model swap in progress)
			}
			s.dir = axis / s.length;
			return s;
		}
		return std::nullopt;
	}

	void PlayerHandPoints(std::vector<HandPoint>& a_out)
	{
		a_out.clear();
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		for (const bool firstPerson : { false, true }) {
			auto* root = player->Get3D(firstPerson);
			if (!root) {
				continue;
			}
			for (const auto* n : kGripR) {
				if (auto* node = Find(root, n)) {
					a_out.push_back({ node->world.translate, false });
				}
			}
			for (const auto* n : kGripL) {
				if (auto* node = Find(root, n)) {
					a_out.push_back({ node->world.translate, true });
				}
			}
		}
	}

	void Capsules(RE::Actor* a_actor, float a_radiusScale, std::vector<Capsule>& a_out)
	{
		a_out.clear();
		auto* root = a_actor ? a_actor->Get3D(false) : nullptr;
		if (!root) {
			return;
		}
		const float scale = a_actor->GetScale() * a_radiusScale;
		for (const auto& bone : kBones) {
			auto* a = Find(root, bone.a);
			auto* b = Find(root, bone.b);
			if (a && b) {
				a_out.push_back({ a->world.translate, b->world.translate, bone.radius * scale, bone.a });
			}
		}
		if (a_out.empty()) {  // creature / unknown skeleton: one sphere around the middle of its bound
			const auto& wb = root->worldBound;
			if (wb.radius > 0.0f) {
				a_out.push_back({ wb.center, wb.center, wb.radius * 0.6f, "" });
			}
		}
	}

	float PointSegment(const RE::NiPoint3& p, const RE::NiPoint3& a, const RE::NiPoint3& b, float& t)
	{
		const auto ab = b - a;
		const float len2 = ab.SqrLength();
		t = len2 > 1e-6f ? std::clamp((p - a).Dot(ab) / len2, 0.0f, 1.0f) : 0.0f;
		return (p - Lerp(a, b, t)).SqrLength();
	}

	float SegmentSegment(const RE::NiPoint3& p1, const RE::NiPoint3& q1, const RE::NiPoint3& p2, const RE::NiPoint3& q2, float& s, float& t)
	{
		// Ericson, Real-Time Collision Detection 5.1.9
		const auto  d1 = q1 - p1;
		const auto  d2 = q2 - p2;
		const auto  r = p1 - p2;
		const float a = d1.Dot(d1);
		const float e = d2.Dot(d2);
		const float f = d2.Dot(r);
		constexpr float eps = 1e-6f;

		if (a <= eps && e <= eps) {
			s = t = 0.0f;
			return r.SqrLength();
		}
		if (a <= eps) {
			s = 0.0f;
			t = std::clamp(f / e, 0.0f, 1.0f);
		} else {
			const float c = d1.Dot(r);
			if (e <= eps) {
				t = 0.0f;
				s = std::clamp(-c / a, 0.0f, 1.0f);
			} else {
				const float b = d1.Dot(d2);
				const float denom = a * e - b * b;
				s = denom != 0.0f ? std::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
				t = (b * s + f) / e;
				if (t < 0.0f) {
					t = 0.0f;
					s = std::clamp(-c / a, 0.0f, 1.0f);
				} else if (t > 1.0f) {
					t = 1.0f;
					s = std::clamp((b - c) / a, 0.0f, 1.0f);
				}
			}
		}
		return ((p1 + d1 * s) - (p2 + d2 * t)).SqrLength();
	}

	void Nearby(const RE::NiPoint3& a_center, float a_range, bool a_includeDead, std::vector<RE::Actor*>& a_out)
	{
		a_out.clear();
		auto* lists = RE::ProcessLists::GetSingleton();
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!lists) {
			return;
		}
		const float range2 = a_range * a_range;
		std::vector<std::pair<float, RE::Actor*>> found;
		for (auto& handle : lists->highActorHandles) {
			auto ptr = handle.get();
			auto* actor = ptr.get();
			if (!actor || actor == player || actor->IsDisabled() || !actor->Get3D(false)) {
				continue;
			}
			if (!a_includeDead && actor->IsDead()) {
				continue;
			}
			const float d2 = (actor->GetPosition() - a_center).SqrLength();
			if (d2 <= range2) {
				found.emplace_back(d2, actor);
			}
		}
		std::ranges::sort(found, {}, &std::pair<float, RE::Actor*>::first);
		for (auto& [d, a] : found) {
			a_out.push_back(a);
		}
	}
}
