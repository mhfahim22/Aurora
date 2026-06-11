/* GPU matmul via DirectX 12 compute shader (primary) + DX11 fallback.
 * Pure D3D API, no extra SDKs. Built into Windows 10+.
 * Falls back → CPU OpenMP.
 */
#include "runtime/ai/ai_common.h"
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif

extern "C" {

#ifdef _WIN32

/* ─── HLSL matmul shader ─── */
static const char g_matmul_cs[] =
    "cbuffer Params : register(b0) { uint M, N, K; }\n"
    "StructuredBuffer<float> A : register(t0);\n"
    "StructuredBuffer<float> B : register(t1);\n"
    "RWStructuredBuffer<float> C : register(u0);\n"
    "[numthreads(16,16,1)]\n"
    "void main(uint3 tid : SV_DispatchThreadID) {\n"
    "  uint r = tid.x, c = tid.y;\n"
    "  if (r >= M || c >= N) return;\n"
    "  float sum = 0;\n"
    "  for (uint k = 0; k < K; k++)\n"
    "    sum += A[r * K + k] * B[k * N + c];\n"
    "  C[r * N + c] = sum;\n"
    "}\n";

/* ─── DX12 state ─── */
static struct {
    ID3D12Device* dev;
    ID3D12CommandQueue* q;
    ID3D12CommandAllocator* alloc;
    ID3D12GraphicsCommandList* cl;
    ID3D12RootSignature* rs;
    ID3D12PipelineState* pso;
    ID3D12Fence* fence;
    HANDLE fence_evt;
    UINT64 fence_val;
} g12 = {};

static void dx12_cleanup() {
    if (g12.fence_evt) { CloseHandle(g12.fence_evt); g12.fence_evt = nullptr; }
    if (g12.fence) { g12.fence->Release(); g12.fence = nullptr; }
    if (g12.pso) { g12.pso->Release(); g12.pso = nullptr; }
    if (g12.rs) { g12.rs->Release(); g12.rs = nullptr; }
    if (g12.cl) { g12.cl->Release(); g12.cl = nullptr; }
    if (g12.alloc) { g12.alloc->Release(); g12.alloc = nullptr; }
    if (g12.q) { g12.q->Release(); g12.q = nullptr; }
    if (g12.dev) { g12.dev->Release(); g12.dev = nullptr; }
}

static int dx12_init() {
    if (g12.dev) return 1;
    IDXGIFactory4* f = nullptr;
    ID3DBlob* rsb = nullptr, *csb = nullptr, *err = nullptr;
    int ok = 0;
    do {
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&f)))) break;
        IDXGIAdapter1* a = nullptr; HRESULT hr = E_FAIL;
        for (UINT i = 0; f->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; i++) {
            DXGI_ADAPTER_DESC1 d; a->GetDesc1(&d);
            if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { a->Release(); continue; }
            hr = D3D12CreateDevice(a, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g12.dev));
            a->Release(); if (SUCCEEDED(hr)) break;
        }
        if (FAILED(hr) || !g12.dev) break;
        D3D12_COMMAND_QUEUE_DESC qd = {};
        qd.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        if (FAILED(g12.dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&g12.q)))) break;
        if (FAILED(g12.dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&g12.alloc)))) break;
        if (FAILED(g12.dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, g12.alloc, nullptr, IID_PPV_ARGS(&g12.cl)))) break;
        g12.cl->Close();

        D3D12_ROOT_PARAMETER rp[4] = {};
        rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rp[0].Descriptor.ShaderRegister = 0;
        rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rp[1].Descriptor.ShaderRegister = 0;
        rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rp[2].Descriptor.ShaderRegister = 1;
        rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rp[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        rp[3].Descriptor.ShaderRegister = 0;
        rp[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC rsd = {};
        rsd.NumParameters = 4;
        rsd.pParameters = rp;
        rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &rsb, &err))) break;
        if (FAILED(g12.dev->CreateRootSignature(0, rsb->GetBufferPointer(), rsb->GetBufferSize(), IID_PPV_ARGS(&g12.rs)))) break;

        if (FAILED(D3DCompile(g_matmul_cs, sizeof(g_matmul_cs) - 1, "matmul_cs",
            nullptr, nullptr, "main", "cs_5_0", 0, 0, &csb, &err))) break;
        D3D12_COMPUTE_PIPELINE_STATE_DESC psd = {};
        psd.pRootSignature = g12.rs;
        psd.CS.pShaderBytecode = csb->GetBufferPointer();
        psd.CS.BytecodeLength = csb->GetBufferSize();
        if (FAILED(g12.dev->CreateComputePipelineState(&psd, IID_PPV_ARGS(&g12.pso)))) break;
        if (FAILED(g12.dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g12.fence)))) break;
        g12.fence_evt = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        if (!g12.fence_evt) break;
        ok = 1;
    } while (0);
    if (f) f->Release();
    if (rsb) rsb->Release(); if (csb) csb->Release(); if (err) err->Release();
    if (!ok) { dx12_cleanup(); return 0; }
    return 1;
}

