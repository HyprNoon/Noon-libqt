set(QCEFVIEW_DIR "/home/pharmaracist/.local/QCefView")

add_library(QCefView SHARED IMPORTED GLOBAL)
set_target_properties(QCefView PROPERTIES
    IMPORTED_LOCATION "${QCEFVIEW_DIR}/bin/libQCefView.so"
    INTERFACE_INCLUDE_DIRECTORIES "${QCEFVIEW_DIR}/include"
)

qt_add_qml_module(noon_utils_cef
    URI Noon.Utils.Cef
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Utils/Cef"
    SOURCES
        CefWidget.hpp
)

target_link_libraries(noon_utils_cef PRIVATE
    Qt6::Quick
    Qt6::Widgets
    QCefView
)

target_link_directories(noon_utils_cef PRIVATE
    "${QCEFVIEW_DIR}/bin"
)
set_target_properties(noon_utils_cef PROPERTIES
    INSTALL_RPATH "${QCEFVIEW_DIR}/bin"
    BUILD_RPATH "${QCEFVIEW_DIR}/bin"
)

if(NOT QML_INSTALL_DIR)
    set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/lib/qt6/qml" CACHE PATH "QML install dir")
endif()

install(TARGETS noon_utils_cef noon_utils_cefplugin DESTINATION "${QML_INSTALL_DIR}/Noon/Utils/Cef")
qt_query_qml_module(noon_utils_cef QMLDIR qmldir_cef TYPEINFO typeinfo_cef)
install(FILES ${qmldir_cef} ${typeinfo_cef} DESTINATION "${QML_INSTALL_DIR}/Noon/Utils/Cef")
