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
noon_setup(noon_utils_cef Noon.Utils.Cef)
