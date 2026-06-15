add_subdirectory(lib/qtqr)
add_subdirectory(utils)
add_subdirectory(protocols)
add_subdirectory(network)
add_subdirectory(hypr)
add_subdirectory(devices)

noon_setup(noon Noon
    QT_COMPONENTS Qml Quick
    LINK Qt6::Quick
)
