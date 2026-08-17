#include "ui/blur.h"

#if defined(_WIN32)
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace dx11_blur {
namespace {
constexpr UINT kBlurScale = 4;
constexpr int kBlurIterations = 2;
constexpr float kBlurStrength = 1.45f;

struct BlurConstants {
    float offset_x;
    float offset_y;
    float padding[2];
};

ID3D11Texture2D* g_capture_texture = nullptr;
ID3D11ShaderResourceView* g_capture_srv = nullptr;
ID3D11Texture2D* g_ping_texture = nullptr;
ID3D11ShaderResourceView* g_ping_srv = nullptr;
ID3D11RenderTargetView* g_ping_rtv = nullptr;
ID3D11Texture2D* g_pong_texture = nullptr;
ID3D11ShaderResourceView* g_pong_srv = nullptr;
ID3D11RenderTargetView* g_pong_rtv = nullptr;

ID3D11VertexShader* g_vertex_shader = nullptr;
ID3D11PixelShader* g_pixel_shader = nullptr;
ID3D11SamplerState* g_sampler = nullptr;
ID3D11Buffer* g_constant_buffer = nullptr;
ID3D11RasterizerState* g_rasterizer_state = nullptr;
ID3D11DepthStencilState* g_depth_stencil_state = nullptr;
ID3D11BlendState* g_blend_state = nullptr;

UINT g_source_width = 0;
UINT g_source_height = 0;
UINT g_blur_width = 0;
UINT g_blur_height = 0;
DXGI_FORMAT g_format = DXGI_FORMAT_UNKNOWN;

template <typename T>
void SafeRelease(T*& object) {
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

void ReleasePipelineResources() {
    SafeRelease(g_blend_state);
    SafeRelease(g_depth_stencil_state);
    SafeRelease(g_rasterizer_state);
    SafeRelease(g_constant_buffer);
    SafeRelease(g_sampler);
    SafeRelease(g_pixel_shader);
    SafeRelease(g_vertex_shader);
}

void ReleaseTargetResources() {
    SafeRelease(g_capture_srv);
    SafeRelease(g_capture_texture);
    SafeRelease(g_ping_rtv);
    SafeRelease(g_ping_srv);
    SafeRelease(g_ping_texture);
    SafeRelease(g_pong_rtv);
    SafeRelease(g_pong_srv);
    SafeRelease(g_pong_texture);
    g_source_width = 0;
    g_source_height = 0;
    g_blur_width = 0;
    g_blur_height = 0;
    g_format = DXGI_FORMAT_UNKNOWN;
}

bool CompileShader(const char* source, const char* entry, const char* profile, ID3DBlob** bytecode) {
    ID3DBlob* errors = nullptr;
    const HRESULT hr = D3DCompile(source, std::strlen(source), "pulse_dx11_blur", nullptr, nullptr,
        entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, bytecode, &errors);
    SafeRelease(errors);
    return SUCCEEDED(hr) && *bytecode != nullptr;
}

bool EnsurePipeline(ID3D11Device* device) {
    if (g_vertex_shader != nullptr && g_pixel_shader != nullptr && g_sampler != nullptr && g_constant_buffer != nullptr &&
        g_rasterizer_state != nullptr && g_depth_stencil_state != nullptr && g_blend_state != nullptr) {
        return true;
    }
    ReleasePipelineResources();

    static constexpr const char* shader_source = R"(
Texture2D source_texture : register(t0);
SamplerState linear_sampler : register(s0);

cbuffer BlurConstants : register(b0) {
    float2 blur_offset;
    float2 padding;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput vs_main(uint vertex_id : SV_VertexID) {
    VSOutput output;
    output.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float4 ps_main(VSOutput input) : SV_Target {
    float4 color = source_texture.Sample(linear_sampler, input.uv) * 0.2270270270;
    color += source_texture.Sample(linear_sampler, input.uv + blur_offset * 1.3846153846) * 0.3162162162;
    color += source_texture.Sample(linear_sampler, input.uv - blur_offset * 1.3846153846) * 0.3162162162;
    color += source_texture.Sample(linear_sampler, input.uv + blur_offset * 3.2307692308) * 0.0702702703;
    color += source_texture.Sample(linear_sampler, input.uv - blur_offset * 3.2307692308) * 0.0702702703;
    return color;
}
)";

    ID3DBlob* vertex_bytecode = nullptr;
    ID3DBlob* pixel_bytecode = nullptr;
    if (!CompileShader(shader_source, "vs_main", "vs_4_0", &vertex_bytecode) ||
        !CompileShader(shader_source, "ps_main", "ps_4_0", &pixel_bytecode)) {
        SafeRelease(vertex_bytecode);
        SafeRelease(pixel_bytecode);
        return false;
    }

    HRESULT hr = device->CreateVertexShader(vertex_bytecode->GetBufferPointer(), vertex_bytecode->GetBufferSize(), nullptr, &g_vertex_shader);
    if (SUCCEEDED(hr)) {
        hr = device->CreatePixelShader(pixel_bytecode->GetBufferPointer(), pixel_bytecode->GetBufferSize(), nullptr, &g_pixel_shader);
    }
    SafeRelease(vertex_bytecode);
    SafeRelease(pixel_bytecode);
    if (FAILED(hr)) {
        return false;
    }

    D3D11_SAMPLER_DESC sampler_desc{};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampler_desc.MinLOD = 0.0f;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&sampler_desc, &g_sampler))) {
        return false;
    }

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(BlurConstants);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(device->CreateBuffer(&buffer_desc, nullptr, &g_constant_buffer))) {
        return false;
    }

    D3D11_RASTERIZER_DESC rasterizer_desc{};
    rasterizer_desc.FillMode = D3D11_FILL_SOLID;
    rasterizer_desc.CullMode = D3D11_CULL_NONE;
    rasterizer_desc.DepthClipEnable = TRUE;
    if (FAILED(device->CreateRasterizerState(&rasterizer_desc, &g_rasterizer_state))) {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depth_desc{};
    depth_desc.DepthEnable = FALSE;
    depth_desc.StencilEnable = FALSE;
    if (FAILED(device->CreateDepthStencilState(&depth_desc, &g_depth_stencil_state))) {
        return false;
    }

    D3D11_BLEND_DESC blend_desc{};
    blend_desc.RenderTarget[0].BlendEnable = FALSE;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    return SUCCEEDED(device->CreateBlendState(&blend_desc, &g_blend_state));
}

bool CreateTextureSet(ID3D11Device* device, UINT source_width, UINT source_height, DXGI_FORMAT format) {
    const UINT blur_width = std::max(1u, (source_width + kBlurScale - 1u) / kBlurScale);
    const UINT blur_height = std::max(1u, (source_height + kBlurScale - 1u) / kBlurScale);
    if (g_capture_texture != nullptr && g_source_width == source_width && g_source_height == source_height &&
        g_blur_width == blur_width && g_blur_height == blur_height && g_format == format) {
        return true;
    }

    ReleaseTargetResources();

    D3D11_TEXTURE2D_DESC capture_desc{};
    capture_desc.Width = source_width;
    capture_desc.Height = source_height;
    capture_desc.MipLevels = 1;
    capture_desc.ArraySize = 1;
    capture_desc.Format = format;
    capture_desc.SampleDesc.Count = 1;
    capture_desc.Usage = D3D11_USAGE_DEFAULT;
    capture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&capture_desc, nullptr, &g_capture_texture)) ||
        FAILED(device->CreateShaderResourceView(g_capture_texture, nullptr, &g_capture_srv))) {
        ReleaseTargetResources();
        return false;
    }

    D3D11_TEXTURE2D_DESC blur_desc = capture_desc;
    blur_desc.Width = blur_width;
    blur_desc.Height = blur_height;
    blur_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    const bool created =
        SUCCEEDED(device->CreateTexture2D(&blur_desc, nullptr, &g_ping_texture)) &&
        SUCCEEDED(device->CreateShaderResourceView(g_ping_texture, nullptr, &g_ping_srv)) &&
        SUCCEEDED(device->CreateRenderTargetView(g_ping_texture, nullptr, &g_ping_rtv)) &&
        SUCCEEDED(device->CreateTexture2D(&blur_desc, nullptr, &g_pong_texture)) &&
        SUCCEEDED(device->CreateShaderResourceView(g_pong_texture, nullptr, &g_pong_srv)) &&
        SUCCEEDED(device->CreateRenderTargetView(g_pong_texture, nullptr, &g_pong_rtv));
    if (!created) {
        ReleaseTargetResources();
        return false;
    }

    g_source_width = source_width;
    g_source_height = source_height;
    g_blur_width = blur_width;
    g_blur_height = blur_height;
    g_format = format;
    return true;
}

