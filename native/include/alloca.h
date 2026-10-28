#ifdef _WIN32
#define alloca __builtin_alloca
#else
#include_next <alloca.h>
#endif
