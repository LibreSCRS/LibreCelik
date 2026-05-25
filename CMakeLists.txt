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
set(LIBRECELIK_VERSION_FULL "4.1.0" CACHE STRING "Full version string including pre-release suffix")
add_compile_definitions(LIBRECELIK_VERSION_FULL="${LIBRECELIK_VERSION_FULL}")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Apple Clang ships <stop_token> behind an experimental flag.
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options(-fexperimental-library)
    add_link_options(-fexperimental-library)
endif()

# TEMPORARY (4.2 cross-platform testing phase): track LibreMiddleware `main`
# so Linux/macOS testers build against the latest LM without a local-source
# override. MUST revert to a real tag (`4.2.0`) before tagging LibreCelik
# 4.2.0 — releases require reproducible tag-pinned dependencies.
set(MIDDLEWARE_VERSION main)

# Hybrid LM consumption: FetchContent (default) or find_package(CONFIG).
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindOrUseLibreMiddleware.cmake)

if(NOT LIBRECELIK_USE_INSTALLED_LM)
    # Fetch LibreMiddleware (smartcard, rs-eid, rs-health, eu-vrc, emrtd, pkcs15 libraries)
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
    if(FETCHCONTENT_SOURCE_DIR_LIBREMIDDLEWARE)
        set(MIDDLEWARE_VERSION "${MIDDLEWARE_VERSION}-local")
    endif()

    # Add LibreMiddleware's cmake modules to our path (for FindPCSC, FindUUID)
    list(APPEND CMAKE_MODULE_PATH "${libremiddleware_SOURCE_DIR}/cmake")
else()
    # CONFIG-package mode: take version from the package, and rely on the
    # installed LibreMiddlewareConfig.cmake to have re-exposed FindPCSC etc.
    set(MIDDLEWARE_VERSION ${LibreMiddleware_VERSION})
endif()

# Digital signing UI — requires LibreMiddleware built with BUILD_SIGNING=ON.
# Can be independently disabled even if LM has signing support.
option(ENABLE_SIGNING "Enable digital signing UI (requires LM signing support)" ON)
if(ENABLE_SIGNING AND NOT LIBREMIDDLEWARE_HAS_SIGNING)
    message(STATUS "Signing UI disabled: LibreMiddleware was built without BUILD_SIGNING")
    set(ENABLE_SIGNING OFF)
endif()
set(LIBRECELIK_SIGNING_ENABLED ${ENABLE_SIGNING})

set(LIBRECELIK_GUI_PLUGIN_DIR "${CMAKE_BINARY_DIR}/gui-plugins" CACHE PATH
    "Directory where GUI plugin .so files are placed")

add_subdirectory(resources)
add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(test)
