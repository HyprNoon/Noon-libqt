pkg_check_modules(CLATEX REQUIRED clatexmath cairomm-1.0 pangomm-1.4)

noon_setup(noon_utils_latex Noon.Utils.Latex
    SOURCES
        LatexRenderer.hpp
    LINK
        Qt6::Quick
        Qt6::Concurrent
        ${CLATEX_LIBRARIES}
    DEFINES
        BUILD_GTK
    INCLUDES
        ${CLATEX_INCLUDE_DIRS}
)
