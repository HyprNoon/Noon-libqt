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
noon_setup(noon_utils_dialogs Noon.Utils.Dialogs)