static int dx12_run(const float* A, const float* B, float* C,
                    int64_t M, int64_t N, int64_t K) {
    UINT szA = (UINT)(M * K * 4);
    UINT szB = (UINT)(K * N * 4);
    UINT szC = (UINT)(M * N * 4);
    UINT upSize = 256 + szA + szB;
    D3D12_HEAP_PROPERTIES hpDef = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_HEAP_PROPERTIES hpUp = { D3D12_HEAP_TYPE_UPLOAD };
    D3D12_HEAP_PROPERTIES hpRd = { D3D12_HEAP_TYPE_READBACK };
    D3D12_RESOURCE_DESC bd = {};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bd.Alignment = 0;
    bd.Height = 1;
    bd.DepthOrArraySize = 1;
    bd.MipLevels = 1;
    bd.Format = DXGI_FORMAT_UNKNOWN;
    bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bd.Flags = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource *bufA = nullptr, *bufB = nullptr, *bufC = nullptr;
    ID3D12Resource *up = nullptr, *rd = nullptr;
    int ok = 0;
    void* p = nullptr;

    do {
        bd.Width = szA;
        if (FAILED(g12.dev->CreateCommittedResource(&hpDef, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&bufA)))) break;
        bd.Width = szB;
        if (FAILED(g12.dev->CreateCommittedResource(&hpDef, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&bufB)))) break;
        bd.Width = szC;
        bd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if (FAILED(g12.dev->CreateCommittedResource(&hpDef, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&bufC)))) break;
        bd.Flags = D3D12_RESOURCE_FLAG_NONE;
        bd.Width = upSize;
        if (FAILED(g12.dev->CreateCommittedResource(&hpUp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&up)))) break;
        bd.Width = szC;
        if (FAILED(g12.dev->CreateCommittedResource(&hpRd, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rd)))) break;

        up->Map(0, nullptr, &p);
        struct { UINT m, n, k; } prm = { (UINT)M, (UINT)N, (UINT)K };
        memcpy(p, &prm, sizeof(prm));
        memcpy((char*)p + 256, A, szA);
        memcpy((char*)p + 256 + szA, B, szB);
        up->Unmap(0, nullptr); p = nullptr;

        g12.alloc->Reset();
        g12.cl->Reset(g12.alloc, g12.pso);
        g12.cl->CopyBufferRegion(bufA, 0, up, 256, szA);
        g12.cl->CopyBufferRegion(bufB, 0, up, 256 + szA, szB);

        D3D12_RESOURCE_BARRIER ba[2] = {};
        ba[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        ba[0].Transition.pResource = bufA;
        ba[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        ba[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        ba[1] = ba[0]; ba[1].Transition.pResource = bufB;
        g12.cl->ResourceBarrier(2, ba);

        g12.cl->SetComputeRootSignature(g12.rs);
        g12.cl->SetComputeRootConstantBufferView(0, up->GetGPUVirtualAddress());
        g12.cl->SetComputeRootShaderResourceView(1, bufA->GetGPUVirtualAddress());
        g12.cl->SetComputeRootShaderResourceView(2, bufB->GetGPUVirtualAddress());
        g12.cl->SetComputeRootUnorderedAccessView(3, bufC->GetGPUVirtualAddress());
        g12.cl->Dispatch(((UINT)M + 15) / 16, ((UINT)N + 15) / 16, 1);

        D3D12_RESOURCE_BARRIER bb = {};
        bb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        bb.Transition.pResource = bufC;
        bb.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        bb.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        g12.cl->ResourceBarrier(1, &bb);
        g12.cl->CopyResource(rd, bufC);
        g12.cl->Close();

        ID3D12CommandList* lists[] = { g12.cl };
        g12.q->ExecuteCommandLists(1, lists);
        g12.fence_val++;
        g12.q->Signal(g12.fence, g12.fence_val);
        if (g12.fence->GetCompletedValue() < g12.fence_val) {
            g12.fence->SetEventOnCompletion(g12.fence_val, g12.fence_evt);
            WaitForSingleObject(g12.fence_evt, INFINITE);
        }

        D3D12_RANGE rng = { 0, szC };
        rd->Map(0, &rng, &p);
        if (p) { memcpy(C, p, szC); rd->Unmap(0, nullptr); p = nullptr; }
        ok = 1;
    } while (0);

    if (bufA) bufA->Release(); if (bufB) bufB->Release(); if (bufC) bufC->Release();
    if (up) up->Release(); if (rd) rd->Release();
    return ok;
}

/* ─── DX11 state ─── */
static struct {
    ID3D11Device* dev;
    ID3D11DeviceContext* ctx;
    ID3D11ComputeShader* shader;
} g11 = {};

static void dx11_cleanup() {
    if (g11.shader) { g11.shader->Release(); g11.shader = nullptr; }
    if (g11.ctx) { g11.ctx->Release(); g11.ctx = nullptr; }
    if (g11.dev) { g11.dev->Release(); g11.dev = nullptr; }
}

static int dx11_init() {
    if (g11.dev) return 1;
    ID3DBlob* cs = nullptr, *err = nullptr;
    int ok = 0;
    do {
        D3D_FEATURE_LEVEL lv[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            lv, 2, D3D11_SDK_VERSION, &g11.dev, nullptr, &g11.ctx))) break;
        if (FAILED(D3DCompile(g_matmul_cs, sizeof(g_matmul_cs) - 1, "matmul_cs",
            nullptr, nullptr, "main", "cs_5_0", 0, 0, &cs, &err))) break;
        if (FAILED(g11.dev->CreateComputeShader(cs->GetBufferPointer(), cs->GetBufferSize(),
            nullptr, &g11.shader))) break;
        ok = 1;
    } while (0);
    if (cs) cs->Release(); if (err) err->Release();
    if (!ok) { dx11_cleanup(); return 0; }
    return 1;
}

