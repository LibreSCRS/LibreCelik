# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(pks-gui-plugin MODULE
    pkswidgetplugin.h
    pkswidgetplugin.cpp
    pkswidget.h
    pkswidget.cpp
)
target_include_directories(pks-gui-plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(pks-gui-plugin PRIVATE
    Qt6::Widgets
    CardPlugin
)
set_target_properties(pks-gui-plugin PROPERTIES
    AUTOMOC ON
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${LIBRECELIK_GUI_PLUGIN_DIR}"
)
