cmake_minimum_required(VERSION 3.16)

set(project_name http_demo)

project(${project_name})

set(CMAKE_CXX_COMPILER "c++")
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 17) 

set(CMAKE_CXX_FLAGS -g -W -lsockpp)
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS: " "${CMAKE_CXX_FLAGS}")

include_directories(.)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_executable(main main.cpp cxxhttp.cpp headers.cpp bufio.hpp)