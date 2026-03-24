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

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(MIDDLEWARE_VERSION main)

# Fetch LibreMiddleware (smartcard, eidcard, vehiclecard libraries)
include(FetchContent)
FetchContent_Declare(LibreMiddleware
    GIT_REPOSITORY https://github.com/LibreSCRS/LibreMiddleware.git
    GIT_TAG ${MIDDLEWARE_VERSION}
)
FetchContent_MakeAvailable(LibreMiddleware)

# Read the actual version from LibreMiddleware's own git tags.
# When using a local checkout (FETCHCONTENT_SOURCE_DIR_LIBREMIDDLEWARE),
# append "-local" so the UI clearly shows it's a development build.
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
    WORKING_DIRECTORY ${libremiddleware_SOURCE_DIR}
    OUTPUT_VARIABLE MW_GIT_VERSION
    RESULT_VARIABLE MW_GIT_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(MW_GIT_RESULT EQUAL 0 AND MW_GIT_VERSION)
    set(MIDDLEWARE_VERSION ${MW_GIT_VERSION})
endif()
if(DEFINED FETCHCONTENT_SOURCE_DIR_LIBREMIDDLEWARE)
    set(MIDDLEWARE_VERSION "${MIDDLEWARE_VERSION}-local")
endif()

# Add LibreMiddleware's cmake modules to our path (for FindPCSC, FindUUID)
list(APPEND CMAKE_MODULE_PATH "${libremiddleware_SOURCE_DIR}/cmake")

set(LIBRECELIK_GUI_PLUGIN_DIR "${CMAKE_BINARY_DIR}/gui-plugins" CACHE PATH
    "Directory where GUI plugin .so files are placed")

add_subdirectory(resources)
add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(test)
