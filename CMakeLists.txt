cmake_minimum_required(VERSION 3.10)
project(binance_ws_client)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Boost 库
find_package(Boost REQUIRED COMPONENTS system json ssl)

# 查找 OpenSSL 库
find_package(OpenSSL REQUIRED)

# 包含头文件路径
include_directories(${Boost_INCLUDE_DIRS})

# 添加可执行文件
add_executable(binance_ws_client ws_ans.cpp)

# 链接库
target_link_libraries(binance_ws_client
    PRIVATE
    Boost::system
    Boost::json
    Boost::ssl
    OpenSSL::SSL
    OpenSSL::Crypto
)