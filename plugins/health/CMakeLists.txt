# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(health-gui-plugin MODULE
    healthwidgetplugin.h
    healthwidgetplugin.cpp
    healthwidget.h
    healthwidget.cpp
    health.ui
)
target_include_directories(health-gui-plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(health-gui-plugin PRIVATE
    Qt6::Widgets
    CardPlugin
)
set_target_properties(health-gui-plugin PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${LIBRECELIK_GUI_PLUGIN_DIR}"
)
