# 指定cmake的版本，随意了，我们不会用太高级的功能
cmake_minimum_required(VERSION 3.16)

# set可以设定一个变量
set(project_name http_demo)

# 项目名称 ${xxxx} 可以导入之前的变量
project(${project_name})

set(CMAKE_CXX_COMPILER "c++") # 编译器
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 17) # cpp指定标准

# 设定编译的FLAGS
set(CMAKE_CXX_FLAGS -g -W -lsockpp)
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS: " "${CMAKE_CXX_FLAGS}")

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

# 添加一个可执行文件
add_executable(main main.cpp)
add_executable(server server.cpp)