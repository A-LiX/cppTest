#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <iostream>

cppcoro::single_consumer_event event1;
cppcoro::single_consumer_event event2;

cppcoro::task<> task1() {
    while (true) {
        co_await event1;   // 等待自己被触发
        std::cout << "Task 1 running" << std::endl;
        event1.reset();    // 重置自己
        event2.set();      // 唤醒 task2
    }
}

cppcoro::task<> task2() {
    while (true) {
        co_await event2;   // 等待自己被触发
        std::cout << "Task 2 running" << std::endl;
        event2.reset();    // 重置自己
        event1.set();      // 唤醒 task1
    }
}

int main() {
    // 启动时先激活 task1
    event1.set();
    cppcoro::sync_wait(cppcoro::when_all_ready(task1(), task2()));
}