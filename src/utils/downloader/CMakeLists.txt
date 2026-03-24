qt_add_qml_module(noon_utils_download
    URI Noon.Utils.Download
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Utils/Download"
    SOURCES
        dw_item.cpp
        dw_item.hpp
        dw_model.cpp
        dw_model.hpp
)

target_include_directories(noon_utils_download PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(noon_utils_download PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    KF6::KIOCore
    KF6::KIOWidgets
    KF6::KIOGui
)

if(NOT QML_INSTALL_DIR)
    set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/lib/qt6/qml" CACHE PATH "QML install dir")
endif()

install(TARGETS noon_utils_download noon_utils_downloadplugin
    DESTINATION "${QML_INSTALL_DIR}/Noon/Utils/Download"
)

qt_query_qml_module(noon_utils_download
    QMLDIR   qmldir_download
    TYPEINFO typeinfo_download
)

install(FILES ${qmldir_download} ${typeinfo_download}
    DESTINATION "${QML_INSTALL_DIR}/Noon/Utils/Download"
)
