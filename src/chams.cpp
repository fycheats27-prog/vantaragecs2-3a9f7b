// chams.cpp — añade filtro 254 (ignorar cuerpo) para el preset weapons+gloves.
#include "chams.h"
#include "config.h"
#include "debug.h"
#include <Windows.h>
#include <MinHook.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace Features::Visuals::Chams {

    using DrawIndexedFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, INT);
    using DrawIndexedInstFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT, INT, UINT);

    static const char* kPS_Vis_Mag = "float4 main(float4 p : SV_Position) : SV_Target { return float4(1.00, 0.00, 1.00, 1.0); }";
    static const char* kPS_Occ_Mag = "float4 main(float4 p : SV_Position) : SV_Target { return float4(0.45, 0.00, 0.45, 1.0); }";
    static const char* kPS_Vis_Red = "float4 main(float4 p : SV_Position) : SV_Target { return float4(1.00, 0.18, 0.18, 1.0); }";
    static const char* kPS_Occ_Red = "float4 main(float4 p : SV_Position) : SV_Target { return float4(0.40, 0.04, 0.04, 1.0); }";
    static const char* kPS_Vis_Blue = "float4 main(float4 p : SV_Position) : SV_Target { return float4(0.20, 0.45, 1.00, 1.0); }";
    static const char* kPS_Occ_Blue = "float4 main(float4 p : SV_Position) : SV_Target { return float4(0.06, 0.18, 0.40, 1.0); }";

    static ID3D11Device* g_device = nullptr;
    static ID3D11DeviceContext* g_context = nullptr;
    static ID3D11PixelShader* g_ps_vis[3] = {};
    static ID3D11PixelShader* g_ps_occ[3] = {};
    static ID3D11PixelShader* g_ps_vis_active = nullptr;
    static ID3D11PixelShader* g_ps_occ_active = nullptr;
    static ID3D11DepthStencilState* g_dss_occluded = nullptr;

    static DrawIndexedFn     g_orig_idx = nullptr;
    static DrawIndexedInstFn g_orig_idx_i = nullptr;
    static void* g_idx_addr = nullptr;
    static void* g_idx_i_addr = nullptr;

    static volatile long g_draws_frame = 0;
    static volatile long g_draws_total = 0;
    static volatile long g_painted_idx = 0;
    static volatile long g_painted_idx_i = 0;
    static volatile long g_in_imgui = 0;

    static volatile long g_index_min = 1000;
    static volatile long g_index_max = 300000;
    static volatile long g_inst_min = 1;
    static volatile long g_inst_max = 1;
    static volatile long g_stride_filter = 254;  // 254 = todo menos cuerpo (32)
    static volatile long g_ib_min = 2048;
    static volatile long g_ib_max = 0;
    static volatile long g_through_walls = 1;
    static bool          g_enabled = true;
    static int           g_mode = 0;

    static ID3D11PixelShader* CompilePS(const char* src, ID3D11Device* dev) {
        ID3DBlob* code = nullptr; ID3DBlob* err = nullptr;
        HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr,
            "main", "ps_5_0", 0, 0, &code, &err);
        if (FAILED(hr)) { if (err) err->Release(); return nullptr; }
        ID3D11PixelShader* ps = nullptr;
        hr = dev->CreatePixelShader(code->GetBufferPointer(),
            code->GetBufferSize(), nullptr, &ps);
        code->Release();
        if (err) err->Release();
        return FAILED(hr) ? nullptr : ps;
    }

    static UINT GetVBStride(ID3D11DeviceContext* ctx) {
        ID3D11Buffer* vb = nullptr;
        UINT stride = 0, offset = 0;
        ctx->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
        if (vb) vb->Release();
        return stride;
    }

    static UINT GetIBByteWidth(ID3D11DeviceContext* ctx) {
        ID3D11Buffer* ib = nullptr;
        DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
        UINT offset = 0;
        ctx->IAGetIndexBuffer(&ib, &fmt, &offset);
        if (!ib) return 0;
        D3D11_BUFFER_DESC desc = {};
        ib->GetDesc(&desc);
        ib->Release();
        return desc.ByteWidth;
    }

    static bool ShouldPaint(UINT indexCount, UINT instanceCount, ID3D11DeviceContext* ctx) {
        if (!g_enabled || !g_ps_vis_active) return false;
        if (g_in_imgui) return false;

        UINT imin = (UINT)InterlockedCompareExchange(&g_index_min, 0, 0);
        UINT imax = (UINT)InterlockedCompareExchange(&g_index_max, 0, 0);
        if (indexCount < imin || indexCount > imax) return false;

        UINT stride = GetVBStride(ctx);
        UINT ibw = GetIBByteWidth(ctx);
        if (stride == 0 || ibw == 0) return false;

        UINT filter = (UINT)InterlockedCompareExchange(&g_stride_filter, 0, 0);
        if (filter != 0) {
            if (filter == 255) { // 255: body + weapons (28||32)
                if (stride != 28 && stride != 32) return false;
            }
            else if (filter == 254) { // 254: everything EXCEPT body
                if (stride == 32) return false;
            }
            else {
                if (stride != filter) return false;
            }
        }

        UINT ibmin = (UINT)InterlockedCompareExchange(&g_ib_min, 0, 0);
        if (ibmin != 0 && ibw < ibmin) return false;

        UINT ibmax = (UINT)InterlockedCompareExchange(&g_ib_max, 0, 0);
        if (ibmax != 0 && ibw > ibmax) return false;

        if (instanceCount != 1) {
            UINT instn = (UINT)InterlockedCompareExchange(&g_inst_min, 0, 0);
            UINT instx = (UINT)InterlockedCompareExchange(&g_inst_max, 0, 0);
            if (instanceCount < instn || instanceCount > instx) return false;
        }
        return true;
    }

    template <typename Fn>
    static void DoTwoPass(ID3D11DeviceContext* ctx, Fn&& orig_draw) {
        ID3D11PixelShader* prev_ps = nullptr;
        ID3D11DepthStencilState* prev_dss = nullptr;
        UINT prev_ref = 0;
        ctx->PSGetShader(&prev_ps, nullptr, nullptr);
        ctx->OMGetDepthStencilState(&prev_dss, &prev_ref);

        bool occ = InterlockedCompareExchange(&g_through_walls, 0, 0) != 0;
        if (occ && g_ps_occ_active && g_dss_occluded) {
            ctx->OMSetDepthStencilState(g_dss_occluded, 0);
            ctx->PSSetShader(g_ps_occ_active, nullptr, 0);
            orig_draw();
        }

        ctx->OMSetDepthStencilState(prev_dss, prev_ref);
        ctx->PSSetShader(g_ps_vis_active, nullptr, 0);
        orig_draw();

        ctx->PSSetShader(prev_ps, nullptr, 0);
        ctx->OMSetDepthStencilState(prev_dss, prev_ref);
        if (prev_ps) prev_ps->Release();
        if (prev_dss) prev_dss->Release();
    }

    static void STDMETHODCALLTYPE hk_DrawIndexed(ID3D11DeviceContext* ctx,
        UINT indexCount, UINT startIndex, INT baseVertex) {
        InterlockedIncrement(&g_draws_frame);
        InterlockedIncrement(&g_draws_total);
        if (ShouldPaint(indexCount, 1, ctx)) {
            DoTwoPass(ctx, [&]() { g_orig_idx(ctx, indexCount, startIndex, baseVertex); });
            InterlockedIncrement(&g_painted_idx);
        }
        else {
            g_orig_idx(ctx, indexCount, startIndex, baseVertex);
        }
    }

    static void STDMETHODCALLTYPE hk_DrawIndexedInstanced(ID3D11DeviceContext* ctx,
        UINT indexCountPerInstance, UINT instanceCount,
        UINT startIndex, INT baseVertex, UINT startInstance) {
        InterlockedIncrement(&g_draws_frame);
        InterlockedIncrement(&g_draws_total);
        if (ShouldPaint(indexCountPerInstance, instanceCount, ctx)) {
            DoTwoPass(ctx, [&]() {
                g_orig_idx_i(ctx, indexCountPerInstance, instanceCount, startIndex, baseVertex, startInstance);
                });
            InterlockedIncrement(&g_painted_idx_i);
        }
        else {
            g_orig_idx_i(ctx, indexCountPerInstance, instanceCount, startIndex, baseVertex, startInstance);
        }
    }

    bool Init(ID3D11Device* device, ID3D11DeviceContext* context) {
        g_device = device;
        g_context = context;
        if (!device || !context) return false;

        g_ps_vis[0] = CompilePS(kPS_Vis_Mag, device);
        g_ps_occ[0] = CompilePS(kPS_Occ_Mag, device);
        g_ps_vis[1] = CompilePS(kPS_Vis_Red, device);
        g_ps_occ[1] = CompilePS(kPS_Occ_Red, device);
        g_ps_vis[2] = CompilePS(kPS_Vis_Blue, device);
        g_ps_occ[2] = CompilePS(kPS_Occ_Blue, device);
        if (!g_ps_vis[0] || !g_ps_occ[0]) return false;
        g_ps_vis_active = g_ps_vis[0];
        g_ps_occ_active = g_ps_occ[0];

        D3D11_DEPTH_STENCIL_DESC ds = {};
        ds.DepthEnable = FALSE;
        ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        ds.DepthFunc = D3D11_COMPARISON_ALWAYS;
        ds.StencilEnable = FALSE;
        if (FAILED(device->CreateDepthStencilState(&ds, &g_dss_occluded))) {
            g_dss_occluded = nullptr;
        }

        void** vt = *reinterpret_cast<void***>(context);
        g_idx_addr = vt[12];
        g_idx_i_addr = vt[20];

        MH_STATUS st1 = MH_CreateHook(g_idx_addr, &hk_DrawIndexed, (LPVOID*)&g_orig_idx);
        MH_STATUS st2 = MH_CreateHook(g_idx_i_addr, &hk_DrawIndexedInstanced, (LPVOID*)&g_orig_idx_i);

        if (st1 == MH_OK) MH_EnableHook(g_idx_addr);
        if (st2 == MH_OK) MH_EnableHook(g_idx_i_addr);

        if (st1 != MH_OK && st2 != MH_OK) return false;

        Debug::g_chams.init_done = 1;
        Debug::g_chams.enabled = 1;
        Debug::g_chams.material_ok = 1;
        return true;
    }

    void Shutdown() {
        if (g_idx_addr)   MH_DisableHook(g_idx_addr);
        if (g_idx_i_addr) MH_DisableHook(g_idx_i_addr);
        for (int i = 0; i < 3; ++i) {
            if (g_ps_vis[i]) { g_ps_vis[i]->Release(); g_ps_vis[i] = nullptr; }
            if (g_ps_occ[i]) { g_ps_occ[i]->Release(); g_ps_occ[i] = nullptr; }
        }
        if (g_dss_occluded) { g_dss_occluded->Release(); g_dss_occluded = nullptr; }
    }

    void OnPresent(ID3D11DeviceContext*) {
        Debug::g_chams.good_players = (int)(InterlockedExchange(&g_painted_idx, 0)
            + InterlockedExchange(&g_painted_idx_i, 0));
        Debug::g_chams.write_count = (int)InterlockedExchange(&g_draws_frame, 0);
        Debug::g_chams.good_fires = (int)g_draws_total;
    }

    void SetEnabled(bool on) { g_enabled = on; }
    bool GetEnabled() { return g_enabled; }

    void CycleMode() {
        g_mode = (g_mode + 1) % 3;
        if (g_ps_vis[g_mode]) g_ps_vis_active = g_ps_vis[g_mode];
        if (g_ps_occ[g_mode]) g_ps_occ_active = g_ps_occ[g_mode];
    }
    int CurrentMode() { return g_mode; }

    void SetThroughWalls(bool on) { InterlockedExchange(&g_through_walls, on ? 1 : 0); }
    bool GetThroughWalls() { return InterlockedCompareExchange(&g_through_walls, 0, 0) != 0; }

    void SetInImGuiRender(bool in) { InterlockedExchange(&g_in_imgui, in ? 1 : 0); }

    void SetIndexRange(unsigned mn, unsigned mx) {
        if (mn > mx) { unsigned t = mn; mn = mx; mx = t; }
        InterlockedExchange(&g_index_min, (long)mn);
        InterlockedExchange(&g_index_max, (long)mx);
    }
    unsigned GetIndexMin() { return (unsigned)InterlockedCompareExchange(&g_index_min, 0, 0); }
    unsigned GetIndexMax() { return (unsigned)InterlockedCompareExchange(&g_index_max, 0, 0); }

    void SetInstanceRange(unsigned mn, unsigned mx) {
        if (mn > mx) { unsigned t = mn; mn = mx; mx = t; }
        InterlockedExchange(&g_inst_min, (long)mn);
        InterlockedExchange(&g_inst_max, (long)mx);
    }
    unsigned GetInstanceMin() { return (unsigned)InterlockedCompareExchange(&g_inst_min, 0, 0); }
    unsigned GetInstanceMax() { return (unsigned)InterlockedCompareExchange(&g_inst_max, 0, 0); }

    void SetStrideFilter(unsigned stride) { InterlockedExchange(&g_stride_filter, (long)stride); }
    unsigned GetStrideFilter() { return (unsigned)InterlockedCompareExchange(&g_stride_filter, 0, 0); }

    void SetIBMin(unsigned bytes) { InterlockedExchange(&g_ib_min, (long)bytes); }
    unsigned GetIBMin() { return (unsigned)InterlockedCompareExchange(&g_ib_min, 0, 0); }

    void SetIBMax(unsigned bytes) { InterlockedExchange(&g_ib_max, (long)bytes); }
    unsigned GetIBMax() { return (unsigned)InterlockedCompareExchange(&g_ib_max, 0, 0); }
}