# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

project(LibreCelikTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Core Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
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
    ../src/document/eid/eidreader.h
    ../src/document/eid/eidreader.cpp
)

add_executable(
    ${PROJECT_NAME}
    ${SOURCE_FILES}
    hello_test.cpp
    configuration_test.cpp
    uuid_test.cpp
    eidreader_test.cpp
    eidreader_test.h
)
target_link_libraries(
    ${PROJECT_NAME}
    GTest::gtest_main
    EIdCard
    VehicleCard
    PCSC::PCSC
    UUID::UUID
    Qt6::Core
    Qt6::Widgets
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
