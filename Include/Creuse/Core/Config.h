#if !defined(CREUSE_CONFIG_H)
#define CREUSE_CONFIG_H

#define CREUSE_PLATFORM_WINDOWS 0
#define CREUSE_PLATFORM_LINUX 0
#define CREUSE_PLATFORM_APPLE 0

#if defined(_WIN32) || defined(_WIN64)
    #undef CREUSE_PLATFORM_WINDOWS
    #define CREUSE_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #undef CREUSE_PLATFORM_LINUX
    #define CREUSE_PLATFORM_LINUX 1
#elif defined(__APPLE__)
    #undef CREUSE_PLATFORM_APPLE
    #define CREUSE_PLATFORM_APPLE 1
#endif

#if defined(CREUSE_DEBUG) && !defined(CREUSE_RELEASE)
    #define CREUSE_RELEASE (!(CREUSE_DEBUG))
#elif defined(CREUSE_RELEASE) && !defined(CREUSE_DEBUG)
    #define CREUSE_DEBUG (!(CREUSE_RELEASE))
#elif !defined(CREUSE_DEBUG) && !defined(CREUSE_RELEASE)
    #if defined(_DEBUG) || defined(DEBUG)
        #define CREUSE_DEBUG 1
        #define CREUSE_RELEASE 0
    #else
        #define CREUSE_DEBUG 0
        #define CREUSE_RELEASE 1
    #endif
#endif

#if (CREUSE_DEBUG + CREUSE_RELEASE) != 1
    #error "Creuse: Exactly one of CREUSE_DEBUG / CREUSE_RELEASE must be 1."
#endif

#if !defined(CREUSE_EXCEPTIONS_ENABLED)
    #if defined(_CPPUNWIND) || defined(__EXCEPTIONS) || defined(__cpp_exceptions)
        #define CREUSE_EXCEPTIONS_ENABLED 1
    #else
        #define CREUSE_EXCEPTIONS_ENABLED 0
    #endif
#endif

#endif
