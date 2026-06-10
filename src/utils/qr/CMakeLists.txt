qt_add_qml_module(noon_utils_qr
    URI Noon.Utils.Qr
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Utils/Qr"
    SOURCES
    
    qrcode.cpp
    qrcode.hpp
)

target_include_directories(noon_utils_qr  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/qtqr/src"
)
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Svg
)

target_link_libraries(noon_utils_qr  PRIVATE
    Qt6::Quick
    Qt6::Svg
    qtqr
)
noon_install_qml(noon_utils_qr Noon.Utils.Qr)
