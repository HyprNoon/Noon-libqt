qt_add_qml_module(noon_taskmanager
    URI Noon.TaskManager
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/TaskManager"
    SOURCES
        TaskManager.hpp
)

target_link_libraries(noon_taskmanager PRIVATE Qt6::Quick)
noon_setup(noon_taskmanager Noon.TaskManager)
