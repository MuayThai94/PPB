#include "OStrandsLink.h"
#include "Settings.h"

// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
// OStrands ships no API: it only climaxes when OStim (its Climaxed listener) or SexLab (OrgasmStart) tell it to.
// Both of those paths, and its own test key (OStimBridge::TestClimax), end in ONE internal entry point:
//
//   void FluidManager::QueueClimax(FluidManager* self,      // the singleton, from FluidManager::Get()
//                                  int threadId,            // stored on the emitter; 0 = OStim player thread
//                                  ActorHandle who,         // the actor that climaxes (needs a schlong)
//                                  std::vector<Pair> pairs, // who is inside whom; EMPTY = "no penetration",
//                                                           //   a free spurt from the tip
//                                  std::vector<ActorHandle> bodies)  // bodies the fluid collides with
//
// It locks FluidManager's mutex, MOVES both vectors into a 0x48-byte event at the back of its pending queue and
// unlocks; FluidManager::Update (its main-thread frame task) later runs StartClimax on it. The two vectors are
// passed by value, so in the MSVC x64 ABI the caller passes POINTERS to temporaries the callee owns - after the
// call they are moved-from and empty.
//
// Addresses below are RVAs inside 0_OStrands.dll 0.8.0 (SHA-256 93db60ed...855e, 1,162,752 bytes). Each call
// site is checked against the bytes it must start with before it is ever used, so any other OStrands build
// simply turns the bridge's OStrands half off (logged), instead of jumping into the middle of a function.
// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

namespace PPBOS::OStrandsLink
{
	namespace
	{
		constexpr std::uintptr_t kRvaQueueClimax = 0x42330;
		constexpr std::uintptr_t kRvaFluidManagerGet = 0x45CA0;

		// QueueClimax prologue: mov rax,rsp / mov [rax+10],rbx / mov [rax+20],r9 / mov [rax+18],r8d / push rbp,rsi,rdi,r12,r13,r14
		constexpr std::array<std::uint8_t, 24> kSigQueueClimax{
			0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x4C, 0x89, 0x48, 0x20, 0x44,
			0x89, 0x40, 0x18, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56
		};
		// FluidManager::Get: function-local static guard - the rip-relative operand (8C 25 0D 00) pins the build
		constexpr std::array<std::uint8_t, 24> kSigFluidManagerGet{
			0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0x0D, 0x8C, 0x25, 0x0D, 0x00,
			0x65, 0x48, 0x8B, 0x04, 0x25, 0x58, 0x00, 0x00, 0x00, 0xBA, 0x34, 0x00
		};

		// OStrands frees the moved vectors with ITS operator delete, which is ucrtbase's free(). Our own CRT may be
		// static (/MT, a separate heap), so every buffer we hand over is allocated straight from ucrtbase - the one
		// heap both modules agree on. A stateless allocator keeps std::vector's layout identical (3 pointers).
		struct Ucrt
		{
			using malloc_t = void* (*)(std::size_t);
			using free_t = void (*)(void*);
			malloc_t alloc = nullptr;
			free_t   release = nullptr;

			static Ucrt& Get()
			{
				static Ucrt u = [] {
					Ucrt r;
					if (const auto mod = REX::W32::GetModuleHandleW(L"ucrtbase.dll")) {
						r.alloc = reinterpret_cast<malloc_t>(REX::W32::GetProcAddress(mod, "malloc"));
						r.release = reinterpret_cast<free_t>(REX::W32::GetProcAddress(mod, "free"));
					}
					return r;
				}();
				return u;
			}
		};

		template <class T>
		struct UcrtAllocator
		{
			using value_type = T;
			UcrtAllocator() noexcept = default;
			template <class U>
			UcrtAllocator(const UcrtAllocator<U>&) noexcept
			{}

			T* allocate(std::size_t n)
			{
				// keep every block under 4 KiB: MSVC's operator delete treats >= 4 KiB blocks as 32-byte
				// over-aligned (it reads a header in front of the pointer), which a plain malloc block lacks
				if (n * sizeof(T) >= 0x1000) {
					throw std::length_error("OStrands bridge vector too large");
				}
				if (auto p = Ucrt::Get().alloc ? Ucrt::Get().alloc(n * sizeof(T)) : nullptr) {
					return static_cast<T*>(p);
				}
				throw std::bad_alloc();
			}
			void deallocate(T* p, std::size_t) noexcept { Ucrt::Get().release(p); }

			template <class U>
			bool operator==(const UcrtAllocator<U>&) const noexcept { return true; }
		};

		struct Pair  // 12 bytes, as OStrands stores it; we never fill one (empty = free spurt)
		{
			std::uint32_t giver;
			std::uint32_t kind;
			std::uint32_t receiver;
		};
		static_assert(sizeof(Pair) == 12);

		using PairVec = std::vector<Pair, UcrtAllocator<Pair>>;
		using HandleVec = std::vector<std::uint32_t, UcrtAllocator<std::uint32_t>>;
		static_assert(sizeof(PairVec) == 24 && sizeof(HandleVec) == 24);

