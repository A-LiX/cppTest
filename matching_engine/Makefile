CXX = g++
CXXFLAGS = -std=c++20 -Wall -O2

# 可执行文件名称
TARGET1 = normal_matching
TARGET2 = auction_matching

# 源文件
SRCS = src/normal_matching.cpp src/auction_matching.cpp

# 目标文件
OBJS = src/normal_matching.o src/auction_matching.o

# 默认目标
all: $(TARGET1) $(TARGET2)

# 编译第一个可执行文件
$(TARGET1): src/normal_matching.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# 编译第二个可执行文件
$(TARGET2): src/auction_matching.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# 编译源文件
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理目标
clean:
	rm -f $(OBJS) $(TARGET1) $(TARGET2)