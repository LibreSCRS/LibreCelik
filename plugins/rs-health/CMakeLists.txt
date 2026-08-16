# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

add_library(rs-health-gui-plugin MODULE
    rshealthwidgetplugin.h
    rshealthwidgetplugin.cpp
    healthwidget.h
    healthwidget.cpp
    healthtextdocument.h
    healthtextdocument.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/collapsiblesection.h
    ${CMAKE_SOURCE_DIR}/src/utils/collapsiblesection.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/cardheadercard.h
    ${CMAKE_SOURCE_DIR}/src/utils/cardheadercard.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/fieldsectionbuilder.h
    ${CMAKE_SOURCE_DIR}/src/utils/fieldsectionbuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/pluginwidgetbase.h
    ${CMAKE_SOURCE_DIR}/src/utils/pluginwidgetbase.cpp
)
target_include_directories(rs-health-gui-plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(rs-health-gui-plugin PRIVATE
    Qt6::Widgets
    LibreAgent::ClientQt
    LibreCelikPrinting
)
set_target_properties(rs-health-gui-plugin PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${LIBRECELIK_GUI_PLUGIN_DIR}"
)
