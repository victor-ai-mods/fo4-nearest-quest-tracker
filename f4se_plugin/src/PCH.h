#pragma once

#include "F4SE/F4SE.h"
#include "RE/Fallout.h"

#include <charconv>
#include <unordered_set>

#define DLLEXPORT __declspec(dllexport)

using namespace std::literals;

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
