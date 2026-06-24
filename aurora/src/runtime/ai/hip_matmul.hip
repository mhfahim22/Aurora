/* HIP/ROCm matmul kernel - GPU-accelerated matrix multiplication */
#include <hip/hip_runtime.h>

__global__ void hip_matmul_kernel(const double* A, const double* B, double* C,
                                   int64_t M, int64_t N, int64_t K) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < M && col < N) {
        double sum = 0.0;
        for (int64_t k = 0; k < K; k++)
            sum += A[row * K + k] * B[k * N + col];
        C[row * N + col] = sum;
    }
}

extern "C" int hip_matmul_available() {
    int count = 0;
    hipError_t err = hipGetDeviceCount(&count);
    return (err == hipSuccess && count > 0) ? 1 : 0;
}

extern "C" int hip_matmul(const double* A, const double* B, double* C,
                          int64_t M, int64_t N, int64_t K) {
    if (!A || !B || !C) return 0;
    double *dA, *dB, *dC;
    hipMalloc(&dA, (size_t)M * K * sizeof(double));
    hipMalloc(&dB, (size_t)K * N * sizeof(double));
    hipMalloc(&dC, (size_t)M * N * sizeof(double));
    hipMemcpy(dA, A, (size_t)M * K * sizeof(double), hipMemcpyHostToDevice);
    hipMemcpy(dB, B, (size_t)K * N * sizeof(double), hipMemcpyHostToDevice);
    dim3 block(16, 16);
    dim3 grid((N + 15) / 16, (M + 15) / 16);
    hipLaunchKernelGGL(hip_matmul_kernel, grid, block, 0, 0, dA, dB, dC, M, N, K);
    hipMemcpy(C, dC, (size_t)M * N * sizeof(double), hipMemcpyDeviceToHost);
    hipFree(dA); hipFree(dB); hipFree(dC);
    return 1;
}
