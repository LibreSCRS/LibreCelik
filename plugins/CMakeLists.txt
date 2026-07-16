# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

# LIBRECELIK_GUI_PLUGIN_DIR is defined in root CMakeLists.txt

# The GUI plugins compile at the same C++23 floor as src/ and test/; set it
# here (the plugins/ scope is a top-level sibling of src/, so the src/ standard
# does not propagate) rather than relying on LibreMiddleware's cxx_std_23
# interface requirement.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(rs-eid)
add_subdirectory(eu-vrc)
add_subdirectory(rs-health)
add_subdirectory(token)
add_subdirectory(emrtd)
add_subdirectory(piv)
