# 设置最低的 CMake 版本要求
cmake_minimum_required(VERSION 3.10)

# 定义项目名称
project(cppTest)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 指定编译器路径
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_C_COMPILER "/usr/bin/gcc")

# 添加第一个程序
add_executable(boost_client boost_client.cpp)
target_include_directories(boost_client PRIVATE /opt/homebrew/include)
target_link_directories(boost_client PRIVATE /opt/homebrew/lib)
target_link_libraries(boost_client z pthread ssl crypto boost_system simdjson)

# 添加第二个程序
add_executable(other_program other_program.cpp)
target_include_directories(other_program PRIVATE /opt/homebrew/include)
target_link_directories(other_program PRIVATE /opt/homebrew/lib)
target_link_libraries(other_program z pthread ssl crypto boost_system simdjson)