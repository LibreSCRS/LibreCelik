# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

project(LibreCelikTests)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Core Widgets Test PrintSupport)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test PrintSupport)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(FetchContent)
# googletest's own install rules default ON and would leak libgtest*.a and
# include/gtest/ into the production prefix; it is a test dependency, not a
# shipped artefact.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(googletest)

# enable_testing() is safe to call again here even though the root
# CMakeLists.txt already calls it before add_subdirectory(test). Keeping this
# call is what makes `ctest --test-dir build/test` work on its own, so that
# path stays alive no matter how the root file is later reordered.
enable_testing()

# The repository ROOT is deliberately not on the include path. It holds a file
# named VERSION, and on a case-insensitive filesystem `#include <version>`
# resolves to that instead of the standard header -- which fails the macOS build
# inside libc++'s own includes while Linux, being case-sensitive, never notices.
# Targets that reach into plugins/ name it through the plugins directory below.
get_filename_component(PARENT_DIR ../ ABSOLUTE)
include_directories(${PARENT_DIR}/src)
include_directories(${PARENT_DIR}/plugins)
include_directories("${PROJECT_BINARY_DIR}/../src")

set(SOURCE_FILES
    ../src/utils/libreceliklog.h
    ../src/utils/libreceliklog.cpp
)

add_executable(
    ${PROJECT_NAME}
    ${SOURCE_FILES}
    hello_test.cpp
    configuration_test.cpp
    collapsiblesection_test.cpp
    ../src/utils/collapsiblesection.h
    ../src/utils/collapsiblesection.cpp
    fieldsectionbuilder_test.cpp
    ../src/utils/fieldsectionbuilder.h
    ../src/utils/fieldsectionbuilder.cpp
    securitystatuswidget_test.cpp
    ../src/utils/securitystatuswidget.h
    ../src/utils/securitystatuswidget.cpp
    spinnerpage_test.cpp
    ../src/utils/spinnerpage.h
    ../src/utils/spinnerpage.cpp
    ../src/utils/messageline.h
    ../src/utils/messageline.cpp
)
target_link_libraries(
    ${PROJECT_NAME}
    GTest::gtest_main
    Qt6::Core
    Qt6::Widgets
    # fieldsectionbuilder.cpp renders the client's own flattened identity rows,
    # so this catch-all target both compiles against the client headers and
    # links the exported flatten symbol. The link carries the include path too.
    LibreAgent::ClientQt
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
    # cardwidgetplugin.h now speaks the client's field-group types.
    LibreAgent::ClientQt
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

# The field-group successor of the middleware card-data helpers, which left
# with the middleware read path: same shape, same case list, the client's types.
add_executable(FieldValueTests
    fieldvalue_test.cpp
)
target_link_libraries(FieldValueTests PRIVATE
    GTest::gtest_main
    Qt6::Core
    LibreAgent::ClientQt
)
target_include_directories(FieldValueTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
gtest_discover_tests(FieldValueTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Compiles a plugin source directly — but the eu-vrc family already speaks the
# field-group plugin interface, so this suite is back in the build ahead of the
# plugins/ tree, which waits for the families still to be re-typed.
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
    LibreAgent::ClientQt
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

# Compiles a plugin source directly — the rs-health family speaks the
# field-group plugin interface, so this suite guards that formatter's re-type.
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
    LibreAgent::ClientQt
    LibreCelikPrinting
    Qt6::Widgets
    Qt6::PrintSupport
)
set_target_properties(HealthTextDocumentTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(HealthTextDocumentTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Compiles a plugin source directly — the rs-eid family speaks the field-group
# plugin interface, so this suite guards that formatter's re-type.
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
    LibreAgent::ClientQt
    LibreCelikPrinting
    Qt6::Widgets
    Qt6::PrintSupport
)
set_target_properties(EIdTextDocumentTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(EIdTextDocumentTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Compiles a plugin source directly — the piv family speaks the field-group
# plugin interface, so this suite guards that formatter's re-type.
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
    LibreAgent::ClientQt
    LibreCelikPrinting
    Qt6::Widgets
    Qt6::PrintSupport
)
set_target_properties(PIVTextDocumentTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(PIVTextDocumentTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

if(LIBRECELIK_SIGNING_ENABLED)
    # certformat.cpp joins this target for the KeyUsage-bitmask cases; it is
    # LM-free from the certificate-viewer re-type onward, so it adds no
    # middleware dependency. The target itself goes LM-free with the signing
    # wizard's re-type: nothing it compiles parses DER in-process any more.
    # The file-selection page joins this target (with the drop zone and the
    # list delegate it owns) so the routing and selection-cap cases exercise
    # the production widget instead of a test-local copy of its tables.
    add_executable(SigningUnitTests
        signing_unit_test.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/fileselectionpage.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/filedropzone.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/filelistdelegate.cpp
        ${CMAKE_SOURCE_DIR}/src/certificate/certformat.cpp
    )
    target_include_directories(SigningUnitTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/../src
    )
    target_link_libraries(SigningUnitTests PRIVATE
        GTest::gtest
        Qt6::Core
        Qt6::Widgets
        # The page speaks the client's signature-format vocabulary and spells
        # the batch bound from the client header.
        LibreAgent::ClientQt
    )
    # FileSelectionPage, FileDropZone and FileListDelegate are QObjects.
    set_target_properties(SigningUnitTests PROPERTIES AUTOMOC ON)
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

# --- Settings dialog over the agent's Config1 (offscreen GUI) ---
#
# The same production translation units i18n_settings_state_test compiles —
# the dialog now speaks Config1, so the two suites share a source list — plus
# the scripted fake gateway that answers its reads and records its writes.
#
# The abstract vtable anchors (agentgateway/cardcontroller/signcontroller .cpp)
# ride along for the same reason every other fake-driven target needs them: the
# fakes derive from those classes and the target cannot link without them.
#
# NO LibreSCRS::* link: every translation unit here speaks the client's own
# vocabulary. A middleware link appearing in this target would mean a settings
# TU started reading card configuration in-process again.
add_executable(SettingsConfig1Tests
    settings_config1_test.cpp
    fake_gateway/fakeagentgateway.cpp
    fake_gateway/fakecardcontroller.cpp
    fake_gateway/fakesigncontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/agentgateway.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/cardcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/signcontroller.cpp
    # The dialog prefills its trust-tier controls through buildConfig1Import,
    # so the import TU is part of what this suite exercises.
    ${CMAKE_SOURCE_DIR}/src/agent/settingsimport.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/settingsdialog.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/tlitemdelegate.cpp
    ${CMAKE_SOURCE_DIR}/src/signing/tsaitemdelegate.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/listitemdelegate.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/localeresolver.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/buttonbox.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/dialogs.cpp
)
target_include_directories(SettingsConfig1Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(SettingsConfig1Tests PRIVATE
    GTest::gtest_main
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
    LibreAgent::ClientQt
)
set_target_properties(SettingsConfig1Tests PROPERTIES AUTOMOC ON)
gtest_discover_tests(SettingsConfig1Tests PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen")

# --- D7 runtime test: SettingsDialog preference-state coverage ---
#
# Compiles the live SettingsDialog implementation directly into the
# test binary (rather than depending on the LibreCelik executable) so
# the test exercises the production retranslate path with no Qt
# plugin loading. Mirrors the SigningWizardE2ETests pattern.
#
# The fakes and the abstract vtable anchors are here because the dialog now
# takes a gateway: the suite constructs it against the scripted one, and the
# target cannot link a fake whose base classes' anchors are absent.
add_executable(i18n_settings_state_test
    i18n_settings_state_test.cpp
    i18n_test_support/RetranslatableWidgetFixture.cpp
    fake_gateway/fakeagentgateway.cpp
    fake_gateway/fakecardcontroller.cpp
    fake_gateway/fakesigncontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/agentgateway.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/cardcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/signcontroller.cpp
    # Same reason as SettingsConfig1Tests: settingsdialog.cpp's trust-tier
    # prefill calls buildConfig1Import.
    ${CMAKE_SOURCE_DIR}/src/agent/settingsimport.cpp
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
    LibreAgent::ClientQt
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

# --- EidWidget full-model rendering coverage ---
#
# The full-model ctor is what a card page is (re)built from at identityReady,
# and the model's group order is delivery-dependent (stream order vs the wire
# map's keyed order on a recovered read). These cases pin that every section
# renders regardless of arrival order.
add_executable(eidwidget_test
    eidwidget_test.cpp
    ${CMAKE_SOURCE_DIR}/plugins/rs-eid/eidwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/collapsiblesection.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/cardheadercard.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/fieldsectionbuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/pluginwidgetbase.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.cpp
)
target_include_directories(eidwidget_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins
)
target_link_libraries(eidwidget_test PRIVATE
    GTest::gtest_main
    Qt6::Core
    Qt6::Widgets
    LibreAgent::ClientQt
)
set_target_properties(eidwidget_test PROPERTIES AUTOMOC ON)
gtest_discover_tests(eidwidget_test PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen")

# --- EMRTDWidget annex coverage ---
#
# The annex sections and, above all, that the annex's own verdict renders
# INSIDE the section it describes. Two verdicts on one screen (the travel
# document's passive authentication and the annex's weaker one) read as a
# single guarantee when they share a flat list. securitystatuswidget.cpp is
# linked here and NOT in eidwidget_test: only this widget builds the pane.
add_executable(emrtdwidget_annex_test
    emrtdwidget_annex_test.cpp
    ${CMAKE_SOURCE_DIR}/plugins/emrtd/emrtdwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/collapsiblesection.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/cardheadercard.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/fieldsectionbuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/pluginwidgetbase.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/securitystatuswidget.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.cpp
)
target_include_directories(emrtdwidget_annex_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins
)
target_link_libraries(emrtdwidget_annex_test PRIVATE
    GTest::gtest_main
    Qt6::Core
    Qt6::Widgets
    LibreAgent::ClientQt
)
set_target_properties(emrtdwidget_annex_test PROPERTIES AUTOMOC ON)
# The section-title cases compare RENDERED titles, so they need the real
# catalog: with no translator, qtTrId() hands back the bare id and two sections
# that collide in Serbian look distinct to the test.
target_compile_definitions(emrtdwidget_annex_test PRIVATE
    LIBRECELIK_TRANSLATIONS_DIR_DEFAULT="${CMAKE_BINARY_DIR}/src")
add_dependencies(emrtdwidget_annex_test LibreCelik_lrelease)
gtest_discover_tests(emrtdwidget_annex_test PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen")

# --- D9 runtime test: plugin card-widget retranslate coverage ---
#
# Compiles all 5 production plugin widgets into a single test binary
# and parameterises over them. Each plugin is built with a synthetic
# field-group read (mock_plugin_data.h), snapshotted, language-switched, and
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
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(i18n_plugin_retranslate_test PRIVATE
    GTest::gtest_main
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
    LibreAgent::ClientQt
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
    # that the preview widget renders exactly the visual-signature layout its
    # layout provider hands it — in production the agent's own, here a
    # scripted stand-in. No friend classes, no test-only accessors — uses
    # a custom QPaintEngine to record every drawText / clip / font state
    # change during the widget's paint pass. Verifies the visible-signature
    # fill-box layout.
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
        # pdfpreviewwidget.h speaks the client's own layout result, and the
        # widget asks the agent rather than the middleware for its layout.
        LibreAgent::ClientQt
    )
    set_target_properties(PdfPreviewWidgetTests PROPERTIES AUTOMOC ON)
    gtest_discover_tests(PdfPreviewWidgetTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # --- Live-agent wizard suite (hardware bench only) ---
    #
    # The whole wizard driven against a REAL agent and a REAL card: the live
    # gateway, the live controllers it mints, and the agent's own prompter
    # collecting every credential. It builds and is discovered everywhere and
    # SKIPS everywhere the bench is not set up, so it costs CI nothing but a
    # link — and it is the hardware gate's suite, not a CI suite.
    #
    # NO LibreSCRS::* link and no plugin/module dependencies: nothing in this
    # target talks to a card in-process any more. What it compiles beyond the
    # wizard is exactly the src/agent/ layer the live path needs — the abstract
    # seams' vtable anchors, the live gateway plus the two controllers it mints,
    # and the pure helpers those controllers and the sign page share.
    #
    # The suite's verification oracle is its own artifact-shape assertions —
    # no external tool is invoked (the retired LIBRESCRS_DSS_JAR gate demanded
    # a jar nothing here ever ran).
    add_executable(SigningWizardE2ETests
        signingwizard_e2e_test.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/agentgateway.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/cardcontroller.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/signcontroller.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/errortext.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/fieldmerge.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/opstallwatchdog.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/signrequest.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/live/liveagentgateway.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/live/livecardcontroller.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/live/livesigncontroller.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/signingwizard.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/fileselectionpage.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/signatureplacementpage.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/signpage.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/filedropzone.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/pdfpreviewwidget.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/wizardheaderwidget.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/filelistdelegate.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/resultdelegate.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/buttonbox.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/dialogs.cpp
    )
    # CMAKE_CURRENT_SOURCE_DIR: the suite is asserted through the SHARED
    # emission-contract helpers in fake_gateway/, the same ones the scripted
    # fakes are asserted through.
    target_include_directories(SigningWizardE2ETests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/../src
    )
    target_link_libraries(SigningWizardE2ETests PRIVATE
        GTest::gtest_main
        Qt6::Widgets
        Qt6::Test
        Qt6::PdfWidgets
        LibreAgent::ClientQt
    )
    target_compile_definitions(SigningWizardE2ETests PRIVATE
        # qt_add_translations binds the :/i18n/ resource only to the main
        # LibreCelik target, so the test binary loads the .qm from the
        # build dir directly. The path tracks where qt_add_translations
        # writes the lrelease output (currently ${CMAKE_BINARY_DIR}/src/).
        LIBRECELIK_QM_DIR="${CMAKE_BINARY_DIR}/src"
        LIBRESCRS_TESTING
    )
    set_target_properties(SigningWizardE2ETests PROPERTIES AUTOMOC ON)
    add_dependencies(SigningWizardE2ETests LibreCelik_lrelease)
    gtest_discover_tests(SigningWizardE2ETests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # --- Fake-driven wizard behavior suite (offscreen GUI) ---
    #
    # The whole wizard, every page it stacks, and NOTHING that dials: the
    # gateway and the sign controller are the campaign's scripted fakes, so the
    # cases assert what LC does with an agent's answers rather than whether an
    # agent answered. The abstract vtable anchors
    # (agentgateway/cardcontroller/signcontroller .cpp) ride along for the same
    # reason the other fake-driven targets need them — the fakes derive from
    # those classes and the target cannot link without them.
    #
    # signrequest.cpp is here because the sign page counts the ceremonies a
    # selection really costs through the PRODUCTION partitioner, and
    # errortext.cpp because the halt case asserts the agent's own failure line.
    #
    # NO LibreSCRS::* link: every translation unit in this target speaks the
    # client's own vocabulary. A middleware link appearing here would mean a
    # wizard TU started parsing card data in-process again.
    #
    # The TEST_PREFIX is load-bearing: gtest_discover_tests registers the gtest
    # SUITE name (WizardFake), which does not contain "SigningWizardFake", so
    # `ctest -R SigningWizardFake` only matches through the prefix.
    add_executable(SigningWizardFakeTests
        signingwizard_fake_test.cpp
        fake_gateway/fakeagentgateway.cpp
        fake_gateway/fakecardcontroller.cpp
        fake_gateway/fakesigncontroller.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/agentgateway.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/cardcontroller.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/signcontroller.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/errortext.cpp
        ${CMAKE_SOURCE_DIR}/src/agent/signrequest.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/signingwizard.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/fileselectionpage.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/signatureplacementpage.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/signpage.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/filedropzone.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/filelistdelegate.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/pdfpreviewwidget.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/resultdelegate.cpp
        ${CMAKE_SOURCE_DIR}/src/signing/wizardheaderwidget.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/dialogs.cpp
    )
    target_include_directories(SigningWizardFakeTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/../src
    )
    target_link_libraries(SigningWizardFakeTests PRIVATE
        GTest::gtest_main
        Qt6::Widgets
        Qt6::Test
        Qt6::PdfWidgets
        LibreAgent::ClientQt
    )
    set_target_properties(SigningWizardFakeTests PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
    )
    target_compile_definitions(SigningWizardFakeTests PRIVATE
        # Build-tree .qm directory baked in at compile time so the fixture can
        # install LibreCelik_en.qm (mirrors TokenSectionTests). Both plural
        # lines this suite reads substitute a count into a TRANSLATED template,
        # so an untranslated run would render the bare catalog id and drop the
        # number the case is about.
        LIBRECELIK_TRANSLATIONS_DIR_DEFAULT="${CMAKE_BINARY_DIR}/src")
    add_dependencies(SigningWizardFakeTests LibreCelik_lrelease)
    gtest_discover_tests(SigningWizardFakeTests
        TEST_PREFIX "SigningWizardFake."
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# Header-card placeholder render guard, plus the certificate/credential row
# rendering. Exercises TokenSection::setTokenInfo against an empty FieldGroup
# and against a group containing a field with an empty value, asserting the
# header still renders and the em-dash (U+2014) placeholder appears in the
# rendered value cell; asserts the security-status tokens and the credential
# counters land in the rendered tree. Compiles the full TokenSection
# translation unit + its presentation dependencies; the heavyweight pattern
# mirrors SigningWizardE2ETests because the certificate viewer TUs are still
# middleware-typed and ride along in this target.
add_executable(TokenSectionTests
    tokensection_placeholder_test.cpp
    ${CMAKE_SOURCE_DIR}/src/document/tokensection.cpp
    ${CMAKE_SOURCE_DIR}/src/certificate/certformat.cpp
    ${CMAKE_SOURCE_DIR}/src/certificate/certificateviewerdlg.cpp
    ${CMAKE_SOURCE_DIR}/src/certificate/certificateviewerwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/certificate/certificateviewerwidget.ui
    ${CMAKE_SOURCE_DIR}/src/certificate/certificatehierarchymodel.cpp
    ${CMAKE_SOURCE_DIR}/src/certificate/certificatetreeviewmodel.cpp
    ${CMAKE_SOURCE_DIR}/src/certificate/certificatepropertiesmodel.cpp
    ${CMAKE_SOURCE_DIR}/src/certificate/certificateinfoitem.cpp
    # The viewer dialog connects to AgentGateway's certificateDerReady, so the
    # abstract gateway's moc TU has to be in the target; the header alone is
    # not enough for a pointer-to-member-signal connect to link.
    ${CMAKE_SOURCE_DIR}/src/agent/agentgateway.h
    ${CMAKE_SOURCE_DIR}/src/agent/agentgateway.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/cardheadercard.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/collapsiblesection.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/buttonbox.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/dialogs.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.cpp
)
target_include_directories(TokenSectionTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/../src
)
# The FIRST middleware-free test target: every TU it compiles — the token
# section and the whole certificate viewer — now speaks the agent client's
# types, so no LibreSCRS::Plugin / SmartCard / Trust / Certificate link
# remains. Keep it that way; a new link here means a TU started parsing card
# data in-process again.
target_link_libraries(TokenSectionTests PRIVATE
    GTest::gtest_main
    Qt6::Widgets
    Qt6::Test
    LibreAgent::ClientQt
)
set_target_properties(TokenSectionTests PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
target_compile_definitions(TokenSectionTests PRIVATE
    # Build-tree .qm directory baked in at compile time so the fixture can
    # install LibreCelik_en.qm (mirrors SlotLabelFormatterTests). The counter
    # row substitutes numbers into a translated template, so an untranslated
    # run would render the bare catalog ID and drop them.
    LIBRECELIK_TRANSLATIONS_DIR_DEFAULT="${CMAKE_BINARY_DIR}/src")
add_dependencies(TokenSectionTests LibreCelik_lrelease)
gtest_discover_tests(TokenSectionTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# --- Agent service-layer suites ---
#
# Home for every src/agent/ unit suite. Deliberately GTest::gtest, NOT
# gtest_main: the target's entry point is agentlayer_test_main.cpp, which
# installs a QCoreApplication before RUN_ALL_TESTS so QTimer-driven and
# transport-driven suites have a live application object and event
# dispatcher. Qt6::Test is linked for QSignalSpy, which the controller and
# watchdog suites use.
#
# The TEST_PREFIX is load-bearing: gtest_discover_tests registers the gtest
# SUITE names (ResolvePresence, ...), so `ctest -R AgentLayer` only matches
# through the prefix. Without it every filtered run of this target dies on
# --no-tests=error against perfectly correct code.
#
# fake_gateway/ is test-only scaffolding — the scripted backend every offscreen
# GUI suite in this campaign is driven by, plus the shared emission-contract
# helpers both the fakes and the live controllers are asserted against. It is
# compiled into test targets only and is never shipped. The abstract vtable
# anchors (agentgateway/cardcontroller/signcontroller .cpp) must be compiled in
# too: the fakes derive from those classes, so without the anchors the target
# cannot link.
add_executable(AgentLayerTests
    agentlayer_test_main.cpp
    agent_presence_test.cpp
    agent_artifactio_test.cpp
    agent_errortext_test.cpp
    agent_errortext_coverage_test.cpp
    agent_fieldmerge_test.cpp
    agent_opstallwatchdog_test.cpp
    agent_signrequest_test.cpp
    agent_liveagentgateway_test.cpp
    settings_import_test.cpp
    fake_gateway_selftest.cpp
    fake_gateway/fakeagentgateway.cpp
    fake_gateway/fakecardcontroller.cpp
    fake_gateway/fakesigncontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/agentgateway.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/cardcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/signcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/errortext.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/fieldmerge.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/opstallwatchdog.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/signrequest.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/settingsimport.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/live/liveagentgateway.cpp
    # The live gateway MINTS these two, so the suite that
    # compiles it links them: a test target that stubbed them out would prove
    # something the application never runs.
    ${CMAKE_SOURCE_DIR}/src/agent/live/livecardcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/live/livesigncontroller.cpp
)
target_include_directories(AgentLayerTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(AgentLayerTests PRIVATE
    GTest::gtest
    Qt6::Core
    Qt6::Test
    LibreAgent::ClientQt
)
set_target_properties(AgentLayerTests PROPERTIES AUTOMOC ON)
target_compile_definitions(AgentLayerTests PRIVATE
    # The error-text suite loads the Serbian catalog: the English-leak defect it
    # guards is invisible in an English session, where the leaked sentence and
    # the correct one are the same words. Baked in at compile time rather than
    # passed as a CTest ENVIRONMENT property, same as SlotLabelFormatterTests.
    LIBRECELIK_TRANSLATIONS_DIR_DEFAULT="${CMAKE_BINARY_DIR}/src")
add_dependencies(AgentLayerTests LibreCelik_lrelease)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # The coverage guard's whole mechanism: its classification switches carry no
    # `default:` arm, so an enumerator appended to the client library's ErrorCode
    # or CallError stops them being exhaustive and -Wswitch fires. Promoted to an
    # error on THIS source only, because errortext.cpp itself must keep its
    # default arms — at run time an unknown code has to reach the agent's own
    # message rather than a diagnostic.
    set_source_files_properties(agent_errortext_coverage_test.cpp
        PROPERTIES COMPILE_OPTIONS "-Werror=switch")
endif()
# TIMEOUT 30: the live-gateway suite dials the agent transport, so a wedged
# or absent bus must FAIL this gate rather than wedge it. The client's own
# probes are bounded (1 s handshake / 3 s call), so 30 s is pure margin.
gtest_discover_tests(AgentLayerTests
    TEST_PREFIX "AgentLayer."
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

# --- Main-window flow suites (offscreen GUI) ---
#
# The window itself is deliberately NOT in this binary: linking it drags in
# the whole application. What it COMPOSES is here — the guided presence panel
# and the streamed-group plugin dispatch — driven by the scripted fakes.
#
# Qt6::Widgets (not just Core) because these cases construct real widgets, and
# the fixture installs a QApplication for them.
#
# The abstract vtable anchors (agentgateway/cardcontroller/signcontroller .cpp)
# are compiled in for the same reason AgentLayerTests needs them: the fakes
# derive from those classes and the target cannot link without them.
# errortext.cpp is here because the scripted-error case renders through
# errorText(); libreceliklog.cpp because agentstatewidget.cpp logs its guided
# state through lcGeneral.
#
# The TEST_PREFIX is load-bearing: gtest_discover_tests registers the gtest
# SUITE names (AgentStateWidgetTest, MainFlowTest, ChangePinDlgTest), none of
# which contain "AgentMainFlow", so `ctest -R AgentMainFlow` only matches
# through the prefix.
#
# The credential dialog rides in this binary too: it is another piece the
# window composes, it is driven by the same scripted gateway, and it renders
# through errortext.cpp, which is already here.
add_executable(AgentMainFlowTests
    agent_mainflow_test.cpp
    changepindlg_test.cpp
    fake_gateway/fakeagentgateway.cpp
    fake_gateway/fakecardcontroller.cpp
    fake_gateway/fakesigncontroller.cpp
    mock_widget_plugin/mockwidgetplugin.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/agentgateway.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/cardcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/signcontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/agentstatewidget.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/cardretrypage.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/cardstatuspage.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/errortext.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/spinnerpage.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/messageline.cpp
    ${CMAKE_SOURCE_DIR}/src/document/rs-eid/changepindlg.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/buttonbox.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/libreceliklog.cpp
)
target_include_directories(AgentMainFlowTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_widget_plugin
    ${CMAKE_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(AgentMainFlowTests PRIVATE
    GTest::gtest_main
    Qt6::Widgets
    Qt6::Test
    LibreAgent::ClientQt
)
set_target_properties(AgentMainFlowTests PROPERTIES AUTOMOC ON)
target_compile_definitions(AgentMainFlowTests PRIVATE
    # Build-tree .qm directory baked in at compile time so the credential
    # dialog fixture can install LibreCelik_en.qm (mirrors TokenSectionTests
    # and SigningWizardFakeTests). Its retries line substitutes a count into a
    # TRANSLATED template, so an untranslated run would render the bare catalog
    # id and drop the number the case is about.
    LIBRECELIK_TRANSLATIONS_DIR_DEFAULT="${CMAKE_BINARY_DIR}/src")
add_dependencies(AgentMainFlowTests LibreCelik_lrelease)
gtest_discover_tests(AgentMainFlowTests
    TEST_PREFIX "AgentMainFlow."
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# The reader selector and the page stack, lifted out of the window so the two
# defects that lived between them are reachable at all. Needs no window, no
# gateway and no plugins — two widgets and a map — so this target stays tiny.
add_executable(ReaderPagesTests
    reader_pages_test.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/readerpages.cpp
)
target_include_directories(ReaderPagesTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(ReaderPagesTests PRIVATE
    GTest::gtest_main
    Qt6::Widgets
    Qt6::Test
)
set_target_properties(ReaderPagesTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(ReaderPagesTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# The i18n audit ran only in CI while a documented lupdate run could quietly
# strip the Serbian plural forms on a developer machine — the numerus guard
# (dimension D9) must fail the LOCAL gate too, so the audit is a ctest. The
# tool exits 1 on any non-allowlisted finding.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME I18nAudit
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/i18n_audit.py --strict)
    set_tests_properties(I18nAudit PROPERTIES TIMEOUT 120)
endif()
