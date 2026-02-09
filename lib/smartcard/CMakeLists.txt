# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright hirashix0@proton.me

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PCSC REQUIRED)

add_library(SmartCard STATIC
    src/apdu.cpp
    src/pcsc_connection.cpp
    src/tlv.cpp
)

target_include_directories(SmartCard PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SmartCard PUBLIC PCSC::PCSC)
