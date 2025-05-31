if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
# Botan
add_library(BOTAN STATIC IMPORTED GLOBAL)
set_target_properties(BOTAN PROPERTIES
    IMPORTED_IMPLIB  "${PROJECT_SOURCE_DIR}/thirdparty/botan-3.8.0/macosx/lib/libbotan-3.a"
    IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/thirdparty/botan-3.8.0/macosx/lib/libbotan-3.a"
    INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/thirdparty/botan-3.8.0/include/public"
)

# CelikAPI
add_library(CelikAPI SHARED IMPORTED GLOBAL)
set_target_properties(CelikAPI PROPERTIES
    IMPORTED_IMPLIB  "${PROJECT_SOURCE_DIR}/thirdparty/celikapi-3.3.10/macosx/lib/libCelikApi.dylib"
    IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/thirdparty/celikapi-3.3.10/macosx/lib/libCelikApi.dylib"
    INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/thirdparty/celikapi-3.3.10/include"
)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    # Botan
    add_library(BOTAN STATIC IMPORTED GLOBAL)
    set_target_properties(BOTAN PROPERTIES
        IMPORTED_IMPLIB  "${PROJECT_SOURCE_DIR}/thirdparty/botan-3.8.0/linux/lib/libbotan-3.a"
        IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/thirdparty/botan-3.8.0/linux/lib/libbotan-3.a"
        INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/thirdparty/botan-3.8.0/include/public"
    )

    # CelikAPI
    add_library(CelikAPI SHARED IMPORTED GLOBAL)
    set_target_properties(CelikAPI PROPERTIES
        IMPORTED_IMPLIB  "${PROJECT_SOURCE_DIR}/thirdparty/celikapi-3.4.0/linux/lib/libcelikapi.so"
        IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/thirdparty/celikapi-3.4.0/linux/lib/libcelikapi.so"
        INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/thirdparty/celikapi-3.4.0/include"
    )
endif()
