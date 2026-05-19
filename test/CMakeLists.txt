# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

project(LibreCelikTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Core Widgets Test PrintSupport)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test PrintSupport)
# UUID is used directly by uuid_test.cpp; LC production code has no UUID
# dependency. PCSC is brought in transitively by LM's public SmartCard alias.
find_package(UUID)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(FetchContent)
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(googletest)
enable_testing()

include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR})
get_filename_component(PARENT_DIR ../ ABSOLUTE)
include_directories(${PARENT_DIR})
include_directories(${PARENT_DIR}/src)
include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_BINARY_DIR}/../src")

set(SOURCE_FILES
    ../src/utils/libreceliklog.h
    ../src/utils/libreceliklog.cpp
)

add_executable(
    ${PROJECT_NAME}
    ${SOURCE_FILES}
    hello_test.cpp
    configuration_test.cpp
    uuid_test.cpp
    collapsiblesection_test.cpp
    ../src/utils/collapsiblesection.h
    ../src/utils/collapsiblesection.cpp
    fieldsectionbuilder_test.cpp
    ../src/utils/fieldsectionbuilder.h
    ../src/utils/fieldsectionbuilder.cpp
    securitystatuswidget_test.cpp
    ../src/utils/securitystatuswidget.h
    ../src/utils/securitystatuswidget.cpp
)
target_link_libraries(
    ${PROJECT_NAME}
    GTest::gtest_main
    LibreSCRS::Plugin
    UUID::UUID
    Qt6::Core
    Qt6::Widgets
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_subdirectory(mock_widget_plugin)

add_executable(CardWidgetPluginRegistryTests
    cardwidgetpluginregistry_test.cpp
)
target_include_directories(CardWidgetPluginRegistryTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(CardWidgetPluginRegistryTests
    GTest::gtest_main
    Qt6::Widgets
    LibreSCRS::Plugin
)
target_sources(CardWidgetPluginRegistryTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/plugin/cardwidgetpluginregistry.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.cpp
)
target_compile_definitions(CardWidgetPluginRegistryTests PRIVATE
    MOCK_WIDGET_PLUGIN_DIR="$<TARGET_FILE_DIR:mock-widget-plugin>"
)
set_target_properties(CardWidgetPluginRegistryTests PROPERTIES AUTOMOC ON)
add_dependencies(CardWidgetPluginRegistryTests mock-widget-plugin)
gtest_discover_tests(CardWidgetPluginRegistryTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(AsyncCardReaderTests
    asynccardreader_test.cpp
    ${CMAKE_SOURCE_DIR}/src/asynccardreader.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.cpp
)
target_include_directories(AsyncCardReaderTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(AsyncCardReaderTests
    GTest::gtest_main
    LibreSCRS::Plugin
    LibreSCRS::SmartCard
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)
set_target_properties(AsyncCardReaderTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(AsyncCardReaderTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(QSmartCardMonitorTests
    qsmartcardmonitor_test.cpp
    ../src/smartcard/qsmartcardmonitor.h
    ../src/smartcard/qsmartcardmonitor.cpp
)
set_target_properties(QSmartCardMonitorTests PROPERTIES AUTOMOC ON)
target_link_libraries(QSmartCardMonitorTests PRIVATE
    GTest::gtest_main
    Qt6::Core
    Qt6::Test
    # SmartCard_Impl: required because the test #includes
    # <LibreSCRS/SmartCard/detail/MonitorInjection.h>, an internal-only
    # header #error-guarded behind LIBRESCRS_INTERNAL_BUILD. Mirrors LM's
    # LibreSCRS_MonitorTests setup. Only valid in FetchContent mode.
    SmartCard_Impl
    LibreSCRS::SmartCard
    # The test-only injection helpers (detail::makeMonitorWithProvider,
    # detail::makeSubscriptionIdForTest) deliberately do not ship in the
    # production libLibreSCRS_SmartCard's dynamic export set; their bodies
    # live in this build-tree-only OBJECT archive. Mirrors LM's
    # LibreSCRS_MonitorTests wiring. Only valid in FetchContent mode.
    LibreSCRS_SmartCard_TestHelpers
    Threads::Threads
)
# LIBRESCRS_INTERNAL_BUILD unlocks the detail/MonitorInjection.h
# #error guard. Only valid in FetchContent mode (LM internal headers
# are not installed in CONFIG-package consumption).
target_compile_definitions(QSmartCardMonitorTests PRIVATE LIBRESCRS_INTERNAL_BUILD)
if(DEFINED libremiddleware_SOURCE_DIR)
    target_include_directories(QSmartCardMonitorTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${libremiddleware_SOURCE_DIR}/test
        ${libremiddleware_SOURCE_DIR}/lib/smartcard/src
    )
    gtest_discover_tests(QSmartCardMonitorTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
else()
    message(STATUS "QSmartCardMonitorTests: skipped (requires FetchContent mode for LM internal headers)")
endif()

add_executable(EuVrcTextDocumentTests
    euvrctextdocument_test.cpp
    ../plugins/eu-vrc/euvrctextdocument.h
    ../plugins/eu-vrc/euvrctextdocument.cpp
    ../src/utils/libreceliklog.h
    ../src/utils/libreceliklog.cpp
    ${CMAKE_SOURCE_DIR}/resources/librecelik.qrc
)
target_include_directories(EuVrcTextDocumentTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/document/printing
    ${CMAKE_SOURCE_DIR}/plugins/eu-vrc
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(EuVrcTextDocumentTests
    GTest::gtest
    LibreSCRS::Plugin
    LibreCelikPrinting
    Qt6::Widgets
    Qt6::PrintSupport
)
set_target_properties(EuVrcTextDocumentTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(EuVrcTextDocumentTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(StringUtilsTests
    stringutils_test.cpp
)
target_include_directories(StringUtilsTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(StringUtilsTests
    GTest::gtest
    Qt6::Widgets
)
set_target_properties(StringUtilsTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(StringUtilsTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Pure helper — exercises librecelik::signing::formatSlotLabel which
# composes "<token> — <localised PIN label>" for the multi-PIN PKCS#11
# slot dropdown. No widgets, no signing service; QCoreApplication is
# fixture-installed so qtTrId() returns the catalog-default English
# strings without a translator.
add_executable(SlotLabelFormatterTests
    slotlabelformattertest.cpp
    ${CMAKE_SOURCE_DIR}/src/signing/slotlabelformatter.cpp
)
target_include_directories(SlotLabelFormatterTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(SlotLabelFormatterTests
    GTest::gtest_main
    Qt6::Core
)
target_compile_definitions(SlotLabelFormatterTests PRIVATE
    # Build-tree .qm directory baked in at compile time so the test can
    # load LibreCelik_en.qm without depending on a CTest ENVIRONMENT
    # property (mirrors i18n_settings_state_test).
    LIBRECELIK_TRANSLATIONS_DIR_DEFAULT="${CMAKE_BINARY_DIR}/src")
add_dependencies(SlotLabelFormatterTests LibreCelik_lrelease)
gtest_discover_tests(SlotLabelFormatterTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Pure helper — only Qt::Core needed (QString, QLocale, QStringList).
# No QApplication, no widgets; runs in milliseconds without any UI
# fixture so the locale-fallback contract is exercised on every CI run.
add_executable(LocaleResolverTests
    localeresolvertest.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/localeresolver.cpp
)
target_include_directories(LocaleResolverTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(LocaleResolverTests
    GTest::gtest_main
    Qt6::Core
)
gtest_discover_tests(LocaleResolverTests)

add_executable(HealthTextDocumentTests
    healthtextdocument_test.cpp
    ../plugins/rs-health/healthtextdocument.h
    ../plugins/rs-health/healthtextdocument.cpp
    ../src/utils/libreceliklog.h
    ../src/utils/libreceliklog.cpp
    ${CMAKE_SOURCE_DIR}/resources/librecelik.qrc
)
target_include_directories(HealthTextDocumentTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/document/printing
    ${CMAKE_SOURCE_DIR}/plugins/rs-health
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(HealthTextDocumentTests
    GTest::gtest
    LibreSCRS::Plugin
    LibreCelikPrinting
    Qt6::Widgets
    Qt6::PrintSupport
)
set_target_properties(HealthTextDocumentTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(HealthTextDocumentTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(EIdTextDocumentTests
    eidtextdocument_test.cpp
    ../plugins/rs-eid/eidtextdocument.h
    ../plugins/rs-eid/eidtextdocument.cpp
    ../src/utils/libreceliklog.h
    ../src/utils/libreceliklog.cpp
    ${CMAKE_SOURCE_DIR}/resources/librecelik.qrc
)
target_include_directories(EIdTextDocumentTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/document/printing
    ${CMAKE_SOURCE_DIR}/plugins/rs-eid
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(EIdTextDocumentTests
    GTest::gtest
    LibreSCRS::Plugin
    LibreCelikPrinting
    Qt6::Widgets
    Qt6::PrintSupport
)
set_target_properties(EIdTextDocumentTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(EIdTextDocumentTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(PIVTextDocumentTests
    pivtextdocument_test.cpp
    ../plugins/piv/pivtextdocument.h
    ../plugins/piv/pivtextdocument.cpp
    ../src/utils/libreceliklog.h
    ../src/utils/libreceliklog.cpp
    ${CMAKE_SOURCE_DIR}/resources/librecelik.qrc
)
target_include_directories(PIVTextDocumentTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/document/printing
    ${CMAKE_SOURCE_DIR}/plugins/piv
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(PIVTextDocumentTests
    GTest::gtest
    LibreSCRS::Plugin
    LibreCelikPrinting
    Qt6::Widgets
    Qt6::PrintSupport
)
set_target_properties(PIVTextDocumentTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(PIVTextDocumentTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(EMRTDAuthWidgetTests
    emrtdauthwidget_test.cpp
    ../src/document/emrtd/emrtdauthwidget.h
    ../src/document/emrtd/emrtdauthwidget.cpp
)
target_include_directories(EMRTDAuthWidgetTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(EMRTDAuthWidgetTests
    GTest::gtest
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
    LibreSCRS::Auth
)
set_target_properties(EMRTDAuthWidgetTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(EMRTDAuthWidgetTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

if(LIBRECELIK_SIGNING_ENABLED)
    add_executable(signingconfiguration_test
        signingconfiguration_test.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/signingconfiguration.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.h
        ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.cpp
    )
    target_include_directories(signingconfiguration_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/../src
    )
    target_link_libraries(signingconfiguration_test PRIVATE
        GTest::gtest_main
        Qt6::Core
        LibreSCRS::Signing
    )
    gtest_discover_tests(signingconfiguration_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(signingservice_startup_test
        signingservice_startup_test.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/signingconfiguration.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.h
        ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.cpp
    )
    target_include_directories(signingservice_startup_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/../src
    )
    target_link_libraries(signingservice_startup_test PRIVATE
        GTest::gtest_main
        Qt6::Core
        LibreSCRS::Signing
    )
    gtest_discover_tests(signingservice_startup_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(LIBRECELIK_SIGNING_ENABLED)
    add_executable(SigningUnitTests
        signing_unit_test.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/certutils.cpp
    )
    target_include_directories(SigningUnitTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/../src
    )
    target_link_libraries(SigningUnitTests PRIVATE
        GTest::gtest
        Qt6::Core
        LibreSCRS::Certificate
        LibreSCRS::Signing
    )
    gtest_discover_tests(SigningUnitTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# --- i18n retranslate-audit support library + self-test (rc2) ---
#
# RetranslatableWidgetFixture: gtest base fixture used by D7 (settings
# state) and D9 (plugin retranslate) test suites. The self-test
# (i18n_test_support_self_test) verifies the fixture's snapshot walker,
# translator install/remove lifecycle, and assertAllRetranslated rule
# set in isolation, before any production widget depends on it.
#
# Header-only support files (locale_stable_words.h, mock_plugin_data.h)
# do not need their own translation unit; they are pulled in by D7/D9
# tests as needed.
add_executable(LibreCelikI18nFixtureSelfTest
    i18n_test_support_self_test.cpp
    i18n_test_support/RetranslatableWidgetFixture.cpp
)
target_include_directories(LibreCelikI18nFixtureSelfTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(LibreCelikI18nFixtureSelfTest PRIVATE
    GTest::gtest_main
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)
set_target_properties(LibreCelikI18nFixtureSelfTest PROPERTIES AUTOMOC ON)
gtest_discover_tests(LibreCelikI18nFixtureSelfTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# --- D7 runtime test: SettingsDialog preference-state coverage ---
#
# Compiles the live SettingsDialog implementation directly into the
# test binary (rather than depending on the LibreCelik executable) so
# the test exercises the production retranslate path with no Qt
# plugin loading. Mirrors the SigningWizardE2ETests pattern.
add_executable(i18n_settings_state_test
    i18n_settings_state_test.cpp
    i18n_test_support/RetranslatableWidgetFixture.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/settingsdialog.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/tlitemdelegate.cpp
    ${CMAKE_SOURCE_DIR}/src/signing/tsaitemdelegate.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/listitemdelegate.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/localeresolver.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/buttonbox.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/dialogs.cpp
)
target_include_directories(i18n_settings_state_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(i18n_settings_state_test PRIVATE
    GTest::gtest_main
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)
set_target_properties(i18n_settings_state_test PROPERTIES AUTOMOC ON)
target_compile_definitions(i18n_settings_state_test PRIVATE
    # Build-tree .qm directory baked in at compile time so the fixture can
    # find LibreCelik_*.qm without depending on a CTest ENVIRONMENT property
    # that CMake's gtest_discover_tests fails to forward correctly when more
    # than one var is listed (the var list is expanded as separate args).
    LIBRECELIK_TRANSLATIONS_DIR_DEFAULT="${CMAKE_BINARY_DIR}/src")
add_dependencies(i18n_settings_state_test LibreCelik_lrelease)
gtest_discover_tests(i18n_settings_state_test PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen")

# --- D9 runtime test: plugin card-widget retranslate coverage ---
#
# Compiles all 5 production plugin widgets into a single test binary
# and parameterises over them. Each plugin is built with a synthetic
# CardData (mock_plugin_data.h), snapshotted, language-switched, and
# re-snapshotted; the fixture asserts every shared key changed.
add_executable(i18n_plugin_retranslate_test
    i18n_plugin_retranslate_test.cpp
    i18n_test_support/RetranslatableWidgetFixture.cpp
    # Plugin widgets — compiled directly so we test their retranslate
    # paths without spinning up Qt plugin loading.
    ${CMAKE_SOURCE_DIR}/plugins/rs-eid/eidwidget.cpp
    ${CMAKE_SOURCE_DIR}/plugins/rs-health/healthwidget.cpp
    ${CMAKE_SOURCE_DIR}/plugins/emrtd/emrtdwidget.cpp
    ${CMAKE_SOURCE_DIR}/plugins/eu-vrc/euvrcwidget.cpp
    ${CMAKE_SOURCE_DIR}/plugins/piv/pivwidget.cpp
    # Shared widget utilities used by every plugin.
    ${CMAKE_SOURCE_DIR}/src/utils/collapsiblesection.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/cardheadercard.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/fieldsectionbuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/pluginwidgetbase.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/securitystatuswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.cpp
)
target_include_directories(i18n_plugin_retranslate_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(i18n_plugin_retranslate_test PRIVATE
    GTest::gtest_main
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
    LibreSCRS::Plugin
)
set_target_properties(i18n_plugin_retranslate_test PROPERTIES AUTOMOC ON)
target_compile_definitions(i18n_plugin_retranslate_test PRIVATE
    LIBRECELIK_TRANSLATIONS_DIR_DEFAULT="${CMAKE_BINARY_DIR}/src")
add_dependencies(i18n_plugin_retranslate_test LibreCelik_lrelease)
gtest_discover_tests(i18n_plugin_retranslate_test PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen")

if(LIBRECELIK_SIGNING_ENABLED)
    find_package(Qt6 REQUIRED COMPONENTS PdfWidgets)

    # Paint-capture-based integration test for PdfPreviewWidget. Verifies
    # that the preview widget renders the visual-signature appearance in
    # agreement with LibreSCRS::Signing::layoutVisualSignature for the same
    # (text, box) inputs. No friend classes, no test-only accessors — uses
    # a custom QPaintEngine to record every drawText / clip / font state
    # change during the widget's paint pass. Spec reference:
    # knowledge/specs/2026-05-08-vis-sig-fillbox-design.md (G3 amendment).
    add_executable(PdfPreviewWidgetTests
        pdfpreviewwidget_test.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/pdfpreviewwidget.cpp
    )
    target_include_directories(PdfPreviewWidgetTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/../src
    )
    target_link_libraries(PdfPreviewWidgetTests PRIVATE
        GTest::gtest
        Qt6::Widgets
        Qt6::PdfWidgets
        Qt6::Gui
        LibreSCRS::Signing
    )
    set_target_properties(PdfPreviewWidgetTests PROPERTIES AUTOMOC ON)
    gtest_discover_tests(PdfPreviewWidgetTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(SigningWizardE2ETests
        signingwizard_e2e_test.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/signingwizard.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/fileselectionpage.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/signatureplacementpage.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/signpage.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/filedropzone.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/pdfpreviewwidget.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/certutils.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/wizardheaderwidget.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/filelistdelegate.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/resultdelegate.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/buttonbox.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/dialogs.cpp
    )
    target_include_directories(SigningWizardE2ETests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/../src
    )
    target_link_libraries(SigningWizardE2ETests PRIVATE
        GTest::gtest_main
        Qt6::Widgets
        Qt6::Test
        Qt6::PdfWidgets
        LibreSCRS::Plugin
        LibreSCRS::SmartCard
        LibreSCRS::Signing
        LibreSCRS::Certificate
    )
    # libremiddleware_SOURCE_DIR is set in FetchContent mode; in installed-LM
    # (CONFIG) mode the DSS jar discovery falls back to a runtime-configured
    # path. Tests that require DSS will skip when the jar is unreachable.
    if(NOT DEFINED libremiddleware_SOURCE_DIR)
        set(_dss_jar_dir "")
    else()
        set(_dss_jar_dir "${libremiddleware_SOURCE_DIR}/tools/dss-service/target")
    endif()
    target_compile_definitions(SigningWizardE2ETests PRIVATE
        MIDDLEWARE_PLUGIN_DIR="$<TARGET_FILE_DIR:pkcs15-plugin>"
        PKCS11_MODULE_PATH="$<TARGET_FILE:librescrs-pkcs11>"
        DSS_JAR_DIR="${_dss_jar_dir}"
        # qt_add_translations binds the :/i18n/ resource only to the main
        # LibreCelik target, so the test binary loads the .qm from the
        # build dir directly. The path tracks where qt_add_translations
        # writes the lrelease output (currently ${CMAKE_BINARY_DIR}/src/).
        LIBRECELIK_QM_DIR="${CMAKE_BINARY_DIR}/src"
        LIBRESCRS_TESTING
    )
    set_target_properties(SigningWizardE2ETests PROPERTIES AUTOMOC ON)
    add_dependencies(SigningWizardE2ETests pkcs15-plugin librescrs-pkcs11 LibreCelik_lrelease)
    gtest_discover_tests(SigningWizardE2ETests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
