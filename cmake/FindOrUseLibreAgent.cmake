# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
#
# Hybrid LibreAgent consumption: prefer find_package(CONFIG) when
# LIBRECELIK_USE_INSTALLED_LIBREAGENT=ON, otherwise build the Qt client from
# source via FetchContent at the revision recorded in cmake/libreagent.pin.
# Either path provides the namespaced LibreAgent::ClientQt target the GUI links.
#
# The pin is a fixed 40-hex revision, never a branch: the GUI's behaviour is
# proven against exactly that revision, and a moving branch would let the
# fetched client run ahead of what was proven. Raising it is a deliberate act.
#
# Dev builds re-point FetchContent at a local sibling checkout with
#   -DFETCHCONTENT_SOURCE_DIR_LIBREAGENT=/path/to/LibreAgent
# (the source tree is consumed in place; the fetched project's tests and
# install/export rules stay behind PROJECT_IS_TOP_LEVEL, so only the enabled
# component libraries build here). A green build against a source override
# proves the sources, NOT the pin -- only a fetch of the pinned revision does.
#
# LibreAgent's own cmake_minimum_required() is 3.28, so this file's default
# FetchContent path needs CMake >= 3.28 even though this project's floor stays
# 3.24 -- a limitation of the fetched project, not of this file. The installed
# find_package path is unaffected.

option(LIBRECELIK_USE_INSTALLED_LIBREAGENT
       "Consume an installed LibreAgent (find_package) instead of FetchContent" OFF)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libreagent.pin" LIBREAGENT_PIN)
string(STRIP "${LIBREAGENT_PIN}" LIBREAGENT_PIN)
# Exactly 40 lowercase hex characters. Spelled as a length test plus a
# character-class test because CMake's regex engine has no {n} repetition
# operator -- "^[0-9a-f]{40}$" would silently never match.
string(LENGTH "${LIBREAGENT_PIN}" LIBREAGENT_PIN_LENGTH)
if(NOT LIBREAGENT_PIN_LENGTH EQUAL 40 OR NOT LIBREAGENT_PIN MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "cmake/libreagent.pin must hold one 40-hex commit SHA")
endif()

if(LIBRECELIK_USE_INSTALLED_LIBREAGENT)
    message(STATUS "LibreAgent: using installed package (CONFIG)")
    find_package(LibreAgent 4.2 REQUIRED CONFIG COMPONENTS ClientQt)
else()
    message(STATUS "LibreAgent: building from source (FetchContent, pin ${LIBREAGENT_PIN})")
    include(FetchContent)
    # ClientQt + Wire only; Core stays OFF so this repo never pulls LM
    # through the agent (LibreAgent Core hard-requires LM). The abbreviation
    # is deliberate -- this file must not spell the middleware's full name.
    # ClientQt defaults OFF and Core defaults ON upstream, so all three
    # switches are pre-seeded explicitly (option() only creates a cache entry
    # that does not already exist, so these win over the fetched defaults).
    set(LIBREAGENT_BUILD_CLIENT_QT ON  CACHE BOOL "" FORCE)
    set(LIBREAGENT_BUILD_WIRE      ON  CACHE BOOL "" FORCE)
    set(LIBREAGENT_BUILD_CORE      OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(LibreAgent
        GIT_REPOSITORY https://github.com/LibreSCRS/LibreAgent.git
        GIT_TAG ${LIBREAGENT_PIN})
    # CMAKE_INCLUDE_CURRENT_DIR is ON for this project (Qt convention) and it is a
# *variable*, so it leaks into the fetched project and puts LibreAgent's own
# source root on the include path of LibreAgent's own targets. That root holds a
# file named VERSION, and on a case-insensitive filesystem `#include <version>`
# resolves to it instead of the standard header -- so the macOS build fails to
# parse libc++'s own includes while Linux, being case-sensitive, never notices.
# Off for the duration of the fetch only; the GUI's own targets still get it.
set(_librecelik_saved_include_current_dir ${CMAKE_INCLUDE_CURRENT_DIR})
set(CMAKE_INCLUDE_CURRENT_DIR OFF)
FetchContent_MakeAvailable(LibreAgent) # provides LibreAgent::ClientQt
set(CMAKE_INCLUDE_CURRENT_DIR ${_librecelik_saved_include_current_dir})
unset(_librecelik_saved_include_current_dir)
endif()
