#pragma once

namespace esp {

enum class Team {
    Allies,
    Enemies
};

struct Settings {
    bool enabled;
    bool box;
    bool name;
    bool ammo;
    bool distance;
    bool snapline;
    bool visible_only;
    bool spawn_shield;
    bool chams;
};

struct WeaponSettings {
    bool no_spread;
    bool infinite_ammo;
    bool rapid_fire;
    bool one_hit;
    bool wall_shot;
};

Settings* GetSettings(Team team);
WeaponSettings* GetWeaponSettings();
void Render();
void Shutdown();

} // namespace esp
