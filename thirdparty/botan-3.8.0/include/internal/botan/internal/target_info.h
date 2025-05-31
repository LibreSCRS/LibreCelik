#ifndef BOTAN_TARGET_INFO_H_
#define BOTAN_TARGET_INFO_H_

#include <botan/build.h>

/**
* @file  target_info.h
*
* Automatically generated from
* 'configure.py --disable-deprecated-features --disable-shared-library --cpu=arm64'
*
* Target
*  - Compiler: clang++ -fstack-protector -pthread -stdlib=libc++ -std=c++20 -D_REENTRANT -O3
*  - Arch: arm64
*  - OS: macos
*/

/*
* Configuration
*/
#define BOTAN_CT_VALUE_BARRIER_USE_ASM

[[maybe_unused]] static constexpr bool OptimizeForSize = false;



/*
* Compiler Information
*/
#define BOTAN_BUILD_COMPILER_IS_XCODE

#define BOTAN_COMPILER_INVOCATION_STRING "clang++ -fstack-protector -pthread -stdlib=libc++ -O3"

#define BOTAN_USE_GCC_INLINE_ASM

/*
* External tool settings
*/




/*
* CPU feature information
*/
#define BOTAN_TARGET_ARCH "arm64"

#define BOTAN_TARGET_ARCH_IS_ARM64

#define BOTAN_TARGET_CPU_IS_ARM_FAMILY

#define BOTAN_TARGET_CPU_SUPPORTS_ARMV8CRYPTO
#define BOTAN_TARGET_CPU_SUPPORTS_ARMV8SHA512
#define BOTAN_TARGET_CPU_SUPPORTS_NEON


/*
* Operating system information
*/
#define BOTAN_TARGET_OS_IS_MACOS

#define BOTAN_TARGET_OS_HAS_APPLE_KEYCHAIN
#define BOTAN_TARGET_OS_HAS_ARC4RANDOM
#define BOTAN_TARGET_OS_HAS_ATOMICS
#define BOTAN_TARGET_OS_HAS_CCRANDOM
#define BOTAN_TARGET_OS_HAS_CLOCK_GETTIME
#define BOTAN_TARGET_OS_HAS_COMMONCRYPTO
#define BOTAN_TARGET_OS_HAS_DEV_RANDOM
#define BOTAN_TARGET_OS_HAS_GETENTROPY
#define BOTAN_TARGET_OS_HAS_POSIX1
#define BOTAN_TARGET_OS_HAS_POSIX_MLOCK
#define BOTAN_TARGET_OS_HAS_SANDBOX_PROC
#define BOTAN_TARGET_OS_HAS_SOCKETS
#define BOTAN_TARGET_OS_HAS_SYSCTLBYNAME
#define BOTAN_TARGET_OS_HAS_SYSTEM_CLOCK
#define BOTAN_TARGET_OS_HAS_THREAD_LOCAL


/*
* System paths
*/
#define BOTAN_INSTALL_PREFIX R"(/usr/local)"
#define BOTAN_INSTALL_HEADER_DIR R"(include/botan-3)"
#define BOTAN_INSTALL_LIB_DIR R"(/usr/local/lib)"
#define BOTAN_LIB_LINK "-ldl -framework CoreFoundation -framework Security"
#define BOTAN_LINK_FLAGS "-fstack-protector -pthread -stdlib=libc++"

#define BOTAN_SYSTEM_CERT_BUNDLE "/etc/ssl/cert.pem"

#endif
