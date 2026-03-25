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
)
target_link_libraries(
    ${PROJECT_NAME}
    GTest::gtest_main
    CardPlugin
    EIdCard
    UUID::UUID
    Qt6::Core
    Qt6::Widgets
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})

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
    CardPlugin
)
target_sources(CardWidgetPluginRegistryTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/plugin/cardwidgetpluginregistry.cpp
)
target_compile_definitions(CardWidgetPluginRegistryTests PRIVATE
    MOCK_WIDGET_PLUGIN_DIR="$<TARGET_FILE_DIR:mock-widget-plugin>"
)
set_target_properties(CardWidgetPluginRegistryTests PROPERTIES AUTOMOC ON)
add_dependencies(CardWidgetPluginRegistryTests mock-widget-plugin)
gtest_discover_tests(CardWidgetPluginRegistryTests)

add_executable(AsyncCardReaderTests
    asynccardreader_test.cpp
    ${CMAKE_SOURCE_DIR}/src/asynccardreader.cpp
)
target_include_directories(AsyncCardReaderTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(AsyncCardReaderTests
    GTest::gtest_main
    CardPlugin
    SmartCard
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)
set_target_properties(AsyncCardReaderTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(AsyncCardReaderTests)

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
    SmartCard
    Threads::Threads
)
target_include_directories(QSmartCardMonitorTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${libremiddleware_SOURCE_DIR}/test
)
gtest_discover_tests(QSmartCardMonitorTests)

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
    CardPlugin
    LibreCelikPrinting
    Qt6::Widgets
    Qt6::PrintSupport
)
set_target_properties(EuVrcTextDocumentTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(EuVrcTextDocumentTests)

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
gtest_discover_tests(StringUtilsTests)

add_executable(HealthTextDocumentTests
    healthtextdocument_test.cpp
    ../plugins/health/healthtextdocument.h
    ../plugins/health/healthtextdocument.cpp
    ../src/utils/libreceliklog.h
    ../src/utils/libreceliklog.cpp
    ${CMAKE_SOURCE_DIR}/resources/librecelik.qrc
)
target_include_directories(HealthTextDocumentTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/document/printing
    ${CMAKE_SOURCE_DIR}/plugins/health
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(HealthTextDocumentTests
    GTest::gtest
    CardPlugin
    LibreCelikPrinting
    Qt6::Widgets
    Qt6::PrintSupport
)
set_target_properties(HealthTextDocumentTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(HealthTextDocumentTests)

add_executable(EIdTextDocumentTests
    eidtextdocument_test.cpp
    ../plugins/rseid/eidtextdocument.h
    ../plugins/rseid/eidtextdocument.cpp
    ../src/utils/libreceliklog.h
    ../src/utils/libreceliklog.cpp
    ${CMAKE_SOURCE_DIR}/resources/librecelik.qrc
)
target_include_directories(EIdTextDocumentTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/document/printing
    ${CMAKE_SOURCE_DIR}/plugins/rseid
    ${PROJECT_BINARY_DIR}/../src
)
target_link_libraries(EIdTextDocumentTests
    GTest::gtest
    CardPlugin
    LibreCelikPrinting
    Qt6::Widgets
    Qt6::PrintSupport
)
set_target_properties(EIdTextDocumentTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(EIdTextDocumentTests)

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
)
set_target_properties(EMRTDAuthWidgetTests PROPERTIES AUTOMOC ON)
gtest_discover_tests(EMRTDAuthWidgetTests)
