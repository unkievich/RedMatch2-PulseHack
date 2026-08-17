#include "pulse/plugin.h"

#include "features/esp.h"
#include "ui/blur.h"
#include "ui/pulse_ui.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

#if defined(_WIN32)
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <atomic>
#include <mutex>
#endif

namespace {
ImGuiContext* g_context = nullptr;
bool g_menu_open = true;
float g_menu_alpha = 0.0f;

float EaseInOutCubic(float value) {
    return value < 0.5f
        ? 4.0f * value * value * value
        : 1.0f - std::pow(-2.0f * value + 2.0f, 3.0f) * 0.5f;
}

float AnimateMenuAlpha(float current, bool visible) {
    const float speed = visible ? 10.0f : 13.0f;
    const float step = std::min(1.0f, 1.0f - std::exp(-ImGui::GetIO().DeltaTime * speed));
    current += ((visible ? 1.0f : 0.0f) - current) * step;
    if (current > 0.995f) current = 1.0f;
    if (current < 0.005f) current = 0.0f;
    return current;
}
}

bool RM2Plugin_Initialize(ImGuiContext* context) {
    if (context == nullptr) {
        return false;
    }

    g_context = context;
    ImGui::SetCurrentContext(g_context);
    pulse_ui::Initialize();
    return true;
}

void RM2Plugin_Render() {
    if (g_context == nullptr) {
        return;
    }

    ImGui::SetCurrentContext(g_context);

    if (ImGui::IsKeyPressed(ImGuiKey_Insert, false)) {
        g_menu_open = !g_menu_open;
    }

    esp::Render();
    g_menu_alpha = AnimateMenuAlpha(g_menu_alpha, g_menu_open);
    pulse_ui::Render(&g_menu_open, EaseInOutCubic(g_menu_alpha));
}

void RM2Plugin_Shutdown() {
    esp::Shutdown();
    pulse_ui::Shutdown();
    g_context = nullptr;
}

const char* RM2Plugin_Name() {
    return "Pulse Hack";
}

#if defined(_WIN32)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace {
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);
using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(
    IDXGISwapChain* swap_chain,
    UINT buffer_count,
    UINT width,
    UINT height,
    DXGI_FORMAT new_format,
    UINT swap_chain_flags);

std::atomic_bool g_hooks_installed{false};
std::atomic_bool g_imgui_ready{false};
std::atomic_bool g_unload_requested{false};
std::atomic_bool g_shutdown_started{false};
std::atomic_ulong g_active_hook_calls{0};
std::mutex g_render_mutex;

HMODULE g_module = nullptr;

PresentFn g_original_present = nullptr;
ResizeBuffersFn g_original_resize_buffers = nullptr;
void** g_present_slot = nullptr;
void** g_resize_buffers_slot = nullptr;

HWND g_game_window = nullptr;
WNDPROC g_original_wnd_proc = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_device_context = nullptr;
ID3D11RenderTargetView* g_render_target = nullptr;
std::atomic_bool g_internal_menu_open{true};
float g_internal_menu_alpha = 0.0f;

DWORD WINAPI UnloadThread(LPVOID);

void RequestSelfUnload() {
    if (g_unload_requested.exchange(true)) {
        return;
    }

    HANDLE thread = CreateThread(nullptr, 0, UnloadThread, nullptr, 0, nullptr);
    if (thread == nullptr) {
        g_unload_requested.store(false);
        return;
    }

    CloseHandle(thread);
}

bool IsMouseMessage(UINT msg) {
    switch (msg) {
    case WM_INPUT:
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_NCMOUSEMOVE:
    case WM_NCMOUSELEAVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        return true;
    default:
        return false;
    }
}

void PatchVTableSlot(void** slot, void* replacement, void** original) {
    if (slot == nullptr || replacement == nullptr || original == nullptr) {
        return;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protect)) {
        return;
    }

    *original = *slot;
    *slot = replacement;

    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
}

void RestoreVTableSlot(void** slot, void* original) {
    if (slot == nullptr || original == nullptr) {
        return;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protect)) {
        return;
    }

    *slot = original;

    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
}

void ReleaseRenderTarget() {
    if (g_render_target != nullptr) {
        g_render_target->Release();
        g_render_target = nullptr;
    }
}

