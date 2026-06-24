#include <cuda_runtime.h>
#include <cstdio>
#include <cmath>

extern "C" {

/* ── Element-wise F32 kernels (grid-stride loop) ── */

__global__ void add_kernel_f32(const float* a, const float* b, float* out, int64_t n) {
    int64_t i = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    int64_t stride = (int64_t)gridDim.x * blockDim.x;
    for (; i < n; i += stride) out[i] = a[i] + b[i];
}

__global__ void sub_kernel_f32(const float* a, const float* b, float* out, int64_t n) {
    int64_t i = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    int64_t stride = (int64_t)gridDim.x * blockDim.x;
    for (; i < n; i += stride) out[i] = a[i] - b[i];
}

__global__ void mul_kernel_f32(const float* a, const float* b, float* out, int64_t n) {
    int64_t i = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    int64_t stride = (int64_t)gridDim.x * blockDim.x;
    for (; i < n; i += stride) out[i] = a[i] * b[i];
}

__global__ void div_kernel_f32(const float* a, const float* b, float* out, int64_t n) {
    int64_t i = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    int64_t stride = (int64_t)gridDim.x * blockDim.x;
    for (; i < n; i += stride) out[i] = (b[i] != 0.0f) ? a[i] / b[i] : 0.0f;
}

__global__ void relu_kernel_f32(float* data, int64_t n) {
    int64_t i = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    int64_t stride = (int64_t)gridDim.x * blockDim.x;
    for (; i < n; i += stride) if (data[i] < 0.0f) data[i] = 0.0f;
}

__global__ void sigmoid_kernel_f32(float* data, int64_t n) {
    int64_t i = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    int64_t stride = (int64_t)gridDim.x * blockDim.x;
    for (; i < n; i += stride) data[i] = 1.0f / (1.0f + expf(-data[i]));
}

/* ── Helper: launch 1D kernel with grid-stride ── */
static int launch_elewise(dim3 block, int64_t n) {
    int64_t grid_size = (n + (int64_t)block.x - 1) / (int64_t)block.x;
    if (grid_size > 65535) grid_size = 65535;
    dim3 grid((unsigned int)grid_size);
    return (int)grid.x;
}

/* ── Host-callable wrappers (returns 1 on success, 0 on failure) ── */

int cuda_elewise_available() {
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    return (err == cudaSuccess && count > 0) ? 1 : 0;
}

int cuda_add_f32(const float* a, const float* b, float* out, int64_t n) {
    float *dA, *dB, *dO;
    if (cudaMalloc(&dA, n * sizeof(float)) != cudaSuccess) return 0;
    if (cudaMalloc(&dB, n * sizeof(float)) != cudaSuccess) { cudaFree(dA); return 0; }
    if (cudaMalloc(&dO, n * sizeof(float)) != cudaSuccess) { cudaFree(dA); cudaFree(dB); return 0; }
    cudaMemcpy(dA, a, n * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(dB, b, n * sizeof(float), cudaMemcpyHostToDevice);
    dim3 block(256);
    int g = launch_elewise(block, n);
    add_kernel_f32<<<g, block>>>(dA, dB, dO, n);
    cudaMemcpy(out, dO, n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaFree(dA); cudaFree(dB); cudaFree(dO);
    return 1;
}

int cuda_sub_f32(const float* a, const float* b, float* out, int64_t n) {
    float *dA, *dB, *dO;
    if (cudaMalloc(&dA, n * sizeof(float)) != cudaSuccess) return 0;
    if (cudaMalloc(&dB, n * sizeof(float)) != cudaSuccess) { cudaFree(dA); return 0; }
    if (cudaMalloc(&dO, n * sizeof(float)) != cudaSuccess) { cudaFree(dA); cudaFree(dB); return 0; }
    cudaMemcpy(dA, a, n * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(dB, b, n * sizeof(float), cudaMemcpyHostToDevice);
    dim3 block(256);
    int g = launch_elewise(block, n);
    sub_kernel_f32<<<g, block>>>(dA, dB, dO, n);
    cudaMemcpy(out, dO, n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaFree(dA); cudaFree(dB); cudaFree(dO);
    return 1;
}

int cuda_mul_f32(const float* a, const float* b, float* out, int64_t n) {
    float *dA, *dB, *dO;
    if (cudaMalloc(&dA, n * sizeof(float)) != cudaSuccess) return 0;
    if (cudaMalloc(&dB, n * sizeof(float)) != cudaSuccess) { cudaFree(dA); return 0; }
    if (cudaMalloc(&dO, n * sizeof(float)) != cudaSuccess) { cudaFree(dA); cudaFree(dB); return 0; }
    cudaMemcpy(dA, a, n * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(dB, b, n * sizeof(float), cudaMemcpyHostToDevice);
    dim3 block(256);
    int g = launch_elewise(block, n);
    mul_kernel_f32<<<g, block>>>(dA, dB, dO, n);
    cudaMemcpy(out, dO, n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaFree(dA); cudaFree(dB); cudaFree(dO);
    return 1;
}

int cuda_div_f32(const float* a, const float* b, float* out, int64_t n) {
    float *dA, *dB, *dO;
    if (cudaMalloc(&dA, n * sizeof(float)) != cudaSuccess) return 0;
    if (cudaMalloc(&dB, n * sizeof(float)) != cudaSuccess) { cudaFree(dA); return 0; }
    if (cudaMalloc(&dO, n * sizeof(float)) != cudaSuccess) { cudaFree(dA); cudaFree(dB); return 0; }
    cudaMemcpy(dA, a, n * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(dB, b, n * sizeof(float), cudaMemcpyHostToDevice);
    dim3 block(256);
    int g = launch_elewise(block, n);
    div_kernel_f32<<<g, block>>>(dA, dB, dO, n);
    cudaMemcpy(out, dO, n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaFree(dA); cudaFree(dB); cudaFree(dO);
    return 1;
}

int cuda_relu_f32(float* data, int64_t n) {
    float *dD;
    if (cudaMalloc(&dD, n * sizeof(float)) != cudaSuccess) return 0;
    cudaMemcpy(dD, data, n * sizeof(float), cudaMemcpyHostToDevice);
    dim3 block(256);
    int g = launch_elewise(block, n);
    relu_kernel_f32<<<g, block>>>(dD, n);
    cudaMemcpy(data, dD, n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaFree(dD);
    return 1;
}

int cuda_sigmoid_f32(float* data, int64_t n) {
    float *dD;
    if (cudaMalloc(&dD, n * sizeof(float)) != cudaSuccess) return 0;
    cudaMemcpy(dD, data, n * sizeof(float), cudaMemcpyHostToDevice);
    dim3 block(256);
    int g = launch_elewise(block, n);
    sigmoid_kernel_f32<<<g, block>>>(dD, n);
    cudaMemcpy(data, dD, n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaFree(dD);
    return 1;
}

} /* extern "C" */
