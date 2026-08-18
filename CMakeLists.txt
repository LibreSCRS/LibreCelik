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

# Apple Clang ships <stop_token> behind an experimental flag.
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options(-fexperimental-library)
    add_link_options(-fexperimental-library)
endif()

# Hybrid LibreAgent consumption: FetchContent at cmake/libreagent.pin
# (default) or find_package(CONFIG). Provides LibreAgent::ClientQt.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindOrUseLibreAgent.cmake)

# Digital signing UI. Nothing in this build produces signatures in-process
# any more, so the switch is a pure user-interface choice with no dependency
# to satisfy.
option(ENABLE_SIGNING "Enable digital signing UI" ON)
set(LIBRECELIK_SIGNING_ENABLED ${ENABLE_SIGNING})

set(LIBRECELIK_GUI_PLUGIN_DIR "${CMAKE_BINARY_DIR}/gui-plugins" CACHE PATH
    "Directory where GUI plugin .so files are placed")

add_subdirectory(resources)
add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(test)
