qt_add_qml_module(noon_protocols
    URI Noon.Protocols
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/Protocols"
    SOURCES
    FileProtocols.hpp
    FileProtocols.cpp
)

target_link_libraries(noon_protocols PRIVATE
    Qt6::Core
    Qt6::Qml
)
noon_setup(noon_protocols Noon.Protocols)
