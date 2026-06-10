find_package(PkgConfig REQUIRED)
find_package(MpvQt REQUIRED)
pkg_check_modules(DEPS REQUIRED libdrm nvidia-ml sdl2)

noon_setup(noon_utils Noon.Utils
    SOURCES
        CavaWatcher.hpp
        ClipboardManager.hpp
        ResourcesWatcher.hpp
        FileSystemWatcher.hpp
        SQLReader.hpp
        QMLCrawler.hpp
        EnvManager.hpp
        MpvPlayer.hpp
    LINK
        Qt6::Quick
        Qt6::Sql
        Qt6::Concurrent
        Qt6::Widgets
        MpvQt::MpvQt
        ${DEPS_LIBRARIES}
    INCLUDES
        ${DEPS_INCLUDE_DIRS}
)

#add_subdirectory(cef)
add_subdirectory(latex)
add_subdirectory(qr)
add_subdirectory(dialogs)
add_subdirectory(taskmanager)
