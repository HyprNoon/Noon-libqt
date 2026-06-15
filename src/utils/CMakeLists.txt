noon_setup(noon_utils Noon.Utils
    SOURCES
        CavaWatcher.hpp
        ResourcesWatcher.hpp
        FileSystemWatcher.hpp
        SQLReader.hpp
        QMLCrawler.hpp
        EnvManager.hpp
        MpvPlayer.hpp
        TomlParser.hpp
        TaskManager.hpp
        QrCode.hpp
    QT_COMPONENTS
        Core Gui Qml Quick Sql Concurrent Widgets Svg
    PACKAGES
        MpvQt
    PKG_CHECK
        DEPS libdrm nvidia-ml sdl2
    LINK
        Qt6::Quick
        Qt6::Sql
        Qt6::Concurrent
        Qt6::Widgets
        Qt6::Svg
        MpvQt::MpvQt
        qtqr
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/../lib/qtqr/src
)

add_subdirectory(latex)
add_subdirectory(dialogs)
