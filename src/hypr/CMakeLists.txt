qt_add_qml_module(noon_hypr
    URI Noon.Hypr
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Hypr"
    SOURCES
    HyprBridge.hpp
    HyprParser.hpp
    ScreenTimeManager.hpp
    ScreenTimeManager.cpp
)

find_package(Qt6 REQUIRED COMPONENTS Network)

target_link_libraries(noon_hypr PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
    Qt6::Quick
)
noon_setup(noon_hypr Noon.Hypr)
