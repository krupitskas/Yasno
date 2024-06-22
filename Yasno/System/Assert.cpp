#include "Assert.hpp"

#include <format>

#include <intrin.h>

namespace ysn
{
    bool TriggerAssert(const char* , const char* pMessage, const char* pFile, size_t line)
    {
        //if( debug::hasBreakHandler() )
        //{
        //	debug::triggerBreakHandler( "%s(%d):\n%s", pFile, line, pExpr );
        //}
        //else
        //{
        LogFatal << std::format("Debug Assert:\nFile: {}\nLine: {}\nMessage: {}\n", pFile, line, pMessage);
        if (IsDebuggerPresent())
        {
            return true;
        }
        else
        {
            //debug::halt( "ASSERT: %s\n", pExpr );
        }
        //}

        return false;
    }

    void TriggerBreakpoint()
    {
        __debugbreak();
    }
}
