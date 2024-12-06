module;

#pragma warning( push )
#pragma warning( disable : 4505) // warning C4505: 'AixLog::operator <<': unreferenced function with internal linkage has been removed
#pragma warning( disable : 4100) // warning C4100: 'metadata': unreferenced formal parameter
//#include <aixlog.hpp>
#pragma warning( pop )

//using Severity = AixLog::Severity;

//#define std::cerr LOG(Severity::error)
//#define LogWarning LOG(Severity::warning)
//#define std::cerr LOG(Severity::fatal)
//#define std::cout LOG(Severity::info)
//#define LogTrace LOG(Severity::trace)

export module system.logger;

import std;

export namespace ysn
{

	void InitializeLogger();
}

module :private;

namespace ysn
{
	//struct OutputDebugWindows : public AixLog::SinkFormat
	//{
	//	OutputDebugWindows(const AixLog::Filter& filter, const std::string& format) :
	//		AixLog::SinkFormat(filter, format)
	//	{}

	//	void log(const AixLog::Metadata& metadata, const std::string& message) override
	//	{
	//		std::stringstream ss;
	//		do_log(ss, metadata, message);
	//		OutputDebugStringA(ss.str().c_str());
	//	}
	//};

	void InitializeLogger()
	{
	//	const std::string format = "%H:%M:%S.#ms %d.%m [#severity] ";

	//	std::vector<AixLog::log_sink_ptr> sinks;

	//	auto sink_file = std::make_shared<AixLog::SinkFile>(AixLog::Severity::trace, "yasno.log", format);
	//	sinks.push_back(sink_file);

	//#ifdef YSN_DEBUG
	//	auto sink_debug_output = std::make_shared<OutputDebugWindows>(AixLog::Severity::trace, format);
	//	sinks.push_back(sink_debug_output);
	//#endif

	//	AixLog::Log::init(std::move(sinks));
	}
} // namespace ysn
