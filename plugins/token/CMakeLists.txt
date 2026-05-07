# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(token-gui-plugin MODULE
    tokenwidgetplugin.h
    tokenwidgetplugin.cpp
)
target_include_directories(token-gui-plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(token-gui-plugin PRIVATE
    Qt6::Widgets
    LibreSCRS::Plugin
)
set_target_properties(token-gui-plugin PROPERTIES
    AUTOMOC ON
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${LIBRECELIK_GUI_PLUGIN_DIR}"
)
