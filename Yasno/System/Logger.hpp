#pragma once

#pragma warning( push )
#pragma warning( disable : 4505) // warning C4505: 'AixLog::operator <<': unreferenced function with internal linkage has been removed
#pragma warning( disable : 4100) // warning C4100: 'metadata': unreferenced formal parameter
#include <aixlog.hpp>
#pragma warning( pop )

using Severity = AixLog::Severity;

#define LogError LOG(Severity::error)
#define LogWarning LOG(Severity::warning)
#define LogFatal LOG(Severity::fatal)
#define LogInfo LOG(Severity::info)
#define LogTrace LOG(Severity::trace)

namespace ysn
{
	void InitializeLogger();
}
