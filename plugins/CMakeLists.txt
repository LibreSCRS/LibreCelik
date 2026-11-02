# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

# The card pages are Qt plugins linked into the application, not modules
# loaded from a directory at run time. Every one of them is first-party, built
# and shipped with the application, against an interface that is not
# installed: nothing outside this tree can write one, so loading them from a
# directory bought only its costs -- a search path, an install directory, a
# copy of the shared widget code in each module, and a window with no card
# pages and no error when the path was wrong. Qt's static plugins keep the
# same interface and the same registry; linking a plugin target imports it.

# The GUI plugins compile at the same C++23 floor as src/ and test/; set it
# here (the plugins/ scope is a top-level sibling of src/, so the src/ standard
# does not propagate).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

# One card page: a static Qt plugin over the shared widget library, under the
# project's warnings-as-errors setting, exporting nothing.
function(librecelik_add_gui_plugin name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "CLASS_NAME" "SOURCES")
    qt_add_plugin(${name} STATIC CLASS_NAME ${ARG_CLASS_NAME})
    target_sources(${name} PRIVATE ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE
        Qt6::Widgets
        LibreAgent::ClientQt
        LibreCelikPrinting
        LibreCelikWidgets)
    set_target_properties(${name} PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    librecelik_warnings_as_errors(${name})
endfunction()

add_subdirectory(rs-eid)
add_subdirectory(eu-vrc)
add_subdirectory(rs-health)
add_subdirectory(token)
add_subdirectory(emrtd)