void RenderPass(ID3D11DeviceContext* context, ID3D11ShaderResourceView* source, ID3D11RenderTargetView* target,
                float offset_x, float offset_y) {
    ID3D11ShaderResourceView* null_srv = nullptr;
    context->PSSetShaderResources(0, 1, &null_srv);
    context->OMSetRenderTargets(1, &target, nullptr);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(g_blur_width);
    viewport.Height = static_cast<float>(g_blur_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);
    context->RSSetState(g_rasterizer_state);
    context->OMSetDepthStencilState(g_depth_stencil_state, 0);
    constexpr float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context->OMSetBlendState(g_blend_state, blend_factor, 0xFFFFFFFFu);

    const BlurConstants constants{offset_x, offset_y, {0.0f, 0.0f}};
    context->UpdateSubresource(g_constant_buffer, 0, nullptr, &constants, 0, 0);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(g_vertex_shader, nullptr, 0);
    context->PSSetShader(g_pixel_shader, nullptr, 0);
    context->PSSetSamplers(0, 1, &g_sampler);
    context->PSSetConstantBuffers(0, 1, &g_constant_buffer);
    context->PSSetShaderResources(0, 1, &source);
    context->Draw(3, 0);
    context->PSSetShaderResources(0, 1, &null_srv);
}
} // namespace

