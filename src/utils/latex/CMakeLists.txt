find_package(PkgConfig REQUIRED)
pkg_check_modules(CLATEX REQUIRED clatexmath cairomm-1.0 pangomm-1.4)

qt_add_qml_module(noon_utils_latex
    URI Noon.Utils.Latex
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Utils/Latex"
    SOURCES
        LatexRenderer.hpp
)

target_compile_definitions(noon_utils_latex PRIVATE BUILD_GTK)
target_include_directories(noon_utils_latex PRIVATE ${CLATEX_INCLUDE_DIRS})
target_link_libraries(noon_utils_latex PRIVATE
    Qt6::Quick
    Qt6::Concurrent
    ${CLATEX_LIBRARIES}
)

if(NOT QML_INSTALL_DIR)
    set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/lib/qt6/qml" CACHE PATH "QML install dir")
endif()

install(TARGETS noon_utils_latex noon_utils_latexplugin DESTINATION "${QML_INSTALL_DIR}/Noon/Utils/Latex")
qt_query_qml_module(noon_utils_latex QMLDIR qmldir_latex TYPEINFO typeinfo_latex)
install(FILES ${qmldir_latex} ${typeinfo_latex} DESTINATION "${QML_INSTALL_DIR}/Noon/Utils/Latex")
