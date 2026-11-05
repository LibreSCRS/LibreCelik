# SPDX-License-Identifier: LGPL-2.1-or-later
# FindOrUseLibreAgent -- the one way a LibreSCRS repository consumes LibreAgent.
#
# A consumer carries a byte-identical copy of this file in its cmake/ (the
# check-version gate compares the two) and writes, before including it, the
# default of the one switch that picks where the agent comes from:
#
#   set(LIBRESCRS_AGENT_FROM_SOURCE_DEFAULT ON)   # or OFF
#   include(cmake/FindOrUseLibreAgent.cmake)
#   librescrs_use_libreagent(COMPONENTS Core Wire)
#
# LIBRESCRS_AGENT_FROM_SOURCE (a cache option; -D overrides the default)
#   ON   build the agent from the revision the LibreAgent row of the
#        repository's deps.lock names (FetchContent).
#   OFF  find_package(LibreAgent <this major>.0 CONFIG) -- an installed agent.
#
# One name and one polarity for every repository. Four copies used to carry
# four names, one of them with the opposite sense (a FETCH switch defaulting
# OFF beside three USE_INSTALLED switches defaulting OFF), so the same -D
# meant opposite things one directory apart.
#
# COMPONENTS names what the repository links. On the installed path they are
# the find_package components: without them the lookup probes every component,
# and on a machine that also has the Qt client installed that runs its
# find_dependency(Qt6) for a dependency the repository never asked for. On the
# source path each named component, and what it needs (the socket client needs
# Pkcs11Facade, the Qt client's doubles need the Qt client), is switched on and
# every other one off (Wire is always built, so it has no switch),
# BEFORE the subproject is configured: option() does not overwrite a cache
# variable that already exists, so a switch seeded afterwards does nothing.
#
# The revision and the URL are the LibreAgent row of deps.lock
# (`<name> <url> <commit>`), which `bump-deps` writes and `bump-deps check`
# holds; never a branch, or two builds of one tag would compile different code.
# The lock is a configure dependency, so a bumped row re-runs configure.
# Dev builds point FetchContent at a local checkout with
# -DFETCHCONTENT_SOURCE_DIR_LIBREAGENT=<path>; a green build against a source
# override proves the sources, not the pin.
#
# The floor on the installed path is this repository's own major (the
# repositories release in lockstep): it says only "an agent from another major
# cannot satisfy this build"; the package's SameMajorVersion rule says the rest.

if(NOT DEFINED LIBRESCRS_AGENT_FROM_SOURCE_DEFAULT)
    message(FATAL_ERROR "Set LIBRESCRS_AGENT_FROM_SOURCE_DEFAULT (ON or OFF) before including "
                        "FindOrUseLibreAgent.cmake: the repository decides where its agent comes from.")
endif()
option(LIBRESCRS_AGENT_FROM_SOURCE
       "Build LibreAgent from the deps.lock revision (FetchContent) instead of find_package(CONFIG)"
       ${LIBRESCRS_AGENT_FROM_SOURCE_DEFAULT})

function(librescrs_use_libreagent)
    cmake_parse_arguments(PARSE_ARGV 0 _lua "" "" "COMPONENTS")
    if(_lua_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "librescrs_use_libreagent: unknown arguments: ${_lua_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT _lua_COMPONENTS)
        message(FATAL_ERROR "librescrs_use_libreagent: name the COMPONENTS this repository links")
    endif()
    set(_lua_known Core Wire ClientQt ClientQtTestSupport Pkcs11Facade Pkcs11SocketClient TestSupport)
    foreach(_lua_c IN LISTS _lua_COMPONENTS)
        if(NOT _lua_c IN_LIST _lua_known)
            message(FATAL_ERROR "librescrs_use_libreagent: unknown component ${_lua_c} (known: ${_lua_known})")
        endif()
    endforeach()
    if(NOT PROJECT_VERSION_MAJOR)
        message(FATAL_ERROR "librescrs_use_libreagent: call it after project(), whose major is the floor")
    endif()

    if(NOT LIBRESCRS_AGENT_FROM_SOURCE)
        find_package(LibreAgent ${PROJECT_VERSION_MAJOR}.0 REQUIRED CONFIG COMPONENTS ${_lua_COMPONENTS})
        message(STATUS "LibreAgent: installed package (CONFIG), components ${_lua_COMPONENTS}")
        return()
    endif()

    set(_lua_lock "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../deps.lock")
    if(NOT EXISTS "${_lua_lock}")
        message(FATAL_ERROR "LIBRESCRS_AGENT_FROM_SOURCE is ON but there is no ${_lua_lock} to take the revision from")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_lua_lock}")
    file(STRINGS "${_lua_lock}" _lua_row REGEX "^LibreAgent[ \t]")
    list(LENGTH _lua_row _lua_rows)
    if(NOT _lua_rows EQUAL 1)
        message(FATAL_ERROR "${_lua_lock} must hold exactly one LibreAgent row (it holds ${_lua_rows})")
    endif()
    string(REGEX REPLACE "[ \t]+" ";" _lua_row "${_lua_row}")
    list(LENGTH _lua_row _lua_fields)
    if(NOT _lua_fields EQUAL 3)
        message(FATAL_ERROR "${_lua_lock}: the LibreAgent row is not `<name> <url> <commit>`")
    endif()
    list(GET _lua_row 1 _lua_url)
    list(GET _lua_row 2 _lua_pin)
    if(NOT _lua_pin MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "${_lua_lock}: the LibreAgent revision \"${_lua_pin}\" is not a commit")
    endif()

    # What each named component needs on the source path.
    set(_lua_on ${_lua_COMPONENTS})
    if("ClientQtTestSupport" IN_LIST _lua_on)
        list(APPEND _lua_on ClientQt)
    endif()
    if("Pkcs11SocketClient" IN_LIST _lua_on)
        list(APPEND _lua_on Pkcs11Facade)
    endif()
    foreach(_lua_pair IN ITEMS Core:CORE ClientQt:CLIENT_QT ClientQtTestSupport:CLIENT_QT_TEST_SUPPORT
                               Pkcs11Facade:PKCS11_FACADE Pkcs11SocketClient:PKCS11_SOCKET_CLIENT
                               TestSupport:TEST_SUPPORT)
        string(REPLACE ":" ";" _lua_pair "${_lua_pair}")
        list(GET _lua_pair 0 _lua_name)
        list(GET _lua_pair 1 _lua_switch)
        if(_lua_name IN_LIST _lua_on)
            set(LIBREAGENT_BUILD_${_lua_switch} ON CACHE BOOL "" FORCE)
        else()
            set(LIBREAGENT_BUILD_${_lua_switch} OFF CACHE BOOL "" FORCE)
        endif()
    endforeach()

    message(STATUS "LibreAgent: building from source (FetchContent, pin ${_lua_pin}), components ${_lua_COMPONENTS}")
    include(FetchContent)
    FetchContent_Declare(LibreAgent GIT_REPOSITORY ${_lua_url} GIT_TAG ${_lua_pin})
    # The agent's own headers are found through its targets; a consumer that
    # turned on CMAKE_INCLUDE_CURRENT_DIR must not hand it this directory's.
    # Set here, it reaches the subproject and ends with this function.
    set(CMAKE_INCLUDE_CURRENT_DIR OFF)
    FetchContent_MakeAvailable(LibreAgent)
endfunction()
