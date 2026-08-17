#pragma once

namespace movement {

struct Settings {
    bool speed_hack;
    float move_speed;
    bool fly;
    float fly_speed;
    bool third_person;
    float third_person_distance;
    bool spinbot;
    float spin_speed;
};

Settings* GetSettings();
bool Initialize(void* game_assembly);
void SetLocalPlayer(void* player);
void Apply(void* local_player);
bool IsActive();
int EffectiveCollisionMask(void* player, int current_mask);
void Shutdown();

} // namespace movement
