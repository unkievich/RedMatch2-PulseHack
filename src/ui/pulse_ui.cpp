#include "ui/pulse_ui.h"

#include "features/aimbot.h"
#include "features/esp.h"
#include "features/movement.h"
#include "ui/assets/font/fonts.h"
#include "ui/assets/font/icons.h"
#include "ui/assets/font/mingcute_icon_font.h"
#include "ui/assets/logo/pulse_logo.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>
#include <unordered_map>

namespace pulse_ui {
namespace {
constexpr float kMenuRounding = 11.0f;
constexpr float kPanelRounding = 8.5f;
constexpr float kTabRounding = 8.0f;
constexpr float kRowHeight = 26.0f;
constexpr float kToggleWidth = 28.0f;
constexpr float kRowRightInset = 2.0f;
constexpr ImVec4 kAccent(0.0f, 0.56862748f, 1.0f, 1.0f);

ImGuiContext* g_initialized_context = nullptr;
ImFont* g_font_main = nullptr;
ImFont* g_font_logo = nullptr;
ImFont* g_font_small = nullptr;
ImFont* g_font_icons = nullptr;
ImFont* g_font_mingcute = nullptr;
void* g_background_texture = nullptr;
float g_background_width = 0.0f;
float g_background_height = 0.0f;
bool g_was_open = false;
std::unordered_map<std::string, float> g_animations;

float EaseOutCubic(float t) {
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float EaseInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

float Animate(const std::string& key, bool enabled, float speed) {
    float& value = g_animations[key];
    const float step = 1.0f - std::exp(-ImGui::GetIO().DeltaTime * speed);
    value = ImLerp(value, enabled ? 1.0f : 0.0f, step);
    if (value > 0.995f) value = 1.0f;
    if (value < 0.005f) value = 0.0f;
    return value;
}

int AlphaByte(float opacity) {
    return static_cast<int>(255.0f * std::clamp(opacity * ImGui::GetStyle().Alpha, 0.0f, 1.0f));
}

ImU32 GlassColor(int r, int g, int b, float opacity) {
    return IM_COL32(r, g, b, AlphaByte(opacity));
}

void DrawBlurRegion(ImDrawList* draw, const ImVec2& min, const ImVec2& max, float opacity,
                    float rounding, ImDrawFlags flags) {
    if (g_background_texture == nullptr || g_background_width <= 0.0f || g_background_height <= 0.0f) {
        return;
    }

    const ImVec2 uv_min(
        std::clamp(min.x / g_background_width, 0.0f, 1.0f),
        std::clamp(min.y / g_background_height, 0.0f, 1.0f));
    const ImVec2 uv_max(
        std::clamp(max.x / g_background_width, 0.0f, 1.0f),
        std::clamp(max.y / g_background_height, 0.0f, 1.0f));
    const auto texture_id = (ImTextureID)(std::intptr_t)g_background_texture;
    draw->AddImageRounded(texture_id, min, max, uv_min, uv_max,
        IM_COL32(255, 255, 255, AlphaByte(opacity)), rounding, flags);
}

void FrostedRect(ImDrawList* draw, const ImVec2& min, const ImVec2& max, int r, int g, int b,
                 float opacity, float rounding, ImDrawFlags flags, float blur_opacity = 0.06f) {
    DrawBlurRegion(draw, min, max, blur_opacity, rounding, flags);
    draw->AddRectFilled(min, max, GlassColor(r, g, b, opacity), rounding, flags);
}

void DrawMenuBackdrop(ImDrawList* draw, const ImVec2& min, const ImVec2& max) {
    const ImVec2 sidebar_max(min.x + 220.0f, max.y);
    const ImVec2 content_min(min.x + 220.0f, min.y);
    DrawBlurRegion(draw, min, max, 0.18f, kMenuRounding, ImDrawFlags_RoundCornersAll);
    draw->AddRectFilled(min, max, GlassColor(11, 12, 16, 0.74f), kMenuRounding, ImDrawFlags_RoundCornersAll);
    DrawBlurRegion(draw, content_min, max, 0.24f, kMenuRounding, ImDrawFlags_RoundCornersRight);
    draw->AddRectFilled(content_min, max, GlassColor(11, 12, 16, 0.84f), kMenuRounding, ImDrawFlags_RoundCornersRight);
    DrawBlurRegion(draw, min, sidebar_max, 0.14f, kMenuRounding, ImDrawFlags_RoundCornersLeft);
    draw->AddRectFilled(min, sidebar_max, GlassColor(18, 19, 24, 0.92f), kMenuRounding, ImDrawFlags_RoundCornersLeft);
    draw->AddLine(ImVec2(sidebar_max.x, min.y), ImVec2(sidebar_max.x, max.y), GlassColor(45, 47, 56, 0.42f));
    draw->AddRect(min, max, GlassColor(48, 50, 60, 0.55f), kMenuRounding);
}

bool NavTab(const char* icon, const char* label, bool selected) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    constexpr float width = 190.0f;
    constexpr float height = 36.0f;
    const bool clicked = ImGui::InvisibleButton(label, ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    const float alpha = ImGui::GetStyle().Alpha;
    const float hover_t = EaseOutCubic(Animate(std::string("hover::nav::") + label, hovered, 16.0f));
    const float selected_t = EaseOutCubic(Animate(std::string("state::nav::") + label, selected, 15.0f));

    FrostedRect(draw, p, ImVec2(p.x + width, p.y + height), 26, 28, 35,
        0.18f + 0.54f * selected_t, kTabRounding, ImDrawFlags_RoundCornersAll, 0.10f * selected_t);
    if (hover_t > 0.001f) {
        FrostedRect(draw, p, ImVec2(p.x + width, p.y + height), 40, 42, 51, 0.18f * hover_t, kTabRounding, ImDrawFlags_RoundCornersAll, 0.08f * hover_t);
    }
    if (selected_t > 0.001f) {
        draw->AddRectFilled(ImVec2(p.x, p.y + 8.0f), ImVec2(p.x + 2.0f, p.y + height - 8.0f),
            ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, alpha * selected_t)), 2.0f);
    }

