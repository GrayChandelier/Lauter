cmake_minimum_required(VERSION 3.20)

project(Lauter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)


file(GLOB_RECURSE SOURCES
    CONFIGURE_DEPENDS
    src/*
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PREFIX "src"
    FILES ${SOURCES}
)

add_executable(Lauter
    ${SOURCES}
)

