#pragma once

namespace PPBOS::Body
{
	struct Capsule
	{
		RE::NiPoint3 a;
		RE::NiPoint3 b;
		float        radius;
		const char*  node;  // the bone the hit is reported against (decal + log)
	};

	struct Schlong
	{
		RE::NiPoint3 base;  // NPC Genitals01
		RE::NiPoint3 tip;   // NPC Genitals06 (or the furthest Gen node present)
		RE::NiPoint3 dir;   // unit, base -> tip, i.e. the way it points
		float        length;
		const char*  tipNode;
	};

	RE::NiAVObject* Find(RE::NiAVObject* a_root, const char* a_name);

	// The player's schlong from whichever skeleton carries it (third person first - that is where PPB bends it).
	std::optional<Schlong> PlayerSchlong();

	// Every hand point of the player that can grip: palm and the first finger joints, both hands, from the
	// third-person body AND the first-person (VR controller) skeleton, so it works with or without VRIK.
	struct HandPoint
	{
		RE::NiPoint3 pos;
		bool         left;
	};
	void PlayerHandPoints(std::vector<HandPoint>& a_out);

	// Body capsules of an actor built from its skeleton (humanoid bones); creatures without those bones get one
	// sphere from their 3D bound. Radii are game units, scaled by the actor's scale.
	void Capsules(RE::Actor* a_actor, float a_radiusScale, std::vector<Capsule>& a_out);

	// closest points between segments p1-q1 and p2-q2; returns squared distance, s/t = parameters on each
	float SegmentSegment(const RE::NiPoint3& p1, const RE::NiPoint3& q1, const RE::NiPoint3& p2, const RE::NiPoint3& q2, float& s, float& t);
	float PointSegment(const RE::NiPoint3& p, const RE::NiPoint3& a, const RE::NiPoint3& b, float& t);

	// Actors (never the player) within a_range of a_center that have 3D, sorted nearest first.
	void Nearby(const RE::NiPoint3& a_center, float a_range, bool a_includeDead, std::vector<RE::Actor*>& a_out);
}
