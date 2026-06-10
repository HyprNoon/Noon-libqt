add_subdirectory(lib/qtqr)
add_subdirectory(utils)
add_subdirectory(protocols)
add_subdirectory(network)
add_subdirectory(hypr)
add_subdirectory(devices)
add_subdirectory(taskmanager)

qt_add_qml_module(noon
    URI Noon
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon"
)

target_link_libraries(noon PRIVATE Qt6::Quick)
noon_install_qml(noon Noon)