    if (g_font_mingcute != nullptr) {
        const ImVec2 icon_size = g_font_mingcute->CalcTextSizeA(g_font_mingcute->FontSize, FLT_MAX, 0.0f, icon);
        draw->AddText(g_font_mingcute, g_font_mingcute->FontSize,
            ImVec2(p.x + 14.0f, p.y + (height - icon_size.y) * 0.5f),
            ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, alpha * (0.55f + 0.45f * selected_t))), icon);
    }
    const ImVec2 text_size = g_font_main->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, label);
    draw->AddText(g_font_main, 14.0f, ImVec2(p.x + 42.0f, p.y + (height - text_size.y) * 0.5f),
        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha)), label);
    return clicked;
}

bool ToggleButton(const char* id, bool* value) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    constexpr float height = 16.0f;
    constexpr float width = 28.0f;
    constexpr float radius = height * 0.5f - 2.0f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImGuiID widget_id = ImGui::GetID(id);
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    if (pressed) *value = !*value;

    const std::string animation_id = std::to_string(widget_id);
    const float t = EaseInOutCubic(Animate("state::toggle::" + animation_id, *value, 14.0f));
    const float hover_t = EaseOutCubic(Animate("hover::toggle::" + animation_id, hovered, 16.0f));
    const float alpha = ImGui::GetStyle().Alpha;
    const ImVec4 off = ImLerp(ImVec4(40 / 255.0f, 42 / 255.0f, 51 / 255.0f, alpha),
                               ImVec4(50 / 255.0f, 52 / 255.0f, 62 / 255.0f, alpha), hover_t);
    const ImVec4 on = ImLerp(ImVec4(kAccent.x, kAccent.y, kAccent.z, alpha),
                              ImVec4(0.10f, 0.66862748f, 1.0f, alpha), hover_t);
    draw->AddRectFilled(p, ImVec2(p.x + width, p.y + height), ImGui::GetColorU32(ImLerp(off, on, t)), height * 0.5f);
    draw->AddCircleFilled(ImVec2(p.x + 2.0f + radius + t * (width - radius * 2.0f - 4.0f), p.y + radius + 2.0f),
        radius, ImGui::GetColorU32(ImLerp(ImVec4(130 / 255.0f, 133 / 255.0f, 148 / 255.0f, alpha), ImVec4(1, 1, 1, alpha), t)));
    return pressed;
}