static ID3D11Buffer* dx11_make_srv(ID3D11Device* d, const float* data, UINT sz) {
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sz; bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = 4;
    D3D11_SUBRESOURCE_DATA sd = { data, 0, 0 };
    ID3D11Buffer* buf = nullptr;
    if (FAILED(d->CreateBuffer(&bd, &sd, &buf))) return nullptr;
    return buf;
}

static int dx11_run(const float* A, const float* B, float* C,
                   int64_t M, int64_t N, int64_t K) {
    UINT szA = (UINT)(M * K * 4), szB = (UINT)(K * N * 4), szC = (UINT)(M * N * 4);

    ID3D11Buffer *bA = nullptr, *bB = nullptr, *bCG = nullptr, *bCS = nullptr;
    ID3D11ShaderResourceView *sA = nullptr, *sB = nullptr;
    ID3D11UnorderedAccessView *uC = nullptr;
    ID3D11Buffer* cb = nullptr;
    int ok = 0;

    do {
        bA = dx11_make_srv(g11.dev, A, szA); if (!bA) break;
        bB = dx11_make_srv(g11.dev, B, szB); if (!bB) break;

        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = szC; bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        bd.StructureByteStride = 0; bd.Usage = D3D11_USAGE_DEFAULT;
        if (FAILED(g11.dev->CreateBuffer(&bd, nullptr, &bCG))) break;

        bd.Usage = D3D11_USAGE_STAGING; bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        bd.BindFlags = 0; bd.MiscFlags = 0;
        if (FAILED(g11.dev->CreateBuffer(&bd, nullptr, &bCS))) break;

        D3D11_SHADER_RESOURCE_VIEW_DESC sv = { DXGI_FORMAT_R32_FLOAT,
            D3D11_SRV_DIMENSION_BUFFER, 0, 0 };
        sv.Buffer.FirstElement = 0; sv.Buffer.NumElements = (UINT)(M * K);
        g11.dev->CreateShaderResourceView(bA, &sv, &sA);
        sv.Buffer.NumElements = (UINT)(K * N);
        g11.dev->CreateShaderResourceView(bB, &sv, &sB);

        D3D11_UNORDERED_ACCESS_VIEW_DESC uv = { DXGI_FORMAT_R32_FLOAT,
            D3D11_UAV_DIMENSION_BUFFER, 0, 0 };
        uv.Buffer.FirstElement = 0; uv.Buffer.NumElements = (UINT)(M * N);
        g11.dev->CreateUnorderedAccessView(bCG, &uv, &uC);
        if (!sA || !sB || !uC) break;

        struct { UINT m, n, k; } prm = { (UINT)M, (UINT)N, (UINT)K };
        D3D11_BUFFER_DESC cbd = {};
        cbd.ByteWidth = sizeof(prm); cbd.Usage = D3D11_USAGE_IMMUTABLE;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA scd = { &prm, 0, 0 };
        if (FAILED(g11.dev->CreateBuffer(&cbd, &scd, &cb))) break;

        g11.ctx->CSSetShader(g11.shader, nullptr, 0);
        g11.ctx->CSSetConstantBuffers(0, 1, &cb);
        ID3D11ShaderResourceView* srvs[] = { sA, sB };
        ID3D11UnorderedAccessView* uavs[] = { uC };
        g11.ctx->CSSetShaderResources(0, 2, srvs);
        g11.ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
        g11.ctx->Dispatch(((UINT)M + 15) / 16, ((UINT)N + 15) / 16, 1);

        g11.ctx->CopyResource(bCS, bCG);
        D3D11_MAPPED_SUBRESOURCE mp;
        if (SUCCEEDED(g11.ctx->Map(bCS, 0, D3D11_MAP_READ, 0, &mp))) {
            memcpy(C, mp.pData, szC);
            g11.ctx->Unmap(bCS, 0);
        }
        ok = 1;
    } while (0);

    ID3D11ShaderResourceView* nsrv[2] = {};
    ID3D11UnorderedAccessView* nuav[1] = {};
    g11.ctx->CSSetShaderResources(0, 2, nsrv);
    g11.ctx->CSSetUnorderedAccessViews(0, 1, nuav, nullptr);

    if (cb) cb->Release();
    if (sA) sA->Release(); if (sB) sB->Release(); if (uC) uC->Release();
    if (bA) bA->Release(); if (bB) bB->Release();
    if (bCG) bCG->Release(); if (bCS) bCS->Release();
    return ok;
}

/* ─── Public API ─── */

int dml_available() {
    return dx12_init() || dx11_init();
}

int dml_matmul(const float* A, const float* B, float* C,
               int64_t M, int64_t N, int64_t K) {
    if (g12.dev && dx12_run(A, B, C, M, N, K)) return 1;
    if (g11.dev && dx11_run(A, B, C, M, N, K)) return 1;
    return 0;
}

#else /* not WIN32 */
int dml_available() { return 0; }
int dml_matmul(const float* A, const float* B, float* C,
               int64_t M, int64_t N, int64_t K) {
    (void)A; (void)B; (void)C; (void)M; (void)N; (void)K;
    return 0;
}
#endif

} /* extern "C" */
