#pragma once

struct ImGuiContext;

namespace pulse_ui {

void Initialize();
void SetBackgroundTexture(void* texture, float source_width, float source_height);
void Render(bool* open, float visibility = 1.0f);
void Shutdown();

} // namespace pulse_ui
