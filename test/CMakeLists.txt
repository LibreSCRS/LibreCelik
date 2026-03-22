# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

project(LibreCelikTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Core Widgets Test)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)
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
    ../src/libreceliktypes.h
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
