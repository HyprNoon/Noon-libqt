cmake_minimum_required(VERSION 3.16)

find_package(tinyxml2 REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)

file(GLOB_RECURSE MICROTEX_SRC_FILES
    "src/*.cpp"
)

file(GLOB_RECURSE MICROTEX_RES_FILES
    "res/builtin/*.cpp"
    "res/font/*.cpp"
    "res/sym/*.cpp"
    "res/reg/*.cpp"
    "res/parser/*.cpp"
)

add_library(microtex STATIC
    ${MICROTEX_SRC_FILES}
    ${MICROTEX_RES_FILES}
)

set_target_properties(microtex PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(microtex PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/res
)

target_link_libraries(microtex PUBLIC
    Qt6::Core
    Qt6::Gui
    tinyxml2::tinyxml2
)

target_compile_definitions(microtex PUBLIC
    BUILD_QT
    MICROTEX_RES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/res"
)
