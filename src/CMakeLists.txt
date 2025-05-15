# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets LingistTools PrintSupport)
find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools PrintSupport)
find_package(PCSC)
find_package(UUID)

configure_file (
    "${PROJECT_SOURCE_DIR}/src/config.h.in"
    "${PROJECT_BINARY_DIR}/src/config.h"
)

include_directories(${PROJECT_NAME} PUBLIC ${PCSC_INCLUDE_DIR})
include_directories(${PROJECT_NAME} PUBLIC ${UUID_INCLUDE_DIR})

add_definitions(-DCERT_STORE_PATH="${PROJECT_SOURCE_DIR}/thirdparty/certificates/all")

set(PROJECT_SOURCES
        main.cpp
        librecelik.cpp
        librecelik.h
        librecelik.ui
        utils/libreceliklog.h
        utils/libreceliklog.cpp
        utils/printmanager.h
	utils/utils.h
	smartcard/smartcardwrapper.h
        smartcard/smartcardscanner.h
        smartcard/smartcardscanner.cpp
        smartcard/smartcardevent.h
        smartcard/smartcardreaderlistener.h
        smartcard/smartcardreaderlistener.cpp
        celikapi/celikapiplus.h
        celikapi/celikapiwrapper.h
        celikapi/celikapiwrapper.cpp
        celikapi/celikapisession.h
        celikapi/celikapisession.cpp
        celikapi/celikapisessionfactory.h
        celikapi/celikapisessionfactory.cpp
        document/document.h
        document/document.cpp
        document/eid/eidreader.h
        document/eid/eidreader.cpp
        document/eid/eidtextdocument.cpp
        document/eid/eidtextdocument.h
        document/eid/eid.h
        document/eid/eid.cpp
        document/eid/eid.ui
)

set(RESOURSE_SOURCES
        ${RESOURSE_FILE}
        ${TS_PROJECT_FILES}
        ${APP_ICON}
        ${APP_ICON_MACOS}
)

set_source_files_properties(${APP_ICON_MACOS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

qt_add_executable(${PROJECT_NAME}
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    ${RESOURSE_SOURCES}
)

qt_add_translations(${PROJECT_NAME}
    TS_FILES ${TS_PROJECT_FILES}
    # LUPDATE_OPTIONS -locations none -no-obsolete
    # LRELEASE_OPTIONS -idbased
)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_lupdate)
add_dependencies(${PROJECT_NAME} update_translations)
add_dependencies(${PROJECT_NAME}  ${PROJECT_NAME}_lrelease)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::PrintSupport PCSC::PCSC UUID::UUID BOTAN CelikAPI)

set_target_properties(${PROJECT_NAME} PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER rs.libresc.librecelik
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()
