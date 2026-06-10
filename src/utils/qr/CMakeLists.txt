find_package(Qt6 REQUIRED COMPONENTS Core Gui Svg)

noon_setup(noon_utils_qr Noon.Utils.Qr
    SOURCES
        qrcode.cpp
        qrcode.hpp
    LINK
        Qt6::Quick
        Qt6::Svg
        qtqr
    INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/qtqr/src"
)
