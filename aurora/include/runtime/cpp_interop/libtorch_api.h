#pragma once
#include <cstdint>

#ifdef _WIN32
#define TORCH_API __declspec(dllexport)
#else
#define TORCH_API __attribute__((visibility("default")))
#endif

extern "C" {

TORCH_API void* tensor_1d(int64_t d1, int dtype);
TORCH_API void* tensor_2d(int64_t d1, int64_t d2, int dtype);
TORCH_API void* tensor_3d(int64_t d1, int64_t d2, int64_t d3, int dtype);

TORCH_API void* tensor_create(const int64_t* shape, int ndim, int dtype);
TORCH_API void* tensor_from_blob(void* data, const int64_t* shape, int ndim, int dtype, int64_t numel);
TORCH_API void* tensor_randn(const int64_t* shape, int ndim, int dtype);
TORCH_API void* tensor_zeros(const int64_t* shape, int ndim, int dtype);
TORCH_API void* tensor_ones (const int64_t* shape, int ndim, int dtype);
TORCH_API void* tensor_full (const int64_t* shape, int ndim, double value, int dtype);
TORCH_API void* tensor_arange(int64_t start, int64_t end, int64_t step);

TORCH_API void* tensor_randn_2d(int64_t rows, int64_t cols);
TORCH_API void* tensor_zeros_2d(int64_t rows, int64_t cols);
TORCH_API void* tensor_ones_2d(int64_t rows, int64_t cols);

TORCH_API void*   tensor_data_ptr(void* tensor);
TORCH_API double  tensor_item_float(void* tensor);
TORCH_API int64_t tensor_item_int(void* tensor);

TORCH_API void  tensor_destroy(void* tensor);
TORCH_API void* tensor_clone(void* tensor);

TORCH_API int     tensor_ndim(void* tensor);
TORCH_API int64_t tensor_size(void* tensor, int dim);
TORCH_API int64_t tensor_numel(void* tensor);

TORCH_API void tensor_print(void* tensor);
TORCH_API void tensor_print_shape(void* tensor);
TORCH_API void tensor_print_data(void* tensor, int count);

TORCH_API void* tensor_add(void* a, void* b);
TORCH_API void* tensor_sub(void* a, void* b);
TORCH_API void* tensor_mul(void* a, void* b);
TORCH_API void* tensor_div(void* a, void* b);
TORCH_API void* tensor_add_scalar(void* a, double s);
TORCH_API void* tensor_mul_scalar(void* a, double s);

TORCH_API void* tensor_matmul(void* a, void* b);
TORCH_API void* tensor_dot(void* a, void* b);
TORCH_API void* tensor_transpose_2d(void* a);

TORCH_API void* tensor_relu(void* a);
TORCH_API void* tensor_sigmoid(void* a);
TORCH_API void* tensor_tanh(void* a);
TORCH_API void* tensor_softmax(void* a, int dim);

TORCH_API void* tensor_reshape(void* a, const int64_t* shape, int ndim);
TORCH_API void* tensor_view(void* a, const int64_t* shape, int ndim);
TORCH_API void* tensor_transpose(void* a, int dim0, int dim1);
TORCH_API void* tensor_flatten(void* a);

TORCH_API void* tensor_to_device(void* a, int device_type, int device_index);
TORCH_API void* tensor_to_cpu(void* a);

TORCH_API void* module_load(const char* path);
TORCH_API void* module_forward(void* module, void* input);
TORCH_API void  module_destroy(void* module);

TORCH_API void* optimizer_sgd(void* params, double lr, double momentum);
TORCH_API void  optimizer_zero_grad(void* optimizer);
TORCH_API void  optimizer_step(void* optimizer);
TORCH_API void  optimizer_destroy(void* optimizer);

TORCH_API int cuda_is_available();
TORCH_API int cuda_device_count();

#define TORCH_DTYPE_FLOAT   0
#define TORCH_DTYPE_DOUBLE  1
#define TORCH_DTYPE_INT32   2
#define TORCH_DTYPE_INT64   3
#define TORCH_DTYPE_BOOL    4
#define TORCH_DTYPE_BF16    5

#define TORCH_DEVICE_CPU  0
#define TORCH_DEVICE_CUDA 1

}
