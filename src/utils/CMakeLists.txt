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

if(NOT QML_INSTALL_DIR)
  set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/lib/qt6/qml"
        CACHE PATH "QML install dir")
endif()

install(
    TARGETS noon_utils noon_utilsplugin
    DESTINATION "${QML_INSTALL_DIR}/Noon/Utils"
)

qt_query_qml_module(noon_utils
    QMLDIR    qmldir_utils
    TYPEINFO typeinfo_utils
)

install(
    FILES ${qmldir_utils} ${typeinfo_utils}
    DESTINATION "${QML_INSTALL_DIR}/Noon/Utils"
)

add_subdirectory(latex)
# add_subdirectory(downloader)
add_subdirectory(network)
add_subdirectory(hypr)
add_subdirectory(devices)
add_subdirectory(qr)
add_subdirectory(dialogs)
