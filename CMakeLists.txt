cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
add_library(lua5.1 SHARED vscript_lua51.cpp)

add_library(vscript SHARED vscript_patch.cpp)
target_link_libraries(vscript PUBLIC lua5.1)

add_library(luametahook SHARED luametahook.cpp)
target_link_libraries(luametahook PUBLIC lua5.1)
