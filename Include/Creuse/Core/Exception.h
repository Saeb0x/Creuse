#if !defined(CREUSE_EXCEPTION_H)
#define CREUSE_EXCEPTION_H

#include "Assert.h"

#if !CREUSE_EXCEPTIONS_ENABLED
    #include <cstdlib>
#endif

#if CREUSE_EXCEPTIONS_ENABLED
    #define CREUSE_THROW(exception) throw exception
#else
    #define CREUSE_THROW(exception) (CREUSE_FAIL_MSG(#exception), std::abort())
#endif

#endif