		using QueueClimax_t = void (*)(void* a_self, std::int32_t a_threadId, std::uint32_t a_who, PairVec* a_pairs, HandleVec* a_bodies);
		using FluidManagerGet_t = void* (*)();

		QueueClimax_t     g_queueClimax = nullptr;
		FluidManagerGet_t g_fluidManagerGet = nullptr;

		template <std::size_t N>
		bool Matches(std::uintptr_t a_addr, const std::array<std::uint8_t, N>& a_sig)
		{
			return std::memcmp(reinterpret_cast<const void*>(a_addr), a_sig.data(), N) == 0;
		}
	}

	bool Connect()
	{
		g_queueClimax = nullptr;
		g_fluidManagerGet = nullptr;

		const auto mod = REX::W32::GetModuleHandleW(L"0_OStrands.dll");
		if (!mod) {
			logger::warn("OStrands: 0_OStrands.dll is not loaded - install OStrands 0.8.0. Stroking still counts, but no semen.");
			return false;
		}
		if (!Ucrt::Get().alloc || !Ucrt::Get().release) {
			logger::error("OStrands: ucrtbase.dll malloc/free not found - refusing to hand memory to OStrands");
			return false;
		}

		const auto base = reinterpret_cast<std::uintptr_t>(mod);
		const auto q = base + kRvaQueueClimax;
		const auto g = base + kRvaFluidManagerGet;
		if (!Matches(q, kSigQueueClimax) || !Matches(g, kSigFluidManagerGet)) {
			logger::error("OStrands: 0_OStrands.dll is not the 0.8.0 build this bridge was mapped against "
						  "(code at +{:X}/+{:X} differs). The OStrands half of the bridge is OFF; decals still work.",
				kRvaQueueClimax, kRvaFluidManagerGet);
			return false;
		}

		g_queueClimax = reinterpret_cast<QueueClimax_t>(q);
		g_fluidManagerGet = reinterpret_cast<FluidManagerGet_t>(g);
		logger::info("OStrands: 0.8.0 recognised at {:X}; climax entry point bound", base);

		const auto& os = Settings::Get().ostrands;
		if (os.semenEnabled == "false" || os.semenEnabled == "0") {
			logger::warn("OStrands: [Semen] Enabled = false - OStrands will refuse every climax. Turn it on in OStrands_User.ini.");
		} else if (os.semenEnabled != "true" && os.semenEnabled != "1") {
			const bool ocum = RE::TESDataHandler::GetSingleton()->LookupLoadedModByName("OCum.esp") != nullptr;
			if (!ocum) {
				logger::warn("OStrands: [Semen] Enabled = auto and OCum is not loaded - OStrands only allows semen in "
							 "SexLab scenes then, so the bridge's climax will be refused ('semen is off' in OStrands.log). "
							 "Set [Semen] Enabled = true in OStrands_User.ini (or its SKSE Menu Framework page).");
			}
		}
		return true;
	}

	bool Ready()
	{
		return g_queueClimax && g_fluidManagerGet;
	}

	bool Climax(RE::Actor* a_actor, std::span<RE::Actor* const> a_bodies)
	{
		if (!Ready() || !a_actor) {
			return false;
		}
		auto* fm = g_fluidManagerGet();
		if (!fm) {
			return false;
		}

		PairVec   pairs;  // empty: "no penetration" - OStrands spurts freely from the tip
		HandleVec bodies;
		bodies.reserve((std::min<std::size_t>)(a_bodies.size(), 64));
		for (auto* b : a_bodies) {
			if (b && bodies.size() < 64) {
				bodies.push_back(b->GetHandle().native_handle());
			}
		}

		{
			// OStrands_User.ini may have been edited since the game started - read it again before judging it
			auto& os = Settings::Get().ostrands;
			Settings::Get().Load();
			logger::info("OStrands config now: [General] Enabled={} DebugLog={} [Semen] Enabled={}", os.enabled, os.debugLog, os.semenEnabled);
			if (!os.enabled) {
				logger::warn("OStrands: [General] Enabled = false - OStrands drops every climax");
			}
			if (os.semenEnabled != "true" && os.semenEnabled != "1") {
				logger::warn("OStrands: [Semen] Enabled = {} - unless OCum is installed OStrands will refuse this climax. Set it to true.", os.semenEnabled);
			}
			if (!os.debugLog) {
				logger::info("OStrands: its own DebugLog is off, so OStrands.log will NOT say if it refuses. Set [General] DebugLog = true in OStrands_User.ini to see why.");
			}
		}

		const auto who = a_actor->GetHandle().native_handle();
		g_queueClimax(fm, Settings::Get().threadId, who, &pairs, &bodies);
		// both vectors were moved into OStrands' queue and are now empty; their destructors are no-ops

		logger::info("OStrands: climax queued for {:08X} with {} collision bodies (thread {})",
			a_actor->GetFormID(), a_bodies.size(), Settings::Get().threadId);
		return true;
	}
}