bool Capture(IDXGISwapChain* swap_chain, ID3D11Device* device, ID3D11DeviceContext* context) {
    if (swap_chain == nullptr || device == nullptr || context == nullptr || !EnsurePipeline(device)) {
        return false;
    }

    ID3D11Texture2D* back_buffer = nullptr;
    if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))) || back_buffer == nullptr) {
        return false;
    }

    D3D11_TEXTURE2D_DESC back_desc{};
    back_buffer->GetDesc(&back_desc);
    if (!CreateTextureSet(device, back_desc.Width, back_desc.Height, back_desc.Format)) {
        back_buffer->Release();
        return false;
    }

    if (back_desc.SampleDesc.Count > 1) {
        context->ResolveSubresource(g_capture_texture, 0, back_buffer, 0, back_desc.Format);
    } else {
        context->CopyResource(g_capture_texture, back_buffer);
    }
    back_buffer->Release();

    RenderPass(context, g_capture_srv, g_ping_rtv, kBlurStrength / static_cast<float>(g_source_width), 0.0f);
    RenderPass(context, g_ping_srv, g_pong_rtv, 0.0f, kBlurStrength / static_cast<float>(g_blur_height));
    for (int i = 1; i < kBlurIterations; ++i) {
        RenderPass(context, g_pong_srv, g_ping_rtv, kBlurStrength / static_cast<float>(g_blur_width), 0.0f);
        RenderPass(context, g_ping_srv, g_pong_rtv, 0.0f, kBlurStrength / static_cast<float>(g_blur_height));
    }
    context->OMSetRenderTargets(0, nullptr, nullptr);
    return true;
}

ID3D11ShaderResourceView* Texture() {
    return g_pong_srv;
}

float SourceWidth() {
    return static_cast<float>(g_source_width);
}

float SourceHeight() {
    return static_cast<float>(g_source_height);
}

void ReleaseTargets() {
    ReleaseTargetResources();
}

void Shutdown() {
    ReleaseTargetResources();
    ReleasePipelineResources();
}

} // namespace dx11_blur
#endif
