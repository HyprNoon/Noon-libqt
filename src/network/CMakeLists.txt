qt_add_qml_module(noon_network
    URI Noon.Network
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Network"
    SOURCES
      NetworkManager.hpp
      NetworkManager.cpp
)

find_package(Qt6 REQUIRED COMPONENTS Network)
find_package(KF6NetworkManagerQt REQUIRED)

target_link_libraries(noon_network PRIVATE
    KF6::NetworkManagerQt
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
    Qt6::Quick
)
noon_install_qml(noon_network Noon.Network)
