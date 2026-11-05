# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)

set(CMAKE_OSX_DEPLOYMENT_TARGET 15.0)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
message("CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

include(GitVersion)

set(PROJECT_VERSION_MAJOR ${GIT_VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${GIT_VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${GIT_VERSION_PATCH})

set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

project(LibreCelik VERSION ${PROJECT_VERSION} LANGUAGES CXX)
# The agent client built from source starts the agent on macOS through AppKit
# (Objective-C++). A language a subproject enables is not visible to the
# generator here, so it is enabled at the top level.
if(APPLE)
    enable_language(OBJCXX)
endif()

# project(VERSION X.Y.Z) accepts only a numeric triple, so PROJECT_VERSION can
# never be more than that. The full string — commit distance, hash, dirty
# marker, pre-release label — is what user-facing banners show, and it is
# DERIVED, never written down: a literal here is a claim that stops being true
# the moment the next commit lands, which is how a development build ends up
# introducing itself as a release. Deliberately a plain variable rather than a
# cache entry, so every re-configure restamps it instead of freezing the value
# captured by the first one.
set(LIBRECELIK_VERSION_FULL "${GIT_VERSION_FULL}")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Promote those warnings to errors. Default OFF so a local build stays
# warning-only; CI turns it on for the configuration whose baseline is known
# clean.
#
# Deliberately NOT an add_compile_options(-Werror) next to the line above:
# that is directory-scoped and would also reach this tree's test executables
# and the third-party CBOR library the agent client fetches and builds here
# (see cmake/FindOrUseLibreAgent.cmake). The agent reads this same option and
# applies it to its own library targets; the opt-in below is per target and is
# applied only to the GUI, its printing library and the GUI plugins. Test
# executables stay warning-only for the same reason the strictness is scoped
# at all: the gate exists to protect the shipped code.
option(LIBRESCRS_WERROR "Treat compiler warnings as errors" OFF)

# Opts the named targets into LIBRESCRS_WERROR. A no-op when the option is
# off, so call sites need no condition of their own.
function(librecelik_warnings_as_errors)
    if(NOT LIBRESCRS_WERROR)
        return()
    endif()
    foreach(_target IN LISTS ARGN)
        target_compile_options(${_target} PRIVATE -Werror)
    endforeach()
endfunction()

# Apple Clang ships <stop_token> behind an experimental flag.
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options(-fexperimental-library)
    add_link_options(-fexperimental-library)
endif()

# The agent client library: built from the deps.lock LibreAgent revision by
# default, an installed package with -DLIBRESCRS_AGENT_FROM_SOURCE=OFF (what the
# distribution recipes do). Provides LibreAgent::ClientQt.
set(LIBRESCRS_AGENT_FROM_SOURCE_DEFAULT ON)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindOrUseLibreAgent.cmake)
librescrs_use_libreagent(COMPONENTS ClientQt)

# Two placement invariants, both load-bearing:
#
# AFTER the dependency include above -- enable_testing() switches testing on
# for this directory and everything added under it, and the LibreAgent subtree
# FetchContent adds is not ours to enrol. Included first, it keeps whatever
# testing decision its own CMakeLists.txt makes.
#
# BEFORE the add_subdirectory() block -- a child inherits the setting as it is
# added, so a child that does not call enable_testing() itself is processed
# with testing off and its add_test() registrations go nowhere. The root
# CTestTestfile.cmake lists every child in subdirs() either way, which is what
# would make that loss quiet. test/ makes its own call, so today's suite
# survives either order; the ordering is what keeps the NEXT subdirectory to
# add a test visible to `ctest --test-dir build`.
# Bound every test. ctest's built-in default is 1500 seconds and nothing in this
# tree overrode it, so a single wedged test could hold a hosted runner for
# twenty-five minutes without ever being named; a handful of them consume the
# whole job. That is not hypothetical — one CI job in this project sat for two
# hours and forty-nine minutes on one deadlocked test, and only cancelling it
# released a log that said which.
#
# include(CTest) rather than a bare enable_testing(): DART_TESTING_TIMEOUT is
# read by the CTest MODULE, which is what writes DartConfiguration.tcl, and a
# bare enable_testing() never writes that file. Measured on a twelve-second
# test with a three-second declared timeout, the bare call reports the suite
# passing after twelve seconds and the module form times the test out after
# three — so declaring the value next to a bare call would have had no effect
# at all, silently. include(CTest) calls enable_testing() itself, so the
# placement of the call below is unchanged. Set BEFORE the include, which would
# otherwise cache 1500.
set(DART_TESTING_TIMEOUT 300 CACHE STRING
    "Default per-test ctest timeout, in seconds")
include(CTest)

add_subdirectory(resources)
add_subdirectory(src)
add_subdirectory(plugins)

# BUILD_TESTING, not unconditional. include(CTest) above defines it and defaults
# it ON, so every developer build is unaffected -- but a distribution package is
# built with it OFF: compiling a test suite nobody will run is not part of
# building a package, and it would make GoogleTest a build dependency of one.
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
