#if !defined(CREUSE_UTILITY_H)
#define CREUSE_UTILITY_H

#define CREUSE_ARRAYCOUNT(array) (sizeof(array) / sizeof((array)[0]))

#define CREUSE_KIB(count) (((count) * 1024ULL))
#define CREUSE_MIB(count) (((count) * 1024ULL * 1024ULL))
#define CREUSE_GIB(count) (((count) * 1024ULL * 1024ULL * 1024ULL))

#endif
