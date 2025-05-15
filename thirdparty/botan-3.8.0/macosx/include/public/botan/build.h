#if defined(__x86_64__)
    #include "build_x86_64.h"
#elif defined(__aarch64__)
    #include "build_arm64.h"
#else
    #error Unsupported architecture for botan
#endif
