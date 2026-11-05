// PPB_OStrands - bridges Precision Physic Bodies' touch-driven erection to OStrands' semen, and marks the NPCs
// the spurts land on with decals.
//
//   PPB  (closed DLL)   bends the player's schlong up while a hand is on the groin, and announces
//                        PPB_PlayerMasturbation when the hand lets go at full erection.
//   THIS                measures the hand's stroke travel along the erect shaft; at [Trigger] ClimaxStrokeU it
//                        - makes OStrands climax the player (OStrands' own entry point, see OStrandsLink.cpp),
//                          with every nearby actor as a collision body so the fluid lands on them,
//                        - sweeps the spurts' path and puts a decal on every NPC body it reaches,
//                        - announces PPB_OStrands_Climax (numArg = stroke travel, sender = player).

#include "Body.h"
#include "OStrandsLink.h"
#include "PathDecals.h"
#include "Settings.h"
#include "StrokeMeter.h"

namespace PPBOS
{
	namespace
	{
		std::atomic_bool g_taskPending{ false };
		std::atomic_bool g_running{ false };
		float            g_lastTick = 0.0f;
		float            g_lastIniCheck = 0.0f;
		std::filesystem::file_time_type g_iniStamp{};

		void Notify(const char* a_text)
		{
			if (!Settings::Get().notifications) {
				return;
			}
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) {
				return;
			}
			std::unique_ptr<RE::BSScript::IFunctionArguments> args{ RE::MakeFunctionArguments(RE::BSFixedString(a_text)) };
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
			vm->DispatchStaticCall("Debug", "Notification", args.get(), result);
		}

