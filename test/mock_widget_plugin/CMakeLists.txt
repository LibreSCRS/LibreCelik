find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(mock-widget-plugin MODULE mockwidgetplugin.cpp)
# LibreAgent::ClientQt: the v2 interface this module implements speaks the
# client's field-group types.
target_link_libraries(mock-widget-plugin PRIVATE Qt6::Widgets LibreAgent::ClientQt)
target_include_directories(mock-widget-plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
set_target_properties(mock-widget-plugin PROPERTIES
    AUTOMOC ON
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
