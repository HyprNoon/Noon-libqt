qt_add_qml_module(noon_devices
    URI Noon.Devices
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Devices"
    SOURCES
    GamePadTranslator.hpp
)

find_package(SDL2 REQUIRED)

target_link_libraries(noon_devices PRIVATE
    Qt6::Core
    Qt6::Quick
    SDL2::SDL2
)
noon_install_qml(noon_devices Noon.Devices)