		void Climax(float a_meter, const char* a_why)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}
			const auto& cfg = Settings::Get();
			const float now = Now();

			std::vector<RE::Actor*> bodies;
			Body::Nearby(player->GetPosition(), cfg.collisionRangeU, true, bodies);
			bodies.insert(bodies.begin(), player);  // the player's own body too, as OStim scenes include the climaxer

			logger::info("CLIMAX ({}) meter={:.0f}, {} body(ies) within {:.0f}u", a_why, a_meter, bodies.size(), cfg.collisionRangeU);

			const bool fluid = OStrandsLink::Climax(player, bodies);
			PathDecals::Begin(now);

			SKSE::ModCallbackEvent ev{ "PPB_OStrands_Climax", fluid ? "OStrands" : "NoOStrands", a_meter, player };
			if (auto* src = SKSE::GetModCallbackEventSource()) {
				src->SendEvent(&ev);
			}
			// always say that the trigger fired: if this shows and no semen appears, the problem is on OStrands' side
			Notify(fluid ? "PPB OStrands: climax" : "PPB OStrands: climax - OStrands not available, see PPB_OStrands.log");
		}

		void TestClimax(const char* a_why)
		{
			StrokeMeter::Reset();
			StrokeMeter::StartCooldown(Now());
			Climax(0.0f, a_why);
		}

		void OnStrokeClimax(float a_meter) { Climax(a_meter, "stroke"); }

		void HotReload(float a_now)
		{
			if (a_now - g_lastIniCheck < 1.0f) {
				return;
			}
			g_lastIniCheck = a_now;
			std::error_code ec;
			const auto stamp = std::filesystem::last_write_time("Data/SKSE/Plugins/PPB_OStrands.ini", ec);
			if (!ec && stamp != g_iniStamp) {
				const bool first = g_iniStamp == std::filesystem::file_time_type{};
				g_iniStamp = stamp;
				if (!first) {
					const int before = Settings::Get().testClimax;
					Settings::Get().Load();
					logger::info("PPB_OStrands.ini changed - reloaded");
					if (Settings::Get().testClimax == 1 && before != 1) {
						TestClimax("TestClimax = 1 in the INI");
					}
				}
			}
		}

		void MainTick()
		{
			const float now = Now();
			const float dt = (std::min)(now - g_lastTick, 0.25f);
			g_lastTick = now;

			HotReload(now);
			if (!Settings::Get().enabled) {
				StrokeMeter::Reset();
				return;
			}
			if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
				return;
			}
			StrokeMeter::Tick(now, dt, &OnStrokeClimax);
			PathDecals::Tick(now);
		}

		void TickThread()
		{
			while (g_running) {
				const auto hz = Settings::Get().tickHz;
				std::this_thread::sleep_for(std::chrono::duration<float>(1.0f / hz));
				if (!g_taskPending.exchange(true)) {  // never queue a second tick behind a slow frame
					SKSE::GetTaskInterface()->AddTask([] {
						MainTick();
						g_taskPending = false;
					});
				}
			}
		}

		// [General] TestKey: climax on a key press, bypassing the stroke meter (tells a trigger problem from an
		// OStrands problem in one press).
		class InputSink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			static InputSink* Get()
			{
				static InputSink s;
				return &s;
			}

			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				const int key = Settings::Get().testKey;
				if (!a_event || key <= 0) {
					return RE::BSEventNotifyControl::kContinue;
				}
				for (auto* e = *a_event; e; e = e->next) {
					if (e->GetEventType() != RE::INPUT_EVENT_TYPE::kButton || e->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
						continue;
					}
					const auto* b = e->AsButtonEvent();
					if (b && b->IsDown() && static_cast<int>(b->GetIDCode()) == key) {
						logger::info("TestKey {} pressed", key);
						SKSE::GetTaskInterface()->AddTask([] { TestClimax("TestKey"); });
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// PPB's own announcement: hand released the groin at full erection.
		class ModEventSink : public RE::BSTEventSink<SKSE::ModCallbackEvent>
		{
		public:
			static ModEventSink* Get()
			{
				static ModEventSink s;
				return &s;
			}

			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
			{
				if (a_event && a_event->eventName == "PPB_PlayerMasturbation") {
					const float level = a_event->numArg;
					logger::info("PPB_PlayerMasturbation received (erection level {:.0f})", level);
					if (Settings::Get().enabled && Settings::Get().triggerOnPPBRelease) {
						SKSE::GetTaskInterface()->AddTask([level] {
							const float now = Now();
							if (!StrokeMeter::CoolingDown(now)) {
								StrokeMeter::Reset();
								StrokeMeter::StartCooldown(now);
								Climax(level, "PPB release");
							}
						});
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		void OnMessage(SKSE::MessagingInterface::Message* a_msg)
		{
			switch (a_msg->type) {
			case SKSE::MessagingInterface::kDataLoaded:
				OStrandsLink::Connect();
				PathDecals::Init();
				if (auto* src = SKSE::GetModCallbackEventSource()) {
					src->AddEventSink(ModEventSink::Get());
				}
				if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
					input->AddEventSink(InputSink::Get());
				}
				if (!g_running.exchange(true)) {
					std::thread(TickThread).detach();
				}
				break;
			case SKSE::MessagingInterface::kPostLoadGame:
			case SKSE::MessagingInterface::kNewGame:
				StrokeMeter::Reset();
				break;
			default:
				break;
			}
		}

		void InitLog()
		{
			auto dir = SKSE::log::log_directory();
			if (!dir) {
				return;
			}
			auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((*dir / "PPB_OStrands.log").string(), true);
			auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
			log->set_level(spdlog::level::info);
			log->flush_on(spdlog::level::info);
			log->set_pattern("[%H:%M:%S.%e] [%l] %v");
			spdlog::set_default_logger(std::move(log));
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	using namespace PPBOS;
	SKSE::Init(a_skse);
	InitLog();
	logger::info("PPB_OStrands loaded");

	Settings::Get().Load();
	Now();  // start the clock

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		logger::critical("could not register the SKSE message listener");
		return false;
	}
	return true;
}
