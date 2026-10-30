#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "render/renderer.hpp"
#include <DirectXMath.h>
#include <Windows.h>
#include <chrono>
#include <cstdlib>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
using namespace DirectX;
namespace outpost {
namespace {
void require(HRESULT hr) {
    if (FAILED(hr))
        throw std::runtime_error("DX11 operation failed");
}
struct Vertex {
    float x, y, z;
};
struct Constants {
    XMMATRIX transform;
    XMFLOAT4 color;
};
const char *shader =
    R"(
struct Item {matrix transform;float4 color;};
#ifdef INSTANCED
cbuffer Scene:register(b0){Item items[32];}
#else
cbuffer Scene:register(b0){Item items[1];}
#endif
struct V{float4 position:SV_POSITION;float3 local:TEXCOORD0;float4 color:COLOR0;};
V vs(float3 p:POSITION,uint instance:SV_InstanceID){V o;o.position=mul(float4(p,1),items[instance].transform);o.local=p;o.color=items[instance].color;return o;}
float4 ps(V i):SV_TARGET{float3 n=normalize(cross(ddx(i.local),ddy(i.local)));float light=0.35+0.65*abs(dot(n,normalize(float3(0.4,1,0.6))));return float4(i.color.rgb*light,1);})";
} // namespace
struct Renderer::State {
    HWND window;
    UINT width{}, height{};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11DepthStencilView> depth;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> vertices, constants;
    ComPtr<ID3D11RasterizerState> raster;
    bool instanced{}, measure{};
    ComPtr<ID3D11Query> disjoint, start, end;
    void resize(UINT w, UINT h) {
        context->OMSetRenderTargets(0, nullptr, nullptr);
        target.Reset();
        depth.Reset();
        require(swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0));
        ComPtr<ID3D11Texture2D> back;
        require(swap->GetBuffer(0, IID_PPV_ARGS(&back)));
        require(device->CreateRenderTargetView(back.Get(), nullptr, &target));
        D3D11_TEXTURE2D_DESC td{};
        td.Width = w;
        td.Height = h;
        td.MipLevels = td.ArraySize = 1;
        td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        td.SampleDesc.Count = 1;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ComPtr<ID3D11Texture2D> texture;
        require(device->CreateTexture2D(&td, nullptr, &texture));
        require(device->CreateDepthStencilView(texture.Get(), nullptr, &depth));
        width = w;
        height = h;
    }
};
Renderer::Renderer(HWND__ *window, bool instanced, bool measure) : state_(std::make_unique<State>()) {
    auto &s = *state_;
    s.window = window;
    s.instanced = instanced;
    s.measure = measure;
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = window;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    HRESULT hr =
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                      D3D11_SDK_VERSION, &sd, &s.swap, &s.device, nullptr, &s.context);
    if (FAILED(hr))
        require(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                                              D3D11_SDK_VERSION, &sd, &s.swap, &s.device, nullptr,
                                              &s.context));
    if (measure) {
        ComPtr<IDXGIDevice> dxgi;
        ComPtr<IDXGIAdapter> adapter;
        require(s.device.As(&dxgi));
        require(dxgi->GetAdapter(&adapter));
        DXGI_ADAPTER_DESC desc{};
        require(adapter->GetDesc(&desc));
        char name[256]{};
        WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr, nullptr);
        std::cout << "ADAPTER " << name << " vendor=" << desc.VendorId << " device=" << desc.DeviceId << '\n';
    }
    ComPtr<ID3DBlob> v, p, error;
    D3D_SHADER_MACRO macros[] = {{"INSTANCED", "1"}, {nullptr, nullptr}};
    require(D3DCompile(shader, strlen(shader), nullptr, instanced ? macros : nullptr, nullptr, "vs", "vs_4_0",
                       0, 0, &v, &error));
    require(D3DCompile(shader, strlen(shader), nullptr, instanced ? macros : nullptr, nullptr, "ps", "ps_4_0",
                       0, 0, &p, &error));
    require(s.device->CreateVertexShader(v->GetBufferPointer(), v->GetBufferSize(), nullptr, &s.vs));
    require(s.device->CreatePixelShader(p->GetBufferPointer(), p->GetBufferSize(), nullptr, &s.ps));
    D3D11_INPUT_ELEMENT_DESC element{
        "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
    require(s.device->CreateInputLayout(&element, 1, v->GetBufferPointer(), v->GetBufferSize(), &s.layout));
    const Vertex corners[] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                              {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
    const int indices[] = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                           3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};
    Vertex mesh[36];
    for (int i = 0; i < 36; ++i)
        mesh[i] = corners[indices[i]];
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(mesh);
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA data{mesh, 0, 0};
    require(s.device->CreateBuffer(&bd, &data, &s.vertices));
    bd.ByteWidth = sizeof(Constants) * (instanced ? 32 : 1);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    require(s.device->CreateBuffer(&bd, nullptr, &s.constants));
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    require(s.device->CreateRasterizerState(&rd, &s.raster));
    if (measure) {
        D3D11_QUERY_DESC query{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
        require(s.device->CreateQuery(&query, &s.disjoint));
        query.Query = D3D11_QUERY_TIMESTAMP;
        require(s.device->CreateQuery(&query, &s.start));
        require(s.device->CreateQuery(&query, &s.end));
    }
}
Renderer::~Renderer() = default;
Renderer::Metrics Renderer::draw(const World &world, int local_slot) {
    auto &s = *state_;
    RECT rect{};
    GetClientRect(s.window, &rect);
    UINT w = rect.right, h = rect.bottom;
    if (!w || !h)
        return {};
    if (w != s.width || h != s.height)
        s.resize(w, h);
    Metrics metrics;
    auto cpu_start = std::chrono::steady_clock::now();
    if (s.measure) {
        s.context->Begin(s.disjoint.Get());
        s.context->End(s.start.Get());
    }
    std::array<Constants, 32> instances{};
    unsigned instance_count = 0;
    float background[] = {0.025f, 0.035f, 0.07f, 1};
    s.context->ClearRenderTargetView(s.target.Get(), background);
    s.context->ClearDepthStencilView(s.depth.Get(), D3D11_CLEAR_DEPTH, 1, 0);
    ID3D11RenderTargetView *target = s.target.Get();
    s.context->OMSetRenderTargets(1, &target, s.depth.Get());
    D3D11_VIEWPORT viewport{0, 0, float(w), float(h), 0, 1};
    s.context->RSSetViewports(1, &viewport);
    s.context->RSSetState(s.raster.Get());
    UINT stride = sizeof(Vertex), offset = 0;
    ID3D11Buffer *vb = s.vertices.Get();
    s.context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    s.context->IASetInputLayout(s.layout.Get());
    s.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    s.context->VSSetShader(s.vs.Get(), nullptr, 0);
    s.context->PSSetShader(s.ps.Get(), nullptr, 0);
    ID3D11Buffer *cb = s.constants.Get();
    s.context->VSSetConstantBuffers(0, 1, &cb);
    s.context->PSSetConstantBuffers(0, 1, &cb);
    auto view = XMMatrixLookAtLH(XMVectorSet(0, 30, 32, 1), XMVectorZero(), XMVectorSet(0, 1, 0, 0));
    auto projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, float(w) / h, 0.1f, 150);
    auto cube = [&](float x, float y, float z, float sx, float sy, float sz, XMFLOAT4 color) {
        Constants c{
            XMMatrixTranspose(XMMatrixScaling(sx, sy, sz) * XMMatrixTranslation(x, y, z) * view * projection),
            color};
        if (s.instanced) {
            if (instance_count == instances.size())
                throw std::runtime_error("instance capacity");
            instances[instance_count++] = c;
        } else {
            s.context->UpdateSubresource(cb, 0, nullptr, &c, 0, 0);
            s.context->Draw(36, 0);
            ++metrics.draws;
        }
    };
    cube(0, -0.4f, 0, 20, 0.2f, 20, {0.08f, 0.12f, 0.17f, 1});
    cube(0, 0.8f, 0, 1.2f, 1.2f, 1.2f, {0.2f, world.core_hp / 100, 0.35f, 1});
    for (int i = 0; i < 4; ++i)
        if (world.match.slots()[i].kind != SlotKind::empty) {
            auto p = world.drones[i].position;
            cube(p.x, 0.8f, p.z, 0.55f, 0.25f, 0.55f,
                 i == local_slot ? XMFLOAT4{0.2f, 1, 1, 1} : XMFLOAT4{0.3f, 0.45f, 1, 1});
        }
    for (const auto &e : world.enemies)
        cube(e.position.x, 0.6f, e.position.z, e.kind ? 0.7f : 0.4f, 0.6f, e.kind ? 0.7f : 0.4f,
             e.kind ? XMFLOAT4{1, 0.5f, 0.1f, 1} : XMFLOAT4{1, 0.15f, 0.2f, 1});
    if (s.instanced) {
        s.context->UpdateSubresource(cb, 0, nullptr, instances.data(), 0, 0);
        s.context->DrawInstanced(36, instance_count, 0, 0);
        ++metrics.draws;
    }
    if (s.measure) {
        s.context->End(s.end.Get());
        s.context->End(s.disjoint.Get());
    }
    metrics.cpu_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cpu_start).count();
    if (s.measure) {
        s.context->Flush();
        auto wait = [&](ID3D11Query *query, void *data, UINT size) {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            HRESULT hr;
            while ((hr = s.context->GetData(query, data, size, 0)) == S_FALSE) {
                if (std::chrono::steady_clock::now() > deadline)
                    throw std::runtime_error("GPU query timeout");
                Sleep(0);
            }
            require(hr);
        };
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timing{};
        UINT64 start{}, end{};
        wait(s.disjoint.Get(), &timing, sizeof(timing));
        wait(s.start.Get(), &start, sizeof(start));
        wait(s.end.Get(), &end, sizeof(end));
        metrics.gpu_valid = !timing.Disjoint && timing.Frequency > 0;
        if (metrics.gpu_valid)
            metrics.gpu_ms = double(end - start) * 1000 / timing.Frequency;
    }
    static bool captured = false;
    static unsigned capture_frame = 0;
    static auto capture_at = std::chrono::steady_clock::time_point{};
    const char *frames = std::getenv("OUTPOST_FRAMES");
    bool sequence_capture = frames && std::chrono::steady_clock::now() >= capture_at;
    if (sequence_capture || (!captured && world.tick >= 60 && std::getenv("OUTPOST_CAPTURE"))) {
        ComPtr<ID3D11Texture2D> back, staging;
        require(s.swap->GetBuffer(0, IID_PPV_ARGS(&back)));
        D3D11_TEXTURE2D_DESC desc{};
        back->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        require(s.device->CreateTexture2D(&desc, nullptr, &staging));
        s.context->CopyResource(staging.Get(), back.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        require(s.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
        BITMAPFILEHEADER file{};
        file.bfType = 0x4d42;
        file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
        file.bfSize = file.bfOffBits + w * h * 4;
        BITMAPINFOHEADER info{};
        info.biSize = sizeof(info);
        info.biWidth = w;
        info.biHeight = -static_cast<LONG>(h);
        info.biPlanes = 1;
        info.biBitCount = 32;
        info.biCompression = BI_RGB;
        char path[MAX_PATH]{};
        if (sequence_capture) {
            snprintf(path, sizeof(path), "%s/frame-%04u.bmp", frames, capture_frame++);
            capture_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        }
        std::ofstream output(sequence_capture ? path : std::getenv("OUTPOST_CAPTURE"), std::ios::binary);
        output.write(reinterpret_cast<char *>(&file), sizeof(file));
        output.write(reinterpret_cast<char *>(&info), sizeof(info));
        std::vector<char> row(w * 4);
        for (UINT y = 0; y < h; ++y) {
            for (UINT x = 0; x < w; ++x) {
                auto pixel = static_cast<const unsigned char *>(mapped.pData) + y * mapped.RowPitch + x * 4;
                row[x * 4] = char(pixel[2]);
                row[x * 4 + 1] = char(pixel[1]);
                row[x * 4 + 2] = char(pixel[0]);
                row[x * 4 + 3] = char(pixel[3]);
            }
            output.write(row.data(), row.size());
        }
        s.context->Unmap(staging.Get(), 0);
        if (!output)
            throw std::runtime_error("Framebuffer capture write failed");
        captured = true;
    }
    require(s.swap->Present(s.measure ? 0 : 1, 0));
    return metrics;
}
} // namespace outpost
