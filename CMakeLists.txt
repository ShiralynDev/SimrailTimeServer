cmake_minimum_required(VERSION 3.10)

project(SimrailTimeServer)

add_definitions(-DEPOLL)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/extern/httpserver)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/extern/httpserver/src)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/extern/curl/include)

add_executable(SimrailTimeServer Main.c)
target_link_libraries(SimrailTimeServer curl)