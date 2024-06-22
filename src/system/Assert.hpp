#pragma once

import std;
import system.logger;

namespace ysn
{
bool TriggerAssert(const char* pExpr, const char* pMessage, const char* pFile, size_t line);
void TriggerBreakpoint();
} // namespace ysn

#define YSN_TRIGGER_ASSERT(Expr, Msg) \
    { \
        bool assert_is_triggered = ((Expr) == 0); \
        if (assert_is_triggered) \
        { \
            if (ysn::TriggerAssert(#Expr, Msg, __FILE__, __LINE__)) \
            { \
                ysn::TriggerBreakpoint(); \
            } \
        } \
    }

#ifdef YSN_DEBUG
#define YSN_ASSERT(Expr) YSN_TRIGGER_ASSERT(Expr, #Expr)
#define YSN_ASSERT_MSG(Expr, Msg) YSN_TRIGGER_ASSERT(Expr, Msg)
#define YSN_STATIC_ASSERT(Expr) static_assert(Expr, "Static assertion failed: " #Expr)
#define YSN_STATIC_ASSERT_MSG(Expr, Msg) static_assert(Expr, Msg ": " #Expr)
#else
#define YSN_ASSERT(Expr)
#define YSN_ASSERT_MSG(Expr, Msg)
#define YSN_STATIC_ASSERT(Expr)
#define YSN_STATIC_ASSERT_MSG(Expr, Msg)
#endif
