#pragma once

namespace PPBOS::OStrandsLink
{
	// Finds 0_OStrands.dll and checks it is the exact build this bridge was mapped against (OStrands 0.8.0).
	// Call once at kPostLoad or later. Logs and returns false - never crashes - on any mismatch.
	bool Connect();

	[[nodiscard]] bool Ready();

	// Makes OStrands start a climax on a_actor exactly as its own OStim/SexLab bridges do: semen spurts from the
	// tip of a_actor's schlong and its fluid collides with, and rests on, every actor in a_bodies.
	// Thread-safe on OStrands' side (it queues under its own mutex); we call it from the main thread anyway.
	bool Climax(RE::Actor* a_actor, std::span<RE::Actor* const> a_bodies);
}
