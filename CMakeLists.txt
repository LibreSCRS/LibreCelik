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

set(MIDDLEWARE_VERSION 1.3.1)

# Fetch LibreMiddleware (smartcard, eidcard, vehiclecard libraries)
include(FetchContent)
FetchContent_Declare(LibreMiddleware
    GIT_REPOSITORY https://github.com/LibreSCRS/LibreMiddleware.git
    GIT_TAG ${MIDDLEWARE_VERSION}
)
FetchContent_MakeAvailable(LibreMiddleware)

option(LOCAL_LIBREMIDDLEWARE "Use local middleware" 0)
if(DEFINED CACHE{FETCHCONTENT_SOURCE_DIR_LIBREMIDDLEWARE})
    set(LOCAL_LIBREMIDDLEWARE 1)
else()
    # Verify fetched middleware version matches what we requested
    if(NOT "${LibreMiddleware_VERSION}" VERSION_EQUAL "${MIDDLEWARE_VERSION}")
        message(WARNING "LibreMiddleware version mismatch: "
                "requested ${MIDDLEWARE_VERSION}, got ${LibreMiddleware_VERSION}")
    endif()
endif()

# Add LibreMiddleware's cmake modules to our path (for FindPCSC, FindUUID)
list(APPEND CMAKE_MODULE_PATH "${libremiddleware_SOURCE_DIR}/cmake")

add_subdirectory(resources)
add_subdirectory(src)
add_subdirectory(test)
