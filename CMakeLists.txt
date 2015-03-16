cmake_minimum_required (VERSION 2.8)

set (CMAKE_BUILD_TYPE Debug)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")

get_filename_component (APP_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
FILE (GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable (${APP_NAME} ${SRCS})

target_link_libraries (${APP_NAME} -lpthread)
