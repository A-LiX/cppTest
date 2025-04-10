# 定义编译器
CXX = g++

# 定义编译选项
CXXFLAGS = -std=c++11 -Wall -I/usr/local/include

# 定义链接选项
LDFLAGS = -L/usr/local/lib -lboost_system -lspdlog -lfmt -lssl -lcrypto -lsimdjson -pthread

# 定义目标文件
TARGET = boost_client

# 定义源文件
SRCS = boost_client.cpp

# 定义目标文件依赖关系
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

# 清理生成的文件
clean:
	rm -f $(TARGET)