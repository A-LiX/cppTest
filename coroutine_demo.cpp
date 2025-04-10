#include <stdio.h>
#include <stdlib.h>
#include "aco.h" // 包含 libaco 的头文件

// 定义一个简单的协程函数
void task_func() {
    int i = 0;
    while (i < 5) {
        printf("Task running: step %d (coroutine: %p)\n", i, aco_get_co());
        i++;
        aco_yield(); // 主动让出协程的执行权
    }
    printf("Task finished (coroutine: %p)\n", aco_get_co());
}

int main() {
    // 初始化主协程上下文
    aco_thread_init(NULL);

    // 创建协程的共享栈（所有协程可以共享同一个栈）
    aco_share_stack_t *share_stack = aco_share_stack_new(0);

    // 创建协程
    aco_t *co = aco_create(NULL, share_stack, 0, task_func, NULL);

    // 运行协程
    while (!aco_is_end(co)) {
        printf("Resuming coroutine: %p\n", co);
        aco_resume(co); // 恢复协程的执行
    }

    // 清理资源
    aco_destroy(co);
    aco_share_stack_destroy(share_stack);

    return 0;
}