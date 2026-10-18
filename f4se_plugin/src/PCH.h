#pragma once

#include "F4SE/F4SE.h"
#include "RE/Fallout.h"

#include <charconv>
#include <unordered_set>

#define DLLEXPORT __declspec(dllexport)

using namespace std::literals;

namespace NQT
{
	// Address Library id of an engine function this plugin calls itself: OG (1.10.163) or AE (1.11.x) DLL.
	// AE ids were matched in the 1.11.191 disassembly (tools/fo4_ae.py), see the comment at each use.
	[[nodiscard]] constexpr std::uint64_t GameID(std::uint64_t a_og, [[maybe_unused]] std::uint64_t a_ae) noexcept
	{
#ifdef NQT_AE
		return a_ae;
#else
		return a_og;
#endif
	}
}

#ifdef NQT_AE
#	include "Scaleform/Scaleform.h"

// libxse/commonlibf4 names that differ from alandtse/CommonLibF4.
namespace RE
{
	namespace Scaleform = ::Scaleform;
	using DIFFICULTY_LEVEL = DifficultyLevel;
}

// libxse/commonlibf4 logs through REX::Impl::Log; same call shape as alandtse's F4SE::log.
namespace logger
{
	template <class... T>
	struct debug
	{
		explicit debug(std::format_string<T...> a_fmt, T&&... a_args, std::source_location a_loc = std::source_location::current())
		{
			REX::Impl::Log(a_loc, REX::ELogLevel::Debug, a_fmt, std::forward<T>(a_args)...);
		}
	};
	template <class... T>
	debug(std::format_string<T...>, T&&...) -> debug<T...>;

	template <class... T>
	struct info
	{
		explicit info(std::format_string<T...> a_fmt, T&&... a_args, std::source_location a_loc = std::source_location::current())
		{
			REX::Impl::Log(a_loc, REX::ELogLevel::Info, a_fmt, std::forward<T>(a_args)...);
		}
	};
	template <class... T>
	info(std::format_string<T...>, T&&...) -> info<T...>;

	template <class... T>
	struct warn
	{
		explicit warn(std::format_string<T...> a_fmt, T&&... a_args, std::source_location a_loc = std::source_location::current())
		{
			REX::Impl::Log(a_loc, REX::ELogLevel::Warning, a_fmt, std::forward<T>(a_args)...);
		}
	};
	template <class... T>
	warn(std::format_string<T...>, T&&...) -> warn<T...>;
}
#else
namespace logger = F4SE::log;
#endif