void ToggleRow(const char* scope, const char* label, bool* value) {
    const ImVec2 row_min = ImGui::GetCursorScreenPos();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.75f, 0.76f, 0.82f, 1.0f), "%s", label);
    const float right_x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x - kRowRightInset;
    ImGui::SetCursorScreenPos(ImVec2(right_x - kToggleWidth, row_min.y + 5.0f));
    ImGui::PushID(scope);
    ImGui::PushID(label);
    ToggleButton("##toggle", value);
    ImGui::PopID();
    ImGui::PopID();
    ImGui::SetCursorScreenPos(ImVec2(row_min.x, row_min.y + kRowHeight));
}

void SliderRow(const char* scope, const char* label, float* value, float minimum, float maximum, const char* format) {
    const ImVec2 row_min = ImGui::GetCursorScreenPos();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.75f, 0.76f, 0.82f, 1.0f), "%s", label);
    const float right_x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x - kRowRightInset;
    ImGui::SetCursorScreenPos(ImVec2(right_x - 112.0f, row_min.y + 3.0f));
    ImGui::SetNextItemWidth(112.0f);
    ImGui::PushID(scope);
    ImGui::PushID(label);
    ImGui::SliderFloat("##slider", value, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();
    ImGui::PopID();
    ImGui::SetCursorScreenPos(ImVec2(row_min.x, row_min.y + kRowHeight));
}

void SliderIntRow(const char* scope, const char* label, int* value, int minimum, int maximum, const char* format) {
    const ImVec2 row_min = ImGui::GetCursorScreenPos();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.75f, 0.76f, 0.82f, 1.0f), "%s", label);
    const float right_x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x - kRowRightInset;
    ImGui::SetCursorScreenPos(ImVec2(right_x - 112.0f, row_min.y + 3.0f));
    ImGui::SetNextItemWidth(112.0f);
    ImGui::PushID(scope);
    ImGui::PushID(label);
    ImGui::SliderInt("##slider", value, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();
    ImGui::PopID();
    ImGui::SetCursorScreenPos(ImVec2(row_min.x, row_min.y + kRowHeight));
}

bool HeaderTab(const char* id, const char* label, bool selected, const ImVec2& size) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const float selected_t = EaseOutCubic(Animate(std::string("state::header::") + id, selected, 15.0f));
    const float hover_t = EaseOutCubic(Animate(std::string("hover::header::") + id, hovered, 16.0f));
    const ImVec4 inactive(0.54f, 0.55f, 0.60f, ImGui::GetStyle().Alpha);
    const ImVec4 active(kAccent.x, kAccent.y, kAccent.z, ImGui::GetStyle().Alpha);
    const ImVec4 text_color = ImLerp(inactive, active, std::max(selected_t, hover_t * 0.35f));
    const ImVec2 text_size = ImGui::CalcTextSize(label);
    draw->AddText(ImVec2(pos.x + (size.x - text_size.x) * 0.5f, pos.y + (size.y - text_size.y) * 0.5f),
        ImGui::GetColorU32(text_color), label);
    if (selected_t > 0.001f) {
        const float half = (size.x * 0.5f - 12.0f) * selected_t;
        const float center = pos.x + size.x * 0.5f;
        draw->AddLine(ImVec2(center - half, pos.y + size.y - 1.0f), ImVec2(center + half, pos.y + size.y - 1.0f),
            ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, ImGui::GetStyle().Alpha * selected_t)), 2.0f);
    }
    return clicked;
}

void BeginPulseChild(const char* id, const char* title, ImVec2 size) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float alpha = ImGui::GetStyle().Alpha;
    const ImVec2 title_pos = ImGui::GetCursorScreenPos();
    draw->AddText(g_font_small, 12.5f, ImVec2(title_pos.x + 4.0f, title_pos.y + 2.0f),
        IM_COL32(132, 134, 144, static_cast<int>(225 * alpha)), title);
    ImGui::SetCursorScreenPos(ImVec2(title_pos.x, title_pos.y + 22.0f));

    size.y = std::max(1.0f, size.y - 22.0f);
    const ImVec2 child_min = ImGui::GetCursorScreenPos();
    const ImVec2 child_max(child_min.x + size.x, child_min.y + size.y);
    FrostedRect(draw, child_min, child_max, 17, 18, 23, 0.72f, kPanelRounding, ImDrawFlags_RoundCornersAll);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(18 / 255.0f, 19 / 255.0f, 24 / 255.0f, 0.10f * alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(48 / 255.0f, 50 / 255.0f, 60 / 255.0f, 0.58f * alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kPanelRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 5.0f));
    ImGui::BeginChild(id, size, true, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}

