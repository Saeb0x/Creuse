#if !defined(CREUSE_ASSERT_H)
#define CREUSE_ASSERT_H

#include "Config.h"
#include "Types.h"

#if !defined(CREUSE_DEFAULT_ASSERT_HANDLER)
    #define CREUSE_DEFAULT_ASSERT_HANDLER 1
#endif

#if CREUSE_DEFAULT_ASSERT_HANDLER
    #include <cstdio>
    #include <cstdlib>
#endif

#if CREUSE_PLATFORM_WINDOWS && CREUSE_DEFAULT_ASSERT_HANDLER
    extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent(void);
    extern "C" __declspec(dllimport) void __stdcall DebugBreak(void);
#endif

using AssertHandler = void(*)(const char* expression, const char* file, int32 line, const char* message);

#if CREUSE_DEFAULT_ASSERT_HANDLER
inline void DefaultAssertHandler(const char* expression, const char* file, int32 line, const char* message)
{
    std::fprintf(stderr, "Creuse: Assertion failed:\n    Expression: %s.\n    At: %s:%d.\n", expression, file, line);

    if(message != nullptr)
    {
        std::fprintf(stderr, "    Message: %s.\n", message);
    }

#if CREUSE_PLATFORM_WINDOWS
    if(IsDebuggerPresent())
    {
        DebugBreak();
    }
#endif

    std::abort();
}

inline AssertHandler g_AssertHandler = &DefaultAssertHandler;
#else
inline AssertHandler g_AssertHandler = nullptr;
#endif

#if CREUSE_DEBUG
#define CREUSE_ASSERT_MSG(expression, message) \
    do \
    { \
        if (!(expression)) \
        { \
            g_AssertHandler(#expression, __FILE__, __LINE__, (message)); \
        } \
    } while (0)
#else
    #define CREUSE_ASSERT_MSG(expression, message) ((void)sizeof(!(expression)), (void)sizeof(message))
#endif

#define CREUSE_ASSERT(expression) CREUSE_ASSERT_MSG(expression, nullptr)
#define CREUSE_ASSERT_STATIC_MSG(expression, message) static_assert(expression, message)
#define CREUSE_ASSERT_STATIC(expression) static_assert(expression)

// NOTE(saeb): Unconditional failure.
#define CREUSE_FAIL_MSG(message) (g_AssertHandler("CREUSE_FAIL", __FILE__, __LINE__, (message)))

#endif
