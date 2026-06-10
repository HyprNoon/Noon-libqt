qt_add_qml_module(noon_taskmanager
    URI Noon.TaskManager
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Noon/TaskManager"
    SOURCES
        TaskManager.hpp
)

target_link_libraries(noon_taskmanager PRIVATE Qt6::Quick)

if(NOT QML_INSTALL_DIR)
  set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/lib/qt6/qml" CACHE PATH "QML install dir")
endif()

install(TARGETS noon_taskmanager noon_taskmanagerplugin DESTINATION "${QML_INSTALL_DIR}/Noon/TaskManager")
qt_query_qml_module(noon_taskmanager QMLDIR qmldir_var TYPEINFO typeinfo_var)
install(FILES ${qmldir_var} ${typeinfo_var} DESTINATION "${QML_INSTALL_DIR}/Noon/TaskManager")