void EndPulseChild() {
    ImGui::EndChild();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(2);
}

void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = kMenuRounding;
    style.ChildRounding = kPanelRounding;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 50.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.ItemSpacing = ImVec2(8, 8);
    style.Colors[ImGuiCol_WindowBg] = ImColor(11, 12, 16, 255);
    style.Colors[ImGuiCol_ChildBg] = ImColor(21, 22, 28, 255);
    style.Colors[ImGuiCol_Border] = ImColor(35, 37, 46, 255);
    style.Colors[ImGuiCol_FrameBg] = ImColor(30, 31, 38, 255);
    style.Colors[ImGuiCol_FrameBgHovered] = ImColor(40, 42, 51, 255);
    style.Colors[ImGuiCol_FrameBgActive] = ImColor(50, 52, 63, 255);
    style.Colors[ImGuiCol_Header] = ImColor(30, 31, 38, 255);
    style.Colors[ImGuiCol_HeaderHovered] = ImColor(40, 42, 51, 255);
    style.Colors[ImGuiCol_HeaderActive] = ImColor(50, 52, 63, 255);
    style.Colors[ImGuiCol_Text] = ImColor(255, 255, 255, 255);
    style.Colors[ImGuiCol_TextDisabled] = ImColor(100, 102, 112, 255);
    style.Colors[ImGuiCol_CheckMark] = kAccent;
    style.Colors[ImGuiCol_SliderGrab] = kAccent;
}
} // namespace

void Initialize() {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context == g_initialized_context) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    io.Fonts->Flags = ImFontAtlasFlags_NoPowerOfTwoHeight;
    io.Fonts->TexDesiredWidth = 4096;

    static const ImWchar text_ranges[] = {0x0020, 0x00FF, 0x0400, 0x052F, 0};
    static const ImWchar icon_ranges[] = {0xE900, 0xF3FF, 0};
    static const ImWchar mingcute_ranges[] = {0xF51D, 0xF51D, 0};

    ImFontConfig text_cfg{};
    text_cfg.FontDataOwnedByAtlas = false;
    text_cfg.OversampleH = 2;
    text_cfg.OversampleV = 2;
    ImFontConfig icon_cfg = text_cfg;
    icon_cfg.GlyphMinAdvanceX = 0.0f;

    g_font_small = io.Fonts->AddFontFromMemoryTTF((void*)Inter_Medium, Inter_Medium_size, 13.0f, &text_cfg, text_ranges);
    g_font_main = io.Fonts->AddFontFromMemoryTTF((void*)Inter_Medium, Inter_Medium_size, 17.0f, &text_cfg, text_ranges);
    g_font_logo = io.Fonts->AddFontFromMemoryTTF((void*)Inter_Bold, Inter_Bold_size, 18.0f, &text_cfg, text_ranges);
    g_font_icons = io.Fonts->AddFontFromMemoryTTF((void*)IconFontData, sizeof(IconFontData), 18.0f, &icon_cfg, icon_ranges);
    g_font_mingcute = io.Fonts->AddFontFromMemoryCompressedBase85TTF(MingCuteIconFont_compressed_data_base85, 20.0f, &icon_cfg, mingcute_ranges);
    io.FontDefault = g_font_main;
    io.Fonts->Build();
    ApplyStyle();
    g_initialized_context = context;
}

void SetBackgroundTexture(void* texture, float source_width, float source_height) {
    g_background_texture = texture;
    g_background_width = source_width;
    g_background_height = source_height;
}

