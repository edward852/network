cmake_minimum_required(VERSION 3.10)
project(select)

set(CMAKE_CXX_FLAGS -g)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
aux_source_directory(. SRC_FILES)
add_executable(select ${SRC_FILES})

find_package(Threads REQUIRED)
target_link_libraries(select ${CMAKE_THREAD_LIBS_INIT})
