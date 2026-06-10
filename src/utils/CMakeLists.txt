find_package(PkgConfig REQUIRED)
find_package(MpvQt REQUIRED)
pkg_check_modules(DEPS REQUIRED libdrm nvidia-ml sdl2)

qt_add_qml_module(noon_utils
    URI Noon.Utils
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Utils"
    SOURCES
        CavaWatcher.hpp
        ClipboardManager.hpp
        ResourcesWatcher.hpp
        FileSystemWatcher.hpp
        SQLReader.hpp
        QMLCrawler.hpp
        EnvManager.hpp
        MpvPlayer.hpp
)

target_include_directories(noon_utils PRIVATE
    ${DEPS_INCLUDE_DIRS}
)

target_link_libraries(noon_utils PRIVATE
    Qt6::Quick
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Widgets
    MpvQt::MpvQt
    ${DEPS_LIBRARIES}
)
noon_setup(noon_utils Noon.Utils)

#add_subdirectory(cef)
add_subdirectory(latex)
add_subdirectory(qr)
add_subdirectory(dialogs)
