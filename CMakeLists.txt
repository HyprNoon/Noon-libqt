cmake_minimum_required(VERSION 3.21)
project(Noon VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Sql Svg Concurrent)
qt_standard_project_setup(REQUIRES 6.5)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})
find_package(KF6KIO REQUIRED)

include(GNUInstallDirs)

if(NOT QML_INSTALL_DIR)
  set(QML_INSTALL_DIR "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}" CACHE PATH "QML module install dir")
endif()

function(noon_setup target uri)
  cmake_parse_arguments(NOON "" "" "SOURCES;LINK;DEFINES;INCLUDES" ${ARGN})

  string(REPLACE "." "/" uri_path ${uri})

  qt_add_qml_module(${target}
    URI ${uri}
    VERSION ${PROJECT_VERSION}
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${uri_path}"
    SOURCES ${NOON_SOURCES}
  )

  if(NOON_LINK)
    target_link_libraries(${target} PRIVATE ${NOON_LINK})
  endif()

  if(NOON_DEFINES)
    target_compile_definitions(${target} PRIVATE ${NOON_DEFINES})
  endif()

  if(NOON_INCLUDES)
    target_include_directories(${target} PRIVATE ${NOON_INCLUDES})
  endif()

  install(TARGETS ${target} ${target}plugin DESTINATION "${QML_INSTALL_DIR}/${uri_path}")
  qt_query_qml_module(${target} QMLDIR _qmldir TYPEINFO _typeinfo)
  install(FILES ${_qmldir} ${_typeinfo} DESTINATION "${QML_INSTALL_DIR}/${uri_path}")
endfunction()

add_subdirectory(src)
