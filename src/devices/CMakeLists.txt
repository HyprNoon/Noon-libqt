qt_add_qml_module(noon_devices
    URI Noon.Devices
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Devices"
    SOURCES
    GamePadTranslator.hpp
)

find_package(Qt6 REQUIRED COMPONENTS Core Quick)
find_package(SDL2 REQUIRED)

target_link_libraries(noon_devices PRIVATE
    Qt6::Core
    Qt6::Quick
    SDL2::SDL2
)

if(NOT QML_INSTALL_DIR)
  set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/lib/qt6/qml" CACHE PATH "QML install dir")
endif()

install(TARGETS noon_devices noon_devicesplugin DESTINATION "${QML_INSTALL_DIR}/Noon/Devices")
qt_query_qml_module(noon_devices QMLDIR qmldir_devices TYPEINFO typeinfo_devices)
install(FILES ${qmldir_devices} ${typeinfo_devices} DESTINATION "${QML_INSTALL_DIR}/Noon/Devices")
