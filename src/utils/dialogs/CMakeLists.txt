cmake_minimum_required(VERSION 3.21)

project(noon_utils_dialogs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Qml
    Quick
)

qt_add_qml_module(noon_utils_dialogs
    URI Noon.Utils.Dialogs
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Utils/Dialogs"
    SOURCES
        font_dialog.cpp
        font_dialog.hpp
)

target_include_directories(noon_utils_dialogs PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(noon_utils_dialogs PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
)

if(NOT QML_INSTALL_DIR)
    set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/lib/qt6/qml" CACHE PATH "QML install dir")
endif()

install(TARGETS noon_utils_dialogs noon_utils_dialogsplugin
    DESTINATION "${QML_INSTALL_DIR}/Noon/Utils/Dialogs"
)

qt_query_qml_module(noon_utils_dialogs
    QMLDIR   qmldir_dialogs
    TYPEINFO typeinfo_dialogs
)

install(FILES ${qmldir_dialogs} ${typeinfo_dialogs}
    DESTINATION "${QML_INSTALL_DIR}/Noon/Utils/Dialogs"
)
