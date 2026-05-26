qt_add_qml_module(noon_utils_hypr
    URI Noon.Utils.Hypr
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Utils/Hypr"
    SOURCES
    HyprBridge.hpp
    HyprParser.hpp
)

find_package(Qt6 REQUIRED COMPONENTS Core Network Concurrent)

target_link_libraries(noon_utils_hypr PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
    Qt6::Quick
)

if(NOT QML_INSTALL_DIR)
    set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/lib/qt6/qml" CACHE PATH "QML install dir")
endif()

install(TARGETS noon_utils_hypr noon_utils_hyprplugin DESTINATION "${QML_INSTALL_DIR}/Noon/Utils/Hypr")
qt_query_qml_module(noon_utils_hypr QMLDIR qmldir_hypr TYPEINFO typeinfo_hypr)
install(FILES ${qmldir_hypr} ${typeinfo_hypr} DESTINATION "${QML_INSTALL_DIR}/Noon/Utils/Hypr")
