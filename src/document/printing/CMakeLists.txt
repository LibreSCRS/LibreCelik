# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

add_library(LibreCelikPrinting STATIC
    textdocument.h
    textdocument.cpp
    ${PROJECT_SOURCE_DIR}/src/utils/libreceliklog.h
    ${PROJECT_SOURCE_DIR}/src/utils/libreceliklog.cpp
)
target_include_directories(LibreCelikPrinting
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src
)
target_link_libraries(LibreCelikPrinting PUBLIC Qt6::Widgets Qt6::PrintSupport)
set_target_properties(LibreCelikPrinting PROPERTIES
    AUTOMOC ON
    POSITION_INDEPENDENT_CODE ON
)
