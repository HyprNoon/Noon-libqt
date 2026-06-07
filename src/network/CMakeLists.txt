qt_add_qml_module(noon_network
    URI Noon.Network
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Network"
    SOURCES
      NetworkManager.hpp
      NetworkManager.cpp
)

find_package(Qt6 REQUIRED COMPONENTS Core Network Concurrent)
find_package(KF6NetworkManagerQt REQUIRED)

target_link_libraries(noon_network PRIVATE
    KF6::NetworkManagerQt
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
    Qt6::Quick
)

if(NOT QML_INSTALL_DIR)
  set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/lib/qt6/qml" CACHE PATH "QML install dir")
endif()

install(TARGETS noon_network noon_networkplugin DESTINATION "${QML_INSTALL_DIR}/Noon/Network")
qt_query_qml_module(noon_network QMLDIR qmldir_network TYPEINFO typeinfo_network)
install(FILES ${qmldir_network} ${typeinfo_network} DESTINATION "${QML_INSTALL_DIR}/Noon/Network")