bool CreateRenderTarget(IDXGISwapChain* swap_chain) {
    ID3D11Texture2D* back_buffer = nullptr;
    const HRESULT hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr) || back_buffer == nullptr) {
        return false;
    }

    const HRESULT rtv_hr = g_device->CreateRenderTargetView(back_buffer, nullptr, &g_render_target);
    back_buffer->Release();
    return SUCCEEDED(rtv_hr);
}

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // SetWindowLongPtrW can make this callback visible to the window thread
    // before InitializeImGui has finished setting up the Win32 backend.  A
    // context alone is not sufficient: ImGui_ImplWin32_WndProcHandler also
    // dereferences BackendPlatformUserData.
    if (g_internal_menu_open.load(std::memory_order_relaxed) &&
        g_imgui_ready.load(std::memory_order_acquire) &&
        ImGui::GetCurrentContext() != nullptr) {
        const bool handled_by_imgui = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
        if (msg == WM_INPUT) {
            DefWindowProcW(hwnd, msg, wparam, lparam);
            return 0;
        }

        if (handled_by_imgui || IsMouseMessage(msg)) {
            return handled_by_imgui ? TRUE : 0;
        }
    }

    WNDPROC original = g_original_wnd_proc;
    return original != nullptr
        ? CallWindowProcW(original, hwnd, msg, wparam, lparam)
        : DefWindowProcW(hwnd, msg, wparam, lparam);
}

bool InitializeImGui(IDXGISwapChain* swap_chain) {
    if (g_imgui_ready.load()) {
        return true;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap_chain->GetDesc(&desc)) || desc.OutputWindow == nullptr) {
        return false;
    }

    if (FAILED(swap_chain->GetDevice(IID_PPV_ARGS(&g_device))) || g_device == nullptr) {
        return false;
    }

    g_device->GetImmediateContext(&g_device_context);
    if (g_device_context == nullptr || !CreateRenderTarget(swap_chain)) {
        return false;
    }

    g_game_window = desc.OutputWindow;
    g_original_wnd_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_game_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    pulse_ui::Initialize();
    ImGui_ImplWin32_Init(g_game_window);
    ImGui_ImplDX11_Init(g_device, g_device_context);

    // Publish readiness only after both platform and renderer backends are
    // fully initialized.  HookedWndProc uses acquire semantics above.
    g_imgui_ready.store(true, std::memory_order_release);
    return true;
}

void RenderMenu(IDXGISwapChain* swap_chain) {
    std::lock_guard<std::mutex> lock(g_render_mutex);

    if (!InitializeImGui(swap_chain)) {
        return;
    }

    if (g_render_target == nullptr && !CreateRenderTarget(swap_chain)) {
        return;
    }

    if (GetAsyncKeyState(VK_INSERT) & 1) {
        g_internal_menu_open.store(!g_internal_menu_open.load());
    }

    const bool menu_open = g_internal_menu_open.load();
    ImGui::GetIO().MouseDrawCursor = menu_open;
    if (menu_open) {
        ClipCursor(nullptr);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    esp::Render();
    g_internal_menu_alpha = AnimateMenuAlpha(g_internal_menu_alpha, menu_open);
    const float menu_draw_alpha = EaseInOutCubic(g_internal_menu_alpha);
    if (menu_open || g_internal_menu_alpha > 0.01f) {
        if (dx11_blur::Capture(swap_chain, g_device, g_device_context)) {
            pulse_ui::SetBackgroundTexture(dx11_blur::Texture(), dx11_blur::SourceWidth(), dx11_blur::SourceHeight());
        } else {
            pulse_ui::SetBackgroundTexture(nullptr, 0.0f, 0.0f);
        }
        bool keep_open = menu_open;
        pulse_ui::Render(&keep_open, menu_draw_alpha);
        if (menu_open && !keep_open) g_internal_menu_open.store(false);
    }

    ImGui::Render();
    g_device_context->OMSetRenderTargets(1, &g_render_target, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

HRESULT STDMETHODCALLTYPE HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    g_active_hook_calls.fetch_add(1, std::memory_order_acquire);

    if (GetAsyncKeyState(VK_END) & 1) {
        RequestSelfUnload();
    }

    if (!g_unload_requested.load()) {
        RenderMenu(swap_chain);
    }

    const HRESULT result = g_original_present(swap_chain, sync_interval, flags);
    g_active_hook_calls.fetch_sub(1, std::memory_order_release);
    return result;
}

HRESULT STDMETHODCALLTYPE HookedResizeBuffers(
    IDXGISwapChain* swap_chain,
    UINT buffer_count,
    UINT width,
    UINT height,
    DXGI_FORMAT new_format,
    UINT swap_chain_flags) {
    g_active_hook_calls.fetch_add(1, std::memory_order_acquire);

    {
        std::lock_guard<std::mutex> lock(g_render_mutex);
        if (g_imgui_ready.load()) {
            ReleaseRenderTarget();
            pulse_ui::SetBackgroundTexture(nullptr, 0.0f, 0.0f);
            dx11_blur::ReleaseTargets();
            ImGui_ImplDX11_InvalidateDeviceObjects();
        }
    }

    const HRESULT hr = g_original_resize_buffers(
        swap_chain,
        buffer_count,
        width,
        height,
        new_format,
        swap_chain_flags);

    {
        std::lock_guard<std::mutex> lock(g_render_mutex);
        if (SUCCEEDED(hr) && g_imgui_ready.load()) {
            CreateRenderTarget(swap_chain);
            ImGui_ImplDX11_CreateDeviceObjects();
        }
    }

    g_active_hook_calls.fetch_sub(1, std::memory_order_release);
    return hr;
}

LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

bool CreateDummySwapChain(HWND window, IDXGISwapChain** swap_chain, ID3D11Device** device, ID3D11DeviceContext** context) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 1;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL feature_level{};
    return SUCCEEDED(D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        &desc,
        swap_chain,
        device,
        &feature_level,
        context));
}

