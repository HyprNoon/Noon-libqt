find_package(PkgConfig REQUIRED)
find_package(MpvQt REQUIRED)
pkg_check_modules(DEPS REQUIRED libdrm nvidia-ml sdl2)

noon_setup(noon_utils Noon.Utils
    SOURCES
        CavaWatcher.hpp
        ResourcesWatcher.hpp
        FileSystemWatcher.hpp
        SQLReader.hpp
        QMLCrawler.hpp
        EnvManager.hpp
        MpvPlayer.hpp
        TaskManager.hpp
        QrCode.hpp
    LINK
        Qt6::Quick
        Qt6::Sql
        Qt6::Concurrent
        Qt6::Widgets
        Qt6::Svg
        MpvQt::MpvQt
        qtqr
        ${DEPS_LIBRARIES}
    INCLUDES
        ${DEPS_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../lib/qtqr/src
)

add_subdirectory(latex)
add_subdirectory(dialogs)
