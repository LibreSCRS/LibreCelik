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
# PCSC is found by LibreMiddleware; UUID is used directly by the app
find_package(UUID)

# Glob cert files now (configure time) so configure_file can reference the path.
# The actual qt_add_resources call is below, after qt_add_executable.
file(GLOB LIBRECELIK_CERT_FILES
    "${libremiddleware_SOURCE_DIR}/thirdparty/certificates/all/*.cer"
    "${libremiddleware_SOURCE_DIR}/thirdparty/certificates/all/*.crt"
)
set(LIBRECELIK_CERTIFICATES_DIR ":/certificates")

configure_file (
    "${PROJECT_SOURCE_DIR}/src/config.h.in"
    "${PROJECT_BINARY_DIR}/src/config.h"
)

set(PROJECT_SOURCES
        main.cpp
        librecelik.cpp
        librecelik.h
        librecelik.ui
        libreceliktypes.h
        utils/libreceliklog.h
        utils/libreceliklog.cpp
        utils/printmanager.h
	utils/utils.h
	smartcard/smartcardwrapper.h
        smartcard/ipcscscanprovider.h
        smartcard/pcscscanprovider.h
        smartcard/smartcardscanner.h
        smartcard/smartcardscanner.cpp
        smartcard/smartcardevent.h
        smartcard/smartcardreaderlistener.h
        smartcard/smartcardreaderlistener.cpp
        document/document.h
        document/document.cpp
        document/textdocument.h
        document/textdocument.cpp
        document/tokensection.h
        document/tokensection.cpp
        document/eid/eidreader.h
        document/eid/eidreader.cpp
        document/eid/eidtextdocument.cpp
        document/eid/eidtextdocument.h
        document/eid/eid.h
        document/eid/eid.cpp
        document/eid/eid.ui
        document/eid/changepindlg.h
        document/eid/changepindlg.cpp
        certificate/certificateinfoitem.h
        certificate/certificateinfoitem.cpp
        certificate/certificatetreeviewmodel.h
        certificate/certificatetreeviewmodel.cpp
        certificate/certificatepropertiesmodel.h
        certificate/certificatepropertiesmodel.cpp
        certificate/certificatehierarchymodel.h
        certificate/certificatehierarchymodel.cpp
        certificate/certificateviewerwidget.h
        certificate/certificateviewerwidget.cpp
        certificate/certificateviewerwidget.ui
        certificate/certificateviewerdlg.h
        certificate/certificateviewerdlg.cpp
        document/vehicle/vehiclereader.h
        document/vehicle/vehiclereader.cpp
        document/vehicle/vehicletextdocument.h
        document/vehicle/vehicletextdocument.cpp
        document/vehicle/vehicle.h
        document/vehicle/vehicle.cpp
        document/vehicle/vehicle.ui
        document/health/healthreader.h
        document/health/healthreader.cpp
        document/health/healthtextdocument.h
        document/health/healthtextdocument.cpp
        document/health/health.h
        document/health/health.cpp
        document/health/health.ui
        document/pks/pksreader.h
        document/pks/pksreader.cpp
        document/pks/pks.h
        document/pks/pks.cpp
        utils/collapsiblesection.h
        utils/collapsiblesection.cpp
        utils/translations_catalog.cpp
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
    LUPDATE_OPTIONS -locations none
)
# qt_add_translations automatically:
#   - Creates the LibreCelik_lrelease target (compiles .ts → .qm in build dir)
#   - Creates a Qt resource that embeds the .qm files at :/i18n/<locale>.qm
#   - Adds the lrelease target as a build dependency of LibreCelik
# Do NOT add a separate qt_add_resources for /i18n — it would create a duplicate
# resource that races with lrelease on a clean CI build, embedding empty data and
# overriding the correct qt_add_translations resource (root cause of broken
# AppImage translations in 1.7.1 and 1.7.2).
# To update translations: edit .ts files, then run LibreCelik_lupdate to sync
# source strings, and LibreCelik_lrelease to recompile (or just cmake --build).
# Embed Serbian CA certificates into the executable via Qt resources so they are
# accessible at runtime in AppImage / DMG builds (where the LibreMiddleware
# source tree is not present).  :/certificates/<name>.cer mirrors the layout
# previously expected from LIBRECELIK_CERTIFICATES_DIR.
qt_add_resources(${PROJECT_NAME} "certificates_res"
    PREFIX "/certificates"
    BASE "${libremiddleware_SOURCE_DIR}/thirdparty/certificates/all"
    FILES ${LIBRECELIK_CERT_FILES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::PrintSupport EIdCard VehicleCard HealthCard PKSCard PCSC::PCSC UUID::UUID)

set_target_properties(${PROJECT_NAME} PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER rs.librescrs.librecelik
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
