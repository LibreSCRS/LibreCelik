# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(vehicle-gui-plugin MODULE
    vehiclewidgetplugin.h
    vehiclewidgetplugin.cpp
    vehiclewidget.h
    vehiclewidget.cpp
    vehicle.ui
    ${CMAKE_SOURCE_DIR}/src/utils/collapsiblesection.h
    ${CMAKE_SOURCE_DIR}/src/utils/collapsiblesection.cpp
)
target_include_directories(vehicle-gui-plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(vehicle-gui-plugin PRIVATE
    Qt6::Widgets
    CardPlugin
)
set_target_properties(vehicle-gui-plugin PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${LIBRECELIK_GUI_PLUGIN_DIR}"
)
