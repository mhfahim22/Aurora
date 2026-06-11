#include <cuda_runtime.h>
#include <cstdio>

extern "C" {

/* Simple tiled matrix multiplication kernel: C[M][N] = A[M][K] @ B[K][N] (row-major) */
__global__ void matmul_kernel(const double* A, const double* B, double* C,
                              int64_t M, int64_t K, int64_t N) {
    int64_t row = blockIdx.y * blockDim.y + threadIdx.y;
    int64_t col = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < M && col < N) {
        double sum = 0.0;
        for (int64_t k = 0; k < K; k++)
            sum += A[row * K + k] * B[k * N + col];
        C[row * N + col] = sum;
    }
}

int cuda_matmul_available() {
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    return (err == cudaSuccess && count > 0) ? 1 : 0;
}

int cuda_matmul(double* A, double* B, double* C,
                int64_t M, int64_t K, int64_t N) {
    double *dA, *dB, *dC;
    cudaError_t err;

    err = cudaMalloc(&dA, M * K * sizeof(double));
    if (err != cudaSuccess) return 0;
    err = cudaMalloc(&dB, K * N * sizeof(double));
    if (err != cudaSuccess) { cudaFree(dA); return 0; }
    err = cudaMalloc(&dC, M * N * sizeof(double));
    if (err != cudaSuccess) { cudaFree(dA); cudaFree(dB); return 0; }

    cudaMemcpy(dA, A, M * K * sizeof(double), cudaMemcpyHostToDevice);
    cudaMemcpy(dB, B, K * N * sizeof(double), cudaMemcpyHostToDevice);

    dim3 block(16, 16);
    dim3 grid((N + 15) / 16, (M + 15) / 16);
    matmul_kernel<<<grid, block>>>(dA, dB, dC, M, K, N);

    cudaMemcpy(C, dC, M * N * sizeof(double), cudaMemcpyDeviceToHost);

    cudaFree(dA); cudaFree(dB); cudaFree(dC);
    return 1;
}

} /* extern "C" */
