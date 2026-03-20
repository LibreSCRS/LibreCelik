# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(rseid-gui-plugin MODULE
    rseidwidgetplugin.h
    rseidwidgetplugin.cpp
    eidwidget.h
    eidwidget.cpp
    eid.ui
)
target_include_directories(rseid-gui-plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(rseid-gui-plugin PRIVATE
    Qt6::Widgets
    CardPlugin
)
set_target_properties(rseid-gui-plugin PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${LIBRECELIK_GUI_PLUGIN_DIR}"
)
