# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

# LIBRECELIK_GUI_PLUGIN_DIR is defined in root CMakeLists.txt

# The GUI plugins compile at the same C++23 floor as src/ and test/; set it
# here (the plugins/ scope is a top-level sibling of src/, so the src/ standard
# does not propagate).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(rs-eid)
add_subdirectory(eu-vrc)
add_subdirectory(rs-health)
add_subdirectory(token)
add_subdirectory(emrtd)
add_subdirectory(piv)

# Every GUI plugin is first-party code, so all of them follow the project's
# warnings-as-errors setting from this one place rather than repeating the
# call in six sibling files.
librecelik_warnings_as_errors(
    rs-eid-gui-plugin
    eu-vrc-gui-plugin
    rs-health-gui-plugin
    token-gui-plugin
    emrtd-gui-plugin
    piv-gui-plugin
)

# The application resolves gui-plugins/ next to its own lib dir at runtime;
# an installed tree must actually put the plugins there, or a prefix install
# starts with no card pages at all. One rule for all six first-party plugins,
# in the same spot that already treats them as a set.
include(GNUInstallDirs)
if(UNIX AND NOT APPLE)
    install(TARGETS
        rs-eid-gui-plugin
        eu-vrc-gui-plugin
        rs-health-gui-plugin
        token-gui-plugin
        emrtd-gui-plugin
        piv-gui-plugin
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/gui-plugins)
endif()
