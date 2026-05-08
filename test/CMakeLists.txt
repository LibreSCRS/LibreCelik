# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

project(LibreCelikTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Core Widgets Test PrintSupport)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test PrintSupport)
# PCSC is found by LibreMiddleware; UUID is used directly by tests
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
    CardPlugin_Impl
    RsEId
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
    CardPlugin_Impl
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
)
target_include_directories(AsyncCardReaderTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(AsyncCardReaderTests
    GTest::gtest_main
    CardPlugin_Impl
    SmartCard_Impl
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
    SmartCard_Impl
    LibreSCRS_SmartCard
    Threads::Threads
)
# detail/MonitorInjection.h is internal: #error-guarded behind
# LIBRESCRS_INTERNAL_BUILD. Mirrors LM's LibreSCRS_MonitorTests setup.
target_compile_definitions(QSmartCardMonitorTests PRIVATE LIBRESCRS_INTERNAL_BUILD)
target_include_directories(QSmartCardMonitorTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${libremiddleware_SOURCE_DIR}/test
    ${libremiddleware_SOURCE_DIR}/lib/smartcard/src
)
gtest_discover_tests(QSmartCardMonitorTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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
    CardPlugin_Impl
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

# Pure helper — only Qt::Core needed (QString, QLocale, QStringList).
# No QApplication, no widgets; runs in milliseconds without any UI
# fixture so the locale-fallback contract is exercised on every CI run.
add_executable(LocaleResolverTests
    locale_resolver_test.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/locale_resolver.cpp
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
    CardPlugin_Impl
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
    CardPlugin_Impl
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
    CardPlugin_Impl
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
        LibreSCRS_Signing
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
        LibreSCRS_Signing
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
        LibreSign
        LibreSCRS_Signing
    )
    gtest_discover_tests(SigningUnitTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

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
        LibreSign
        LibreSCRS_Signing
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
        ${CMAKE_SOURCE_DIR}/src/signing/pkcs11utils.cpp
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
        CardPlugin_Impl
        SmartCard_Impl
        LibreSign
        LibreSCRS_Signing
        LibreSCRS::Certificate
        PCSC::PCSC
    )
    target_compile_definitions(SigningWizardE2ETests PRIVATE
        MIDDLEWARE_PLUGIN_DIR="$<TARGET_FILE_DIR:piv-plugin>"
        PKCS11_MODULE_PATH="$<TARGET_FILE:librescrs-pkcs11>"
        DSS_JAR_DIR="${libremiddleware_SOURCE_DIR}/tools/dss-service/target"
        # qt_add_translations binds the :/i18n/ resource only to the main
        # LibreCelik target, so the test binary loads the .qm from the
        # build dir directly. The path tracks where qt_add_translations
        # writes the lrelease output (currently ${CMAKE_BINARY_DIR}/src/).
        LIBRECELIK_QM_DIR="${CMAKE_BINARY_DIR}/src"
        LIBRESCRS_TESTING
    )
    set_target_properties(SigningWizardE2ETests PROPERTIES AUTOMOC ON)
    add_dependencies(SigningWizardE2ETests piv-plugin pkcs15-plugin cardedge-plugin librescrs-pkcs11 LibreCelik_lrelease)
    gtest_discover_tests(SigningWizardE2ETests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
