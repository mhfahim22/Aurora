#include "runtime/cpp_interop/libtorch_api.h"

#include <torch/torch.h>
#include <cstring>
#include <vector>
#include <new>
#include <iostream>

static torch::Dtype to_dtype(int dtype) {
    switch (dtype) {
        case TORCH_DTYPE_FLOAT:   return torch::kFloat;
        case TORCH_DTYPE_DOUBLE:  return torch::kDouble;
        case TORCH_DTYPE_INT32:   return torch::kInt32;
        case TORCH_DTYPE_INT64:   return torch::kInt64;
        case TORCH_DTYPE_BOOL:    return torch::kBool;
        case TORCH_DTYPE_BF16:    return torch::kBFloat16;
        default: return torch::kFloat;
    }
}

static std::vector<int64_t> to_shape(const int64_t* shape, int ndim) {
    return std::vector<int64_t>(shape, shape + ndim);
}

void* tensor_1d(int64_t d1, int dtype) {
    auto* t = new torch::Tensor(torch::zeros({d1}, to_dtype(dtype)));
    return static_cast<void*>(t);
}

void* tensor_2d(int64_t d1, int64_t d2, int dtype) {
    auto* t = new torch::Tensor(torch::zeros({d1, d2}, to_dtype(dtype)));
    return static_cast<void*>(t);
}

void* tensor_3d(int64_t d1, int64_t d2, int64_t d3, int dtype) {
    auto* t = new torch::Tensor(torch::zeros({d1, d2, d3}, to_dtype(dtype)));
    return static_cast<void*>(t);
}

void* tensor_create(const int64_t* shape, int ndim, int dtype) {
    auto s = to_shape(shape, ndim);
    auto* t = new torch::Tensor(torch::zeros(s, to_dtype(dtype)));
    return static_cast<void*>(t);
}

void* tensor_from_blob(void* data, const int64_t* shape, int ndim, int dtype, int64_t numel) {
    (void)numel;
    auto s = to_shape(shape, ndim);
    auto opts = torch::TensorOptions().dtype(to_dtype(dtype));
    auto* t = new torch::Tensor(torch::from_blob(data, s, opts).clone());
    return static_cast<void*>(t);
}

void* tensor_randn(const int64_t* shape, int ndim, int dtype) {
    auto s = to_shape(shape, ndim);
    auto* t = new torch::Tensor(torch::randn(s, to_dtype(dtype)));
    return static_cast<void*>(t);
}

void* tensor_zeros(const int64_t* shape, int ndim, int dtype) {
    auto s = to_shape(shape, ndim);
    auto* t = new torch::Tensor(torch::zeros(s, to_dtype(dtype)));
    return static_cast<void*>(t);
}

void* tensor_ones(const int64_t* shape, int ndim, int dtype) {
    auto s = to_shape(shape, ndim);
    auto* t = new torch::Tensor(torch::ones(s, to_dtype(dtype)));
    return static_cast<void*>(t);
}

void* tensor_full(const int64_t* shape, int ndim, double value, int dtype) {
    auto s = to_shape(shape, ndim);
    auto* t = new torch::Tensor(torch::full(s, value, to_dtype(dtype)));
    return static_cast<void*>(t);
}

void* tensor_arange(int64_t start, int64_t end, int64_t step) {
    auto* t = new torch::Tensor(torch::arange(start, end, step));
    return static_cast<void*>(t);
}

void* tensor_randn_2d(int64_t rows, int64_t cols) {
    auto* t = new torch::Tensor(torch::randn({rows, cols}));
    return static_cast<void*>(t);
}

void* tensor_zeros_2d(int64_t rows, int64_t cols) {
    auto* t = new torch::Tensor(torch::zeros({rows, cols}));
    return static_cast<void*>(t);
}

void* tensor_ones_2d(int64_t rows, int64_t cols) {
    auto* t = new torch::Tensor(torch::ones({rows, cols}));
    return static_cast<void*>(t);
}

void* tensor_data_ptr(void* tensor) {
    if (!tensor) return nullptr;
    return static_cast<torch::Tensor*>(tensor)->data_ptr();
}

double tensor_item_float(void* tensor) {
    if (!tensor) return 0.0;
    return static_cast<torch::Tensor*>(tensor)->item<double>();
}

int64_t tensor_item_int(void* tensor) {
    if (!tensor) return 0;
    return static_cast<torch::Tensor*>(tensor)->item<int64_t>();
}

void tensor_destroy(void* tensor) {
    if (tensor) delete static_cast<torch::Tensor*>(tensor);
}

void* tensor_clone(void* tensor) {
    if (!tensor) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(tensor);
    auto* c = new torch::Tensor(t.clone());
    return static_cast<void*>(c);
}

int tensor_ndim(void* tensor) {
    if (!tensor) return 0;
    return static_cast<int>(static_cast<torch::Tensor*>(tensor)->dim());
}

int64_t tensor_size(void* tensor, int dim) {
    if (!tensor) return 0;
    return static_cast<torch::Tensor*>(tensor)->size(dim);
}

int64_t tensor_numel(void* tensor) {
    if (!tensor) return 0;
    return static_cast<torch::Tensor*>(tensor)->numel();
}

void tensor_print(void* tensor) {
    if (!tensor) return;
    auto& t = *static_cast<torch::Tensor*>(tensor);
    std::cout << t << std::endl;
}

void tensor_print_shape(void* tensor) {
    if (!tensor) return;
    auto& t = *static_cast<torch::Tensor*>(tensor);
    std::cout << "[";
    for (int i = 0; i < t.dim(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << t.size(i);
    }
    std::cout << "]" << std::endl;
}

void tensor_print_data(void* tensor, int count) {
    if (!tensor || count <= 0) return;
    auto& t = *static_cast<torch::Tensor*>(tensor);
    auto flat = t.flatten();
    int n = std::min(count, (int)t.numel());
    for (int i = 0; i < n; i++) {
        if (i > 0) std::cout << ", ";
        std::cout << flat[i].item<double>();
    }
    std::cout << std::endl;
}

void* tensor_add(void* a, void* b) {
    if (!a || !b) return nullptr;
    auto& ta = *static_cast<torch::Tensor*>(a);
    auto& tb = *static_cast<torch::Tensor*>(b);
    auto* r = new torch::Tensor(ta + tb);
    return static_cast<void*>(r);
}

void* tensor_sub(void* a, void* b) {
    if (!a || !b) return nullptr;
    auto& ta = *static_cast<torch::Tensor*>(a);
    auto& tb = *static_cast<torch::Tensor*>(b);
    auto* r = new torch::Tensor(ta - tb);
    return static_cast<void*>(r);
}

void* tensor_mul(void* a, void* b) {
    if (!a || !b) return nullptr;
    auto& ta = *static_cast<torch::Tensor*>(a);
    auto& tb = *static_cast<torch::Tensor*>(b);
    auto* r = new torch::Tensor(ta * tb);
    return static_cast<void*>(r);
}

void* tensor_div(void* a, void* b) {
    if (!a || !b) return nullptr;
    auto& ta = *static_cast<torch::Tensor*>(a);
    auto& tb = *static_cast<torch::Tensor*>(b);
    auto* r = new torch::Tensor(ta / tb);
    return static_cast<void*>(r);
}

void* tensor_add_scalar(void* a, double s) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(t + s);
    return static_cast<void*>(r);
}

void* tensor_mul_scalar(void* a, double s) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(t * s);
    return static_cast<void*>(r);
}

void* tensor_matmul(void* a, void* b) {
    if (!a || !b) return nullptr;
    auto& ta = *static_cast<torch::Tensor*>(a);
    auto& tb = *static_cast<torch::Tensor*>(b);
    auto* r = new torch::Tensor(torch::matmul(ta, tb));
    return static_cast<void*>(r);
}

void* tensor_dot(void* a, void* b) {
    if (!a || !b) return nullptr;
    auto& ta = *static_cast<torch::Tensor*>(a);
    auto& tb = *static_cast<torch::Tensor*>(b);
    auto* r = new torch::Tensor(torch::dot(ta, tb));
    return static_cast<void*>(r);
}

void* tensor_transpose_2d(void* a) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(t.t());
    return static_cast<void*>(r);
}

void* tensor_relu(void* a) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(torch::relu(t));
    return static_cast<void*>(r);
}

void* tensor_sigmoid(void* a) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(torch::sigmoid(t));
    return static_cast<void*>(r);
}

void* tensor_tanh(void* a) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(torch::tanh(t));
    return static_cast<void*>(r);
}

void* tensor_softmax(void* a, int dim) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(torch::softmax(t, dim));
    return static_cast<void*>(r);
}

void* tensor_reshape(void* a, const int64_t* shape, int ndim) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(t.reshape(to_shape(shape, ndim)));
    return static_cast<void*>(r);
}

void* tensor_view(void* a, const int64_t* shape, int ndim) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(t.view(to_shape(shape, ndim)));
    return static_cast<void*>(r);
}

void* tensor_transpose(void* a, int dim0, int dim1) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(t.transpose(dim0, dim1));
    return static_cast<void*>(r);
}

void* tensor_flatten(void* a) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(t.flatten());
    return static_cast<void*>(r);
}

void* tensor_to_device(void* a, int device_type, int device_index) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    torch::Device device(
        device_type == TORCH_DEVICE_CUDA ? torch::kCUDA : torch::kCPU,
        device_index);
    auto* r = new torch::Tensor(t.to(device));
    return static_cast<void*>(r);
}

void* tensor_to_cpu(void* a) {
    if (!a) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(a);
    auto* r = new torch::Tensor(t.to(torch::kCPU));
    return static_cast<void*>(r);
}

void* module_load(const char* path) {
    try {
        auto* mod = new torch::nn::Module();
        torch::load(*mod, path);
        return static_cast<void*>(mod);
    } catch (...) {
        return nullptr;
    }
}

void* module_forward(void* module, void* input) {
    if (!module || !input) return nullptr;
    auto* mod = static_cast<torch::nn::Module*>(module);
    auto& inp = *static_cast<torch::Tensor*>(input);
    auto* r = new torch::Tensor(mod->forward({inp}).toTensor());
    return static_cast<void*>(r);
}

void module_destroy(void* module) {
    if (module) delete static_cast<torch::nn::Module*>(module);
}

void* optimizer_sgd(void* params, double lr, double momentum) {
    if (!params) return nullptr;
    auto& t = *static_cast<torch::Tensor*>(params);
    auto* opt = new torch::optim::SGD({t}, torch::optim::SGDOptions(lr).momentum(momentum));
    return static_cast<void*>(opt);
}

void optimizer_zero_grad(void* optimizer) {
    if (!optimizer) return;
    static_cast<torch::optim::SGD*>(optimizer)->zero_grad();
}

void optimizer_step(void* optimizer) {
    if (!optimizer) return;
    static_cast<torch::optim::SGD*>(optimizer)->step();
}

void optimizer_destroy(void* optimizer) {
    if (optimizer) delete static_cast<torch::optim::SGD*>(optimizer);
}

int cuda_is_available() {
    return torch::cuda::is_available() ? 1 : 0;
}

int cuda_device_count() {
    return static_cast<int>(torch::cuda::device_count());
}
