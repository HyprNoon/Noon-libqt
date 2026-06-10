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
noon_setup(noon_utils_latex Noon.Utils.Latex)
