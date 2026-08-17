#pragma once

#if defined(_WIN32)
struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
#endif

namespace dx11_blur {

#if defined(_WIN32)
bool Capture(IDXGISwapChain* swap_chain, ID3D11Device* device, ID3D11DeviceContext* context);
ID3D11ShaderResourceView* Texture();
float SourceWidth();
float SourceHeight();
void ReleaseTargets();
void Shutdown();
#endif

} // namespace dx11_blur
