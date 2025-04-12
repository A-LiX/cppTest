cmake_minimum_required(VERSION 3.10)
project(boost_client)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 Boost 库的路径
set(BOOST_ROOT "/usr/local/boost")  # 假设 Boost 安装在 /usr/local/boost
set(Boost_NO_SYSTEM_PATHS ON)  # 禁止使用系统路径查找 Boost

# 查找 Boost 库
find_package(Boost 1.87 REQUIRED COMPONENTS system asio ssl)

# 设置 simdjson 库的路径
set(simdjson_DIR "/usr/local/simdjson")  # 假设 simdjson 安装在 /usr/local/simdjson

# 查找 simdjson 库
find_package(simdjson REQUIRED)

# 设置 spdlog 库的路径
set(spdlog_DIR "/usr/local/spdlog")  # 假设 spdlog 安装在 /usr/local/spdlog

# 查找 spdlog 库
find_package(spdlog REQUIRED)

# 设置 OpenSSL 库的路径
set(OPENSSL_ROOT_DIR "/usr/local/openssl")  # 假设 OpenSSL 安装在 /usr/local/openssl

# 查找 OpenSSL 库
find_package(OpenSSL REQUIRED)

# 包含头文件路径
include_directories(${Boost_INCLUDE_DIRS} ${SIMDJSON_INCLUDE_DIRS} ${SPDLOG_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

# 添加可执行文件
add_executable(boost_client boost_client.cpp)

# 链接库
target_link_libraries(boost_client
    Boost::system
    Boost::asio
    Boost::ssl
    simdjson::simdjson
    spdlog::spdlog
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread  # 显式链接 pthread，用于 spdlog 异步日志
)