#include "runtime/ai/ai_common.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_4.h>
#endif

extern "C" {

/* Forward decls from other modules — stubs if not compiled */
#if AURORA_CUDA
int cuda_matmul_available();
#else
int cuda_matmul_available() { return 0; }
#endif
#if AURORA_HIP
int hip_matmul_available();
#else
int hip_matmul_available() { return 0; }
#endif
int dml_available();  /* always compiled on WIN32 */

/* ── Detect what GPU acceleration is available ── */
char* gpu_info() {
    char buf[2048];
    int64_t pos = 0;

#ifdef _WIN32
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "GPU Detection:\n");

    IDXGIFactory4* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) {
        IDXGIAdapter1* adapter = nullptr;
        int found = 0;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { adapter->Release(); continue; }
            char gpu_name[256];
            size_t converted = 0;
            wcstombs_s(&converted, gpu_name, sizeof(gpu_name), desc.Description, sizeof(gpu_name) - 1);
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [%d] %s (VRAM: %lld MB)\n",
                            found++, gpu_name, (long long)(desc.DedicatedVideoMemory / (1024 * 1024)));
            adapter->Release();
        }
        factory->Release();
        if (found == 0)
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  No GPU found\n");
    } else {
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  DXGI enumeration failed\n");
    }
#else
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "GPU Detection: not supported on this platform\n");
#endif

    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\nAcceleration Paths:\n");

#if AURORA_CUDA
    if (cuda_matmul_available())
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [OK]  CUDA (NVIDIA GPU)\n");
    else
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [--]  CUDA compiled but no NVIDIA GPU detected\n");
#else
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [--]  CUDA: not compiled (needs CUDA Toolkit)\n");
#endif

#if AURORA_HIP
    if (hip_matmul_available())
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [OK]  ROCm/HIP (AMD GPU)\n");
    else
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [--]  ROCm/HIP compiled but no AMD GPU detected\n");
#else
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [--]  ROCm/HIP: not compiled (needs ROCm SDK)\n");
#endif

#if AURORA_DML
    if (dml_available())
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [OK]  DirectML (any DX12 GPU)\n");
    else
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [--]  DirectML compiled but no DX12 GPU detected\n");
#else
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [--]  DirectML: not compiled\n");
#endif

#ifdef _OPENMP
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [OK]  OpenMP (CPU multi-core): enabled\n");
#else
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  [--]  OpenMP: not compiled\n");
#endif

    return AURORA_STRDUP(buf);
}

/* ── Simple GPU count ── */
int64_t gpu_count() {
    int count = 0;
#if AURORA_CUDA
    if (cuda_matmul_available()) count++;
#endif
#if AURORA_HIP
    if (hip_matmul_available()) count++;
#endif
    return count > 0 ? count : 0;
}

/* ── Which GPU backend is active ── */
char* gpu_backend() {
#if AURORA_CUDA
    if (cuda_matmul_available()) return AURORA_STRDUP("cuda");
#endif
#if AURORA_HIP
    if (hip_matmul_available()) return AURORA_STRDUP("hip");
#endif
#if AURORA_DML
    if (dml_available()) return AURORA_STRDUP("dml");
#endif
    return AURORA_STRDUP("cpu");
}

} /* extern "C" */
