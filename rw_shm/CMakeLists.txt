cmake_minimum_required(VERSION 3.10)
project(ws_depth_writer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 Release 编译优化
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# 查找 Drogon（需要先安装）
find_package(Drogon REQUIRED)

# 查找 jsoncpp（系统安装后应能找到）
find_package(jsoncpp REQUIRED)

# 查找 SQLite3（用于 Drogon 编译时依赖）
find_package(SQLite3 REQUIRED)

# 查找 hiredis
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY hiredis)

# 头文件搜索路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${HIREDIS_INCLUDE_DIR}
)

# 添加可执行文件
add_executable(ws_depth_writer
    ws_depth_writer.cpp
)

# 链接 Drogon、jsoncpp、hiredis、ssl 等依赖
target_link_libraries(ws_depth_writer
    Drogon::Drogon
    jsoncpp
    ${HIREDIS_LIBRARY}
    pthread
    ssl
    crypto
    z
    sqlite3
    cares
)