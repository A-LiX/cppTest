#include <coroutine>
#include <iostream>

struct Task {
    struct promise_type {
        Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::exit(1); }
    };
    std::coroutine_handle<promise_type> handle;
    explicit Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() { if (handle) handle.destroy(); }
};

Task my_coroutine() {
    std::cout << "协程开始\n";
    co_await std::suspend_always{};
    std::cout << "协程恢复\n";
}

int main() {
    auto t = my_coroutine();
    std::cout << "main: 首次挂起后\n";
    t.handle.resume();
    std::cout << "main: 协程已恢复并结束\n";
    return 0;
}