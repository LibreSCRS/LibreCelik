# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets LinguistTools PrintSupport)
find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools PrintSupport)

configure_file (
    "${PROJECT_SOURCE_DIR}/src/config.h.in"
    "${PROJECT_BINARY_DIR}/src/config.h"
)

set(PROJECT_SOURCES
        main.cpp
        librecelik.cpp
        librecelik.h
        librecelik.ui
        utils/libreceliklog.h
        utils/libreceliklog.cpp
        utils/printmanager.h
	utils/utils.h
        utils/iconutils.h
        utils/stringutils.h
        agent/agentgateway.h
        agent/agentgateway.cpp
        agent/cardcontroller.h
        agent/cardcontroller.cpp
        agent/signcontroller.h
        agent/signcontroller.cpp
        agent/errortext.h
        agent/errortext.cpp
        agent/fieldmerge.h
        agent/fieldmerge.cpp
        agent/opstallwatchdog.h
        agent/opstallwatchdog.cpp
        agent/optionalsections.h
        agent/agentstatewidget.h
        agent/agentstatewidget.cpp
        agent/signrequest.h
        agent/signrequest.cpp
        agent/live/liveagentgateway.h
        agent/live/liveagentgateway.cpp
        agent/live/livecardcontroller.h
        agent/live/livecardcontroller.cpp
        agent/live/livesigncontroller.h
        agent/live/livesigncontroller.cpp
        smartcard/qsmartcardmonitor.h
        smartcard/qsmartcardmonitor.cpp
        smartcard/smartcardreaderlistener.h
        smartcard/smartcardreaderlistener.cpp
        document/tokensection.h
        document/tokensection.cpp
        document/rs-eid/changepindlg.h
        document/rs-eid/changepindlg.cpp
        document/emrtd/emrtdauthwidget.h
        document/emrtd/emrtdauthwidget.cpp
        certificate/certfields.h
        certificate/certformat.h
        certificate/certformat.cpp
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
        utils/collapsiblesection.h
        utils/collapsiblesection.cpp
        utils/cardheadercard.h
        utils/cardheadercard.cpp
        utils/localeresolver.h
        utils/localeresolver.cpp
        utils/buttonbox.h
        utils/buttonbox.cpp
        utils/dialogs.h
        utils/dialogs.cpp
        utils/fieldsectionbuilder.h
        utils/fieldsectionbuilder.cpp
        utils/listitemdelegate.h
        utils/listitemdelegate.cpp
        utils/pluginwidgetbase.h
        utils/pluginwidgetbase.cpp
        utils/translations_catalog.cpp
        plugin/cardwidgetplugin.h
        plugin/cardwidgetpluginregistry.h
        plugin/cardwidgetpluginregistry.cpp
        plugin/fieldvalue.h
        asynccardreader.h
        asynccardreader.cpp
        aboutdialog.h
        aboutdialog.cpp
        settings/settingsdialog.h
        settings/settingsdialog.cpp
        settings/tlitemdelegate.h
        settings/tlitemdelegate.cpp
        settings/settingskeys.h
        signing/tsaitemdelegate.h
        signing/tsaitemdelegate.cpp
        signing/defaults.h
)

if(APPLE)
    list(APPEND PROJECT_SOURCES utils/macos_menu.h utils/macos_menu.mm)
endif()

if(LIBRECELIK_SIGNING_ENABLED)
    find_package(Qt6 REQUIRED COMPONENTS PdfWidgets)
    list(APPEND PROJECT_SOURCES
        signing/signingwizard.h
        signing/signingwizard.cpp
        signing/fileselectionpage.h
        signing/fileselectionpage.cpp
        signing/signatureplacementpage.h
        signing/signatureplacementpage.cpp
        signing/signpage.h
        signing/signpage.cpp
        signing/slotlabelformatter.h
        signing/slotlabelformatter.cpp
        signing/pdfpreviewwidget.h
        signing/pdfpreviewwidget.cpp
        signing/filedropzone.h
        signing/filedropzone.cpp
        signing/certutils.h
        signing/certutils.cpp
        signing/tsavalidation.h
        signing/wizardheaderwidget.h
        signing/wizardheaderwidget.cpp
        signing/filelistdelegate.h
        signing/filelistdelegate.cpp
        signing/resultdelegate.h
        signing/resultdelegate.cpp
        signing/signingcolors.h
        signing/signingconfiguration.h
        signing/signingconfiguration.cpp
    )
endif()

# Produce THIRD-PARTY-LICENSES.txt in this directory scope so the rule is
# visible to the LibreCelik target's rcc dependency (cross-directory custom
# command OUTPUTs are not resolvable by the Makefile generator). Inputs were
# resolved in resources/CMakeLists.txt.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_tpn_args --manifest "${THIRD_PARTY_MANIFEST}" -o "${THIRD_PARTY_NOTICE_FILE}")
# Scope the rendered notice to the current platform so the binary ships a
# notice listing only the libraries actually bundled in this artifact (a
# macOS DMG must not list Linux-only deps like Avahi/X11/Kerberos and vice
# versa). On platforms we don't ship to, omit the flag — the generator
# falls back to rendering every component (legacy behavior).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND _tpn_args --platform linux)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND _tpn_args --platform macos)
endif()
set(_tpn_deps "${THIRD_PARTY_NOTICE_GEN}" "${THIRD_PARTY_MANIFEST}")
# Depend on the license texts themselves so editing a body (not just the
# manifest) regenerates the notice. New files still need a reconfigure.
file(GLOB _tpn_texts CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/resources/licenses/*.txt")
list(APPEND _tpn_deps ${_tpn_texts})
if(libremiddleware_SOURCE_DIR AND EXISTS "${libremiddleware_SOURCE_DIR}/thirdparty/licenses.json")
    list(APPEND _tpn_args --lm-manifest "${libremiddleware_SOURCE_DIR}/thirdparty/licenses.json")
    list(APPEND _tpn_deps "${libremiddleware_SOURCE_DIR}/thirdparty/licenses.json")
endif()
add_custom_command(
    OUTPUT "${THIRD_PARTY_NOTICE_FILE}"
    COMMAND ${Python3_EXECUTABLE} "${THIRD_PARTY_NOTICE_GEN}" ${_tpn_args}
    DEPENDS ${_tpn_deps}
    COMMENT "Generating THIRD-PARTY-LICENSES.txt"
    VERBATIM)

set(RESOURCE_SOURCES
        ${RESOURCE_FILE}
        ${THIRD_PARTY_NOTICE_RESOURCE}
        ${THIRD_PARTY_NOTICE_FILE}
        ${TS_PROJECT_FILES}
        ${APP_ICON}
        ${APP_ICON_MACOS}
)

# The generated notice .txt is an add_custom_command OUTPUT, not a real
# source: mark it GENERATED so CMake wires the dependency without trying to
# compile it directly.
set_source_files_properties(${THIRD_PARTY_NOTICE_FILE} PROPERTIES
    GENERATED TRUE HEADER_FILE_ONLY TRUE)

set_source_files_properties(${APP_ICON_MACOS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

add_subdirectory(document/printing)

qt_add_executable(${PROJECT_NAME}
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    ${RESOURCE_SOURCES}
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
# 4.0 LM: trust anchors live inside the LibreSCRS::Trust::TrustStore
# composed and exposed by SigningService; LC no longer embeds raw cert files
# via Qt resources or links OpenSSL directly. All certificate parsing and
# chain validation happens through LibreSCRS::Certificate::ParsedCertificate
# and LibreSCRS::Trust::TrustStore.

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::PrintSupport
    LibreCelikPrinting
    LibreSCRS::Plugin
    LibreSCRS::SmartCard
    LibreSCRS::Trust
    LibreSCRS::Certificate
    LibreAgent::ClientQt)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework AppKit")
endif()

if(LIBRECELIK_SIGNING_ENABLED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::PdfWidgets LibreSCRS::Signing)
endif()

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
