# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

add_library(emrtd-gui-plugin MODULE
    emrtdwidgetplugin.h
    emrtdwidgetplugin.cpp
    emrtdwidget.h
    emrtdwidget.cpp
    emrtdtextdocument.h
    emrtdtextdocument.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/collapsiblesection.h
    ${CMAKE_SOURCE_DIR}/src/utils/collapsiblesection.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/cardheadercard.h
    ${CMAKE_SOURCE_DIR}/src/utils/cardheadercard.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/fieldsectionbuilder.h
    ${CMAKE_SOURCE_DIR}/src/utils/fieldsectionbuilder.cpp
)
target_include_directories(emrtd-gui-plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(emrtd-gui-plugin PRIVATE
    Qt6::Widgets
    CardPlugin
    LibreCelikPrinting
)
set_target_properties(emrtd-gui-plugin PROPERTIES
    AUTOMOC ON
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${LIBRECELIK_GUI_PLUGIN_DIR}"
)
