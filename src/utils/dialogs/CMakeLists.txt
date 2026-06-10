noon_setup(noon_utils_dialogs Noon.Utils.Dialogs
    SOURCES
        font_dialog.cpp
        font_dialog.hpp
    LINK
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Qml
        Qt6::Quick
    INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
