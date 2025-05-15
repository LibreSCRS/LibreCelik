# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

project(LibreCelikTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Core Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(PCSC)

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

add_definitions(-DCERT_STORE_PATH="${PARENT_DIR}/thirdparty/certificates/all")

set(SOURCE_FILES
    ../src/utils/libreceliklog.h
    ../src/utils/libreceliklog.cpp
    ../src/celikapi/celikapiplus.h
    ../src/celikapi/celikapisession.h
    ../src/celikapi/celikapisession.cpp
    ../src/celikapi/celikapisessionfactory.h
    ../src/celikapi/celikapisessionfactory.cpp
    ../src/celikapi/celikapiwrapper.h
    ../src/celikapi/celikapiwrapper.cpp
    ../src/document/eid/eidreader.h
    ../src/document/eid/eidreader.cpp
    # ../src/smartcard/apducommand.h
    # ../src/smartcard/apducommand.cpp
    # ../src/smartcard/apduresponse.h
    # ../src/smartcard/apduresponse.cpp
    # ../src/smartcard/smartcard.h
    # ../src/smartcard/smartcard.cpp
    # ../src/smartcard/smartcardcontext.h
    # ../src/smartcard/smartcardcontext.cpp
    # ../src/smartcard/smartcardimpl.h
    # ../src/smartcard/smartcardimpl.cpp
    # ../src/smartcard/smartcardsession.h
    # ../src/smartcard/smartcardsession.cpp
)

add_executable(
    ${PROJECT_NAME}
    ${SOURCE_FILES}
    hello_test.cpp
    configuration_test.cpp
    botan_test.cpp
    uuid_test.cpp
    celikapi_test.cpp
    eidreader_test.cpp
    eidreader_test.h

)
target_link_libraries(
    ${PROJECT_NAME}
    GTest::gtest_main
    BOTAN
    CelikAPI
    PCSC::PCSC
    Qt6::Core
    Qt6::Widgets
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
