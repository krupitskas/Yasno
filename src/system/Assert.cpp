#include "Assert.hpp"

#include <intrin.h>

namespace ysn
{
bool TriggerAssert(const char*, const char*, const char*, size_t)
{
    // if( debug::hasBreakHandler() )
    //{
    //	debug::triggerBreakHandler( "%s(%d):\n%s", pFile, line, pExpr );
    // }
    // else
    //{
    // LogError << std::format("Debug Assert:\nFile: {}\nLine: {}\nMessage: {}\n", pFile, line, pMessage);
    // if (IsDebuggerPresent())
    //{
    //     return true;
    // }
    // else
    //{
    //     //debug::halt( "ASSERT: %s\n", pExpr );
    // }
    // }

    return false;
}

void TriggerBreakpoint()
{
    __debugbreak();
}
} // namespace ysn
