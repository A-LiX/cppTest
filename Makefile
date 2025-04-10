# 编译器
CXX = g++
# 编译选项
CXXFLAGS = -std=c++17 -I/opt/homebrew/include -L/opt/homebrew/lib -pthread
# 链接的库
LIBS = -lspdlog -lfmt -lssl -lcrypto -lsimdjson
# 目标文件
TARGET = boost_client
# 源文件
SRC = boost_client.cpp

# 默认目标
all: $(TARGET)

# 编译目标
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LIBS)

# 清理目标
clean:
	rm -f $(TARGET)
