#pragma once

#include "F4SE/F4SE.h"
#include "RE/Fallout.h"

#include <charconv>
#include <unordered_set>

#define DLLEXPORT __declspec(dllexport)

using namespace std::literals;

namespace NQT
{
	// Address Library id of an engine function this plugin calls itself. NG (1.10.984) and AE (1.11.x) share the
	// id (the AE databases keep the NG ids of matching functions), so one number serves both: the OG + NG DLL picks
	// by runtime, the AE DLL takes it. AE ids were matched in the 1.11.191 disassembly (tools/fo4_ae.py), the NG
	// code at the same id compared with AE (tools/fo4_ng.py cmp), see the comment at each use.
	[[nodiscard]] constexpr auto GameID([[maybe_unused]] std::uint64_t a_og, std::uint64_t a_ngae) noexcept
	{
#ifdef NQT_AE
		return REL::ID(a_ngae);
#else
		return REL::RelocationID(a_og, a_ngae);
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
