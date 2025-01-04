module;

#pragma warning(push)
#pragma warning(disable : 4505) // warning C4505: 'AixLog::operator <<': unreferenced function with internal linkage has been removed
#pragma warning(disable : 4100) // warning C4100: 'metadata': unreferenced formal parameter
#include <aixlog.hpp>
#pragma warning(pop)

export module system.logger;

import std;
import system.compilation;

namespace logger
{
	struct Info
	{
		Info& operator<<(std::string_view msg);
	};

	struct Warning
	{
		Warning& operator<<(std::string_view msg);
	};

	struct Error
	{
		Error& operator<<(std::string_view msg);
	};

	struct Debug
	{
		Debug& operator<<(std::string_view msg);
	};

	struct Fatal
	{
		Fatal& operator<<(std::string_view msg);
	};
}; // namespace logger

export namespace ysn
{
	logger::Info LogInfo;
	logger::Warning LogWarning;
	logger::Error LogError;
	logger::Debug LogDebug;
	logger::Fatal LogFatal;

	void InitializeLogger();
}

module :private;

namespace logger
{
	Info& Info::operator<<(std::string_view msg)
	{
		LOG(AixLog::Severity::info) << msg;
		return *this;
	};

	Warning& Warning::operator<<(std::string_view msg)
	{
		LOG(AixLog::Severity::warning) << msg;
		return *this;
	};

	Error& Error::operator<<(std::string_view msg)
	{
		LOG(AixLog::Severity::error) << msg;
		return *this;
	};

	Debug& Debug::operator<<(std::string_view msg)
	{
		LOG(AixLog::Severity::debug) << msg;
		return *this;
	};

	Fatal& Fatal::operator<<(std::string_view msg)
	{
		LOG(AixLog::Severity::fatal) << msg;
		return *this;
	};
} // namespace logger

namespace ysn
{
	struct OutputDebugWindows : public AixLog::SinkFormat
	{
		OutputDebugWindows(const AixLog::Filter& filter, const std::string& format) : AixLog::SinkFormat(filter, format)
		{
		}

		void log(const AixLog::Metadata& metadata, const std::string& message) override
		{
			std::stringstream ss;
			do_log(ss, metadata, message);
			OutputDebugStringA(ss.str().c_str());
		}
	};

	void InitializeLogger()
	{
		const std::string format = "%H:%M:%S.#ms %d.%m [#severity] ";

		std::vector<AixLog::log_sink_ptr> sinks;

		auto sink_file = std::make_shared<AixLog::SinkFile>(AixLog::Severity::trace, "yasno.log", format);
		sinks.push_back(sink_file);

		if constexpr (!IsReleaseActive())
		{
			auto sink_debug_output = std::make_shared<OutputDebugWindows>(AixLog::Severity::trace, format);
			sinks.push_back(sink_debug_output);
		}

		AixLog::Log::init(std::move(sinks));
	}
}