bool InstallDx11Hooks() {
    if (g_hooks_installed.load()) {
        return true;
    }

    g_shutdown_started.store(false);
    g_unload_requested.store(false);

    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"RM2_DX11_DUMMY_WINDOW";
    RegisterClassExW(&wc);

    HWND dummy_window = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"",
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        100,
        100,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (dummy_window == nullptr) {
        UnregisterClassW(wc.lpszClassName, instance);
        return false;
    }

    IDXGISwapChain* dummy_swap_chain = nullptr;
    ID3D11Device* dummy_device = nullptr;
    ID3D11DeviceContext* dummy_context = nullptr;

    const bool created = CreateDummySwapChain(dummy_window, &dummy_swap_chain, &dummy_device, &dummy_context);
    if (!created) {
        DestroyWindow(dummy_window);
        UnregisterClassW(wc.lpszClassName, instance);
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(dummy_swap_chain);
    g_present_slot = &vtable[8];
    g_resize_buffers_slot = &vtable[13];

    PatchVTableSlot(g_present_slot, reinterpret_cast<void*>(HookedPresent), reinterpret_cast<void**>(&g_original_present));
    PatchVTableSlot(
        g_resize_buffers_slot,
        reinterpret_cast<void*>(HookedResizeBuffers),
        reinterpret_cast<void**>(&g_original_resize_buffers));

    dummy_context->Release();
    dummy_device->Release();
    dummy_swap_chain->Release();
    DestroyWindow(dummy_window);
    UnregisterClassW(wc.lpszClassName, instance);

    const bool installed = g_original_present != nullptr && g_original_resize_buffers != nullptr;
    g_hooks_installed.store(installed);
    return installed;
}

void ShutdownInternalOverlay() {
    if (g_shutdown_started.exchange(true)) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_render_mutex);

    if (g_hooks_installed.load()) {
        RestoreVTableSlot(g_present_slot, reinterpret_cast<void*>(g_original_present));
        RestoreVTableSlot(g_resize_buffers_slot, reinterpret_cast<void*>(g_original_resize_buffers));
        g_hooks_installed.store(false);
    }

    const bool was_imgui_ready = g_imgui_ready.exchange(false, std::memory_order_acq_rel);
    if (was_imgui_ready) {
        esp::Shutdown();
        pulse_ui::Shutdown();
        dx11_blur::Shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    if (g_game_window != nullptr && g_original_wnd_proc != nullptr) {
        SetWindowLongPtrW(g_game_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wnd_proc));
        g_original_wnd_proc = nullptr;
        g_game_window = nullptr;
    }

    ReleaseRenderTarget();

    if (g_device_context != nullptr) {
        g_device_context->Release();
        g_device_context = nullptr;
    }

    if (g_device != nullptr) {
        g_device->Release();
        g_device = nullptr;
    }
}

DWORD WINAPI StartupThread(LPVOID) {
    InstallDx11Hooks();
    return 0;
}

DWORD WINAPI UnloadThread(LPVOID) {
    ShutdownInternalOverlay();

    while (g_active_hook_calls.load(std::memory_order_acquire) != 0) {
        Sleep(1);
    }

    // Allow the hook that observed the zero count to finish returning.
    Sleep(50);

    if (g_module != nullptr) {
        FreeLibraryAndExitThread(g_module, 0);
    }

    return 0;
}
}

bool RM2Overlay_Start() {
    return InstallDx11Hooks();
}

void RM2Overlay_Stop() {
    ShutdownInternalOverlay();
}

bool RM2Overlay_IsRunning() {
    return g_hooks_installed.load();
}

void RM2Overlay_SetVisible(bool visible) {
    g_internal_menu_open.store(visible);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, StartupThread, nullptr, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        ShutdownInternalOverlay();
    }

    return TRUE;
}
#endif
