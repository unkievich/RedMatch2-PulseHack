#pragma once

struct ImGuiContext;

#if defined(_WIN32)
    #if defined(RM2_PLUGIN_BUILD)
        #define RM2_API extern "C" __declspec(dllexport)
    #else
        #define RM2_API extern "C" __declspec(dllimport)
    #endif
#else
    #define RM2_API extern "C"
#endif

// The host owns the ImGui context and frame lifecycle.
RM2_API bool RM2Plugin_Initialize(ImGuiContext* context);
RM2_API void RM2Plugin_Render();
RM2_API void RM2Plugin_Shutdown();
RM2_API const char* RM2Plugin_Name();

#if defined(_WIN32)
// Installs the internal DX11 Present/ResizeBuffers hooks.
// DllMain calls this automatically after the DLL is loaded.
RM2_API bool RM2Overlay_Start();
RM2_API void RM2Overlay_Stop();
RM2_API bool RM2Overlay_IsRunning();
RM2_API void RM2Overlay_SetVisible(bool visible);
#endif
