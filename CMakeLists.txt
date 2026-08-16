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

# CMake's project(VERSION X.Y.Z) strips pre-release suffixes; keep the full
# version (with -rcN / -beta / -local) here so user-facing banners can show it.
set(LIBRECELIK_VERSION_FULL "4.2.0" CACHE STRING "Full version string including pre-release suffix")
add_compile_definitions(LIBRECELIK_VERSION_FULL="${LIBRECELIK_VERSION_FULL}")

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

# The signing wizard's end-to-end suite compiles every wizard translation unit
# directly, and the whole wizard -- preview, page trio, delegates -- is re-typed
# onto the agent one page at a time. The suite's own build description is
# re-pointed once, after the last page has landed, rather than edited alongside
# each page for no review value. The switch is OFF for exactly that window, and
# the suite is absent from the test listing while it is.
option(LIBRECELIK_WIZARD_E2E_READY
    "Build the signing wizard end-to-end suite against the re-typed wizard" OFF)

set(LIBRECELIK_GUI_PLUGIN_DIR "${CMAKE_BINARY_DIR}/gui-plugins" CACHE PATH
    "Directory where GUI plugin .so files are placed")

add_subdirectory(resources)
add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(test)