void Render(bool* open, float visibility) {
    if (open == nullptr || visibility <= 0.001f || ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    Initialize();

    ImGui::SetNextWindowSize(ImVec2(820, 560), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    if (*open && !g_was_open) ImGui::SetNextWindowFocus();
    g_was_open = *open;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(visibility, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings;
    if (!*open) window_flags |= ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("PulseMenu", nullptr, window_flags)) {
        const ImVec2 p = ImGui::GetWindowPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float alpha = ImGui::GetStyle().Alpha;
        static int page = 0;
        static int team_tab = 1;
        DrawMenuBackdrop(draw, p, ImVec2(p.x + 820.0f, p.y + 560.0f));

        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::BeginChild("Sidebar", ImVec2(220, 560), false, ImGuiWindowFlags_NoBackground);
        ImGui::SetCursorPos(ImVec2(20, 25));
        ImGui::PushFont(g_font_icons);
        ImGui::TextColored(ImVec4(kAccent.x, kAccent.y, kAccent.z, alpha), (const char*)u8"\uE900");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::PushFont(g_font_logo);
        ImGui::Text("PULSE");
        ImGui::PopFont();

        ImGui::SetCursorPos(ImVec2(15, 80));
        if (NavTab((const char*)u8"\uF51D", "Players", page == 0)) page = 0;
        ImGui::SetCursorPos(ImVec2(15, 124));
        if (NavTab((const char*)u8"\uF51D", "Aimbot", page == 1)) page = 1;
        ImGui::SetCursorPos(ImVec2(15, 168));
        if (NavTab((const char*)u8"\uF51D", "Movement", page == 2)) page = 2;

        ImGui::SetCursorPos(ImVec2(15, 495));
        const ImVec2 profile_min = ImGui::GetCursorScreenPos();
        const ImVec2 profile_max(profile_min.x + 190, profile_min.y + 50);
        FrostedRect(draw, profile_min, profile_max, 26, 28, 35, 0.72f, kTabRounding, ImDrawFlags_RoundCornersAll, 0.08f);
        draw->AddRect(profile_min, profile_max, GlassColor(48, 50, 60, 0.48f), kTabRounding);
        draw->AddCircleFilled(ImVec2(profile_min.x + 25, profile_min.y + 25), 15.0f, IM_COL32(26, 26, 46, AlphaByte(1.0f)));
        ImGui::PushFont(g_font_icons);
        draw->AddText(g_font_icons, 14.0f, ImVec2(profile_min.x + 18, profile_min.y + 18),
            IM_COL32(200, 200, 200, AlphaByte(1.0f)), (const char*)u8"\uE906");
        ImGui::PopFont();
        draw->AddText(g_font_main, 17.0f, ImVec2(profile_min.x + 50, profile_min.y + 8),
            IM_COL32(255, 255, 255, AlphaByte(1.0f)), "PULSE");
        draw->AddText(g_font_small, 11.0f, ImVec2(profile_min.x + 50, profile_min.y + 26),
            ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, alpha)), "subscription active");
        ImGui::EndChild();

        ImGui::SameLine(0, 0);
        ImGui::BeginChild("MainContent", ImVec2(600, 560), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::SetCursorPos(ImVec2(24.0f, 13.0f));
        const ImVec2 tab_size(132.0f, 28.0f);
        if (page == 0) {
            if (HeaderTab("##allies_header_tab", "Allies", team_tab == 0, tab_size)) team_tab = 0;
            ImGui::SameLine(0.0f, 4.0f);
            if (HeaderTab("##enemies_header_tab", "Enemies", team_tab == 1, tab_size)) team_tab = 1;
        } else {
            const char* title = page == 1 ? "Aimbot" : "Movement";
            HeaderTab("##page_header", title, true, ImVec2(160.0f, 28.0f));
        }
        draw->AddLine(ImVec2(p.x + 220, p.y + 55), ImVec2(p.x + 820, p.y + 55), IM_COL32(35, 37, 46, AlphaByte(1.0f)));

        constexpr float content_margin = 16.0f;
        constexpr float content_width = 568.0f;
        constexpr float child_width = 277.0f;
        ImGui::SetCursorPos(ImVec2(content_margin, 75.0f));
        ImGui::BeginChild("ScrollableTabContent", ImVec2(content_width, 470.0f), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        if (page == 1) {
            aimbot::Settings* aim = aimbot::GetSettings();
            BeginPulseChild("AIMBOT##silent", "SILENT AIM", ImVec2(child_width, 276.0f));
            ToggleRow("silent", "Enable Silent Aim", &aim->enabled);
            ToggleRow("silent", "Draw FOV", &aim->draw_fov);
            ToggleRow("silent", "Require RMB", &aim->require_rmb);
            ToggleRow("silent", "Visible Only", &aim->visible_only);
            ToggleRow("silent", "Ignore Spawn Shield", &aim->ignore_spawn_shield);
            SliderRow("silent", "FOV Radius", &aim->fov_radius, 10.0f, 600.0f, "%.0f px");
            SliderRow("silent", "Max Distance", &aim->max_distance, 10.0f, 300.0f, "%.0f m");
            SliderIntRow("silent", "Hit Chance", &aim->hit_chance, 1, 100, "%d %%");
            EndPulseChild();
            ImGui::SameLine(0.0f, 14.0f);
            esp::WeaponSettings* weapon = esp::GetWeaponSettings();
            BeginPulseChild("WEAPON##combat", "WEAPON", ImVec2(child_width, 196.0f));
            ToggleRow("weapon", "No Spread", &weapon->no_spread);
            ToggleRow("weapon", "Infinite Ammo", &weapon->infinite_ammo);
            ToggleRow("weapon", "Rapid Fire", &weapon->rapid_fire);
            ToggleRow("weapon", "One Hit", &weapon->one_hit);
            ToggleRow("weapon", "Wall Shot", &weapon->wall_shot);
            EndPulseChild();
        } else if (page == 2) {
            movement::Settings* movement = movement::GetSettings();
            BeginPulseChild("MOVEMENT##main", "MOVEMENT", ImVec2(child_width, 276.0f));
            ToggleRow("movement", "Speed Hack", &movement->speed_hack);
            SliderRow("movement", "Ground Speed", &movement->move_speed, 2.0f, 100.0f, "%.1f");
            ToggleRow("movement", "Fly (Noclip)", &movement->fly);
            SliderRow("movement", "Fly Speed", &movement->fly_speed, 2.0f, 200.0f, "%.1f");
            ToggleRow("movement", "Third Person", &movement->third_person);
            SliderRow("movement", "Camera Distance", &movement->third_person_distance, 1.0f, 8.0f, "%.1f m");
            ToggleRow("movement", "Spinbot", &movement->spinbot);
            SliderRow("movement", "Spin Speed", &movement->spin_speed, 90.0f, 2160.0f, "%.0f deg/s");
            EndPulseChild();
        } else {
            esp::Settings* settings = esp::GetSettings(team_tab == 0 ? esp::Team::Allies : esp::Team::Enemies);
            const char* scope = team_tab == 0 ? "allies" : "enemies";
            const char* panel_title = team_tab == 0 ? "ALLY ESP" : "ENEMY ESP";
            BeginPulseChild(team_tab == 0 ? "ESP MAIN##allies" : "ESP MAIN##enemies", panel_title, ImVec2(child_width, 292.0f));
            ToggleRow(scope, "Enable ESP", &settings->enabled);
            ToggleRow(scope, "Box", &settings->box);
            ToggleRow(scope, "Player Name", &settings->name);
            ToggleRow(scope, "Ammo", &settings->ammo);
            ToggleRow(scope, "Distance", &settings->distance);
            ToggleRow(scope, "Snapline", &settings->snapline);
            ToggleRow(scope, "Visible Only", &settings->visible_only);
            ToggleRow(scope, "Spawn Shield", &settings->spawn_shield);
            ToggleRow(scope, "Chams", &settings->chams);
            EndPulseChild();
        }
        ImGui::EndChild();
        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void Shutdown() {
    g_initialized_context = nullptr;
    g_font_main = nullptr;
    g_font_logo = nullptr;
    g_font_small = nullptr;
    g_font_icons = nullptr;
    g_font_mingcute = nullptr;
    g_background_texture = nullptr;
    g_background_width = 0.0f;
    g_background_height = 0.0f;
    g_was_open = false;
    g_animations.clear();
}

} // namespace pulse_ui
