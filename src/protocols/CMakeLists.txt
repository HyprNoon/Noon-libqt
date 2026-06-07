qt_add_qml_module(noon_protocols
    URI Noon.Protocols
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Protocols"
    SOURCES
    FileProtocols.hpp
    FileProtocols.cpp
)

find_package(Qt6 REQUIRED COMPONENTS Core)

target_link_libraries(noon_protocols PRIVATE
    Qt6::Core
    Qt6::Qml
)

if(NOT QML_INSTALL_DIR)
  set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/lib/qt6/qml" CACHE PATH "QML install dir")
endif()

install(TARGETS noon_protocols noon_protocolsplugin DESTINATION "${QML_INSTALL_DIR}/Noon/Protocols")
qt_query_qml_module(noon_protocols QMLDIR qmldir_protocols TYPEINFO typeinfo_protocols)
install(FILES ${qmldir_protocols} ${typeinfo_protocols} DESTINATION "${QML_INSTALL_DIR}/Noon/Protocols")
