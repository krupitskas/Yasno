export module system.asserts;

import std;
import system.logger;
import system.compilation;

// bool TriggerAssert(const char* pExpr, const char* pMessage, const char* pFile, size_t line);
void TriggerBreakpoint()
{
    __debugbreak();
}

export namespace ysn
{
// Aborts the program if the user-specified condition is not true
void AssertMsg(bool condition, std::string_view msg, std::source_location loc = std::source_location::current())
{
    if constexpr (IsDebugActive())
    {
        if (!condition)
        {
            const std::string message = msg.empty() ? "Message not provided!" : msg.data();
            LogError << "ASSERT in function " << loc.function_name() << "Message: " << message << "\n"
                     << "Location: " << std::to_string(loc.line()) << " " << loc.file_name() << " " << "\n";

            TriggerBreakpoint();
        }
    }
}

} // namespace ysn

module :private;