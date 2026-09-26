# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
#
# Hybrid LibreAgent consumption: prefer find_package(CONFIG) when
# LIBRECELIK_USE_INSTALLED_LIBREAGENT=ON, otherwise build the Qt client from
# source via FetchContent at the revision in the LibreAgent row of deps.lock.
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

# The revision -- and the URL -- are the LibreAgent row of deps.lock
# (`<name> <url> <commit> <main|version>`), which `bump-deps` writes and
# `bump-deps check` holds (form, reachable from upstream main, same revision
# as every other consumer, and in CI: the tree actually built == the row).
# This file only reads the row. CMAKE_CONFIGURE_DEPENDS makes a bumped lock
# re-run configure, so the fetched tree follows the lock instead of staying
# at the revision the build directory first fetched.
set(_librecelik_deps_lock "${CMAKE_CURRENT_LIST_DIR}/../deps.lock")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_librecelik_deps_lock}")
file(STRINGS "${_librecelik_deps_lock}" _librecelik_agent_row REGEX "^LibreAgent[ \t]")
list(LENGTH _librecelik_agent_row _librecelik_agent_rows)
if(NOT _librecelik_agent_rows EQUAL 1)
    message(FATAL_ERROR "deps.lock must hold exactly one LibreAgent row")
endif()
string(REGEX REPLACE "[ \t]+" ";" _librecelik_agent_row "${_librecelik_agent_row}")
list(GET _librecelik_agent_row 1 LIBREAGENT_URL)
list(GET _librecelik_agent_row 2 LIBREAGENT_PIN)

if(LIBRECELIK_USE_INSTALLED_LIBREAGENT)
    message(STATUS "LibreAgent: using installed package (CONFIG)")
    find_package(LibreAgent 5.0 REQUIRED CONFIG COMPONENTS ClientQt)
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
        GIT_REPOSITORY ${LIBREAGENT_URL}
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
