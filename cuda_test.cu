#include <iostream>

#define N 512

// CUDA 内核函数：每个线程处理一个数组元素
__global__ void vector_add(float* A, float* B, float* C) {
    int idx = threadIdx.x + blockDim.x * blockIdx.x;
    if (idx < N) {
        C[idx] = A[idx] + B[idx];
    }
}

int main() {
    // CPU 上的数组
    float h_A[N], h_B[N], h_C[N];

    // 初始化数据
    for (int i = 0; i < N; ++i) {
        h_A[i] = i;
        h_B[i] = i * 2;
    }

    // GPU 上的数组指针
    float *d_A, *d_B, *d_C;

    // 分配 GPU 内存
    cudaMalloc((void**)&d_A, N * sizeof(float));
    cudaMalloc((void**)&d_B, N * sizeof(float));
    cudaMalloc((void**)&d_C, N * sizeof(float));

    // 复制数据到 GPU
    cudaMemcpy(d_A, h_A, N * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, h_B, N * sizeof(float), cudaMemcpyHostToDevice);

    // 启动 kernel，每个 block 256 个线程
    vector_add<<<(N + 255) / 256, 256>>>(d_A, d_B, d_C);

    // 把结果从 GPU 拷贝回来
    cudaMemcpy(h_C, d_C, N * sizeof(float), cudaMemcpyDeviceToHost);

    // 打印部分结果
    for (int i = 0; i < 10; ++i) {
        std::cout << h_A[i] << " + " << h_B[i] << " = " << h_C[i] << std::endl;
    }

    // 释放 GPU 内存
    cudaFree(d_A);
    cudaFree(d_B);
    cudaFree(d_C);

    return 0;
}