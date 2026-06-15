noon_setup(noon_utils_latex Noon.Utils.Latex
    SOURCES
        LatexRenderer.hpp
    QT_COMPONENTS
        Quick Concurrent
    PKG_CHECK
        CLATEX clatexmath cairomm-1.0 pangomm-1.4
    LINK
        Qt6::Quick
        Qt6::Concurrent
    DEFINES
        BUILD_GTK
)
