#include "Logger.hpp"

#include <sstream>

namespace ysn
{
	struct OutputDebugWindows : public AixLog::SinkFormat
	{
		OutputDebugWindows(const AixLog::Filter& filter, const std::string& format) :
			AixLog::SinkFormat(filter, format)
		{}

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

	#ifdef YSN_DEBUG
		auto sink_debug_output = std::make_shared<OutputDebugWindows>(AixLog::Severity::trace, format);
		sinks.push_back(sink_debug_output);
	#endif

		AixLog::Log::init(std::move(sinks));
	}
} // namespace ysn
