#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace logger = SKSE::log;
using namespace std::literals;

namespace PPBOS
{
	using Clock = std::chrono::steady_clock;

	// seconds since plugin start, monotonic - every timer in the bridge runs on this
	inline float Now()
	{
		static const auto start = Clock::now();
		return std::chrono::duration<float>(Clock::now() - start).count();
	}
}
