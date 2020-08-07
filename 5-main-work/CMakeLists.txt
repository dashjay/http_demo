cmake_minimum_required(VERSION 3.16)

set(project_name http_demo)

project(${project_name})

set(CMAKE_CXX_COMPILER "c++")
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS -g -W -lsockpp -lpthread)
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS: " "${CMAKE_CXX_FLAGS}")

include_directories(.)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

set(static_http server.cpp bufio.cpp headers.cpp cxxhttp.cpp)

add_library(static_http_lib SHARED ${static_http})

add_executable(main main.cpp)
target_link_libraries(main static_http_lib)


add_executable(multi_thread multi_thread.cpp)
add_executable(lambda lambda.cpp headers.cpp)