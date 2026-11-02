find_package(Qt6 REQUIRED COMPONENTS Widgets)

# A static plugin, like the application's own card pages: linking it imports
# it, and the registry suite reads it back through the same static-plugin list
# the application uses.
qt_add_plugin(mock-widget-plugin STATIC CLASS_NAME MockWidgetPlugin)
target_sources(mock-widget-plugin PRIVATE mockwidgetplugin.h mockwidgetplugin.cpp)
# LibreAgent::ClientQt: the v2 interface this plugin implements speaks the
# client's field-group types.
target_link_libraries(mock-widget-plugin PRIVATE Qt6::Widgets LibreAgent::ClientQt)
target_include_directories(mock-widget-plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
set_target_properties(mock-widget-plugin PROPERTIES AUTOMOC ON)
