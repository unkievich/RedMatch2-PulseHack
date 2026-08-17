#pragma once

namespace aimbot {

struct Vector3 {
    float x;
    float y;
    float z;
};

struct Settings {
    bool enabled;
    bool draw_fov;
    bool require_rmb;
    bool visible_only;
    bool ignore_spawn_shield;
    float fov_radius;
    float max_distance;
    int hit_chance;
};

Settings* GetSettings();
bool Initialize(void* game_assembly);
void UpdateRuntimeSettings();
void SetNoSpread(bool enabled);
void SetWallShot(bool enabled, int damage_layer_mask);
bool IsLocalShotWindow();
void SetTarget(const Vector3& origin, const Vector3& target, bool valid);
void ClearTarget();
void Shutdown();

} // namespace aimbot
