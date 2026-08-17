#include "features/esp.h"
#include "features/aimbot.h"
#include "features/movement.h"

#include <imgui.h>
#include <imgui_internal.h>

#if defined(_WIN32)
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdio>

namespace esp {
namespace {
constexpr std::uintptr_t kFindObjectsOfTypeRva = 0xC76090;
constexpr std::uintptr_t kComponentGetComponentRva = 0xBA5810;
constexpr std::uintptr_t kComponentGetComponentInChildrenRva = 0xBA5700;
constexpr std::uintptr_t kComponentGetComponentInParentRva = 0xBA5790;
constexpr std::uintptr_t kComponentGetTransformRva = 0xBA5930;
constexpr std::uintptr_t kComponentGetGameObjectRva = 0xBA58F0;
constexpr std::uintptr_t kGameObjectGetTransformRva = 0xBA9EB0;
constexpr std::uintptr_t kGameObjectSetActiveRva = 0xBA9A40;
constexpr std::uintptr_t kGameObjectGetActiveSelfRva = 0xBA9E30;
constexpr std::uintptr_t kTransformGetPositionRva = 0xB3DDC0;
constexpr std::uintptr_t kCameraGetMainRva = 0xBA2C50;
constexpr std::uintptr_t kCameraWorldToScreenPointRva = 0xBA2610;
constexpr std::uintptr_t kGameObjectGetActiveInHierarchyRva = 0xBA9DF0;
constexpr std::uintptr_t kRendererGetEnabledRva = 0xB322C0;
constexpr std::uintptr_t kRendererSetEnabledRva = 0xB32400;
constexpr std::uintptr_t kRendererGetMaterialRva = 0xB32070;
constexpr std::uintptr_t kRendererGetSharedMaterialRva = 0xB320F0;
constexpr std::uintptr_t kMaterialGetColorRva = 0xBB0D00;
constexpr std::uintptr_t kMaterialSetColorRva = 0xBB0FF0;
constexpr std::uintptr_t kColliderGetBoundsRva = 0x1092540;
constexpr std::uintptr_t kPhysicsLinecastRva = 0x1093D60;
constexpr std::uintptr_t kNetworkTryGetIntRva = 0x37FDA0;
constexpr std::uintptr_t kTeamIntMethodInfoPtrRva = 0x2D97D18;
constexpr std::uintptr_t kPlayerGetCurrentItemRva = 0x11101E0;
constexpr std::uintptr_t kPlayerApplyDamageRva = 0x110BD60;
constexpr std::size_t kPlayerApplyDamagePatchSize = 17;
constexpr std::size_t kMaxPlayers = 256;

struct Vector3 {
    float x;
    float y;
    float z;
};

struct Bounds {
    Vector3 center;
    Vector3 extents;
};

struct Color {
    float r;
    float g;
    float b;
    float a;
};

struct Il2CppArray {
    void* klass;
    void* monitor;
    void* bounds;
    std::uintptr_t max_length;
    void* vector[1];
};

struct ScreenBox {
    float left;
    float top;
    float right;
    float bottom;
    float distance;
    int ammo;
    int total_ammo;
    int magazine_size;
    bool ally;
    bool visible;
    bool spawn_protected;
    Vector3 head_world;
    char name[64];
};

struct PlayerCache {
    void* player;
    void* transform;
    void* collider;
    void* outline_renderer;
    void* outline_game_object;
    void* outline_material;
    void* chams_material;
    Color outline_color;
    Color original_chams_color;
    char name[64];
    int ammo;
    int total_ammo;
    int magazine_size;
    int team_id;
    bool has_team_id;
    bool original_outline_active;
    bool original_renderer_enabled;
    bool chams_applied;
    bool visible;
    bool spawn_protected;
    ULONGLONG dynamic_at;
    ULONGLONG visibility_at;
    ULONGLONG chams_at;
    ULONGLONG refreshed_at;
    ULONGLONG last_seen_at;
};

struct Il2CppString {
    void* klass;
    void* monitor;
    std::int32_t length;
    wchar_t chars[1];
};

using Il2CppDomainGet = void* (*)();
using Il2CppThreadCurrent = void* (*)();
using Il2CppThreadAttach = void* (*)(void* domain);
using Il2CppDomainAssemblyOpen = void* (*)(void* domain, const char* name);
using Il2CppAssemblyGetImage = const void* (*)(const void* assembly);
using Il2CppClassFromName = void* (*)(const void* image, const char* namespaze, const char* name);
using Il2CppClassGetType = const void* (*)(void* klass);
using Il2CppTypeGetObject = void* (*)(const void* type);
using Il2CppClassGetFieldFromName = void* (*)(void* klass, const char* name);
using Il2CppFieldStaticGetValue = void (*)(void* field, void* value);

using FindObjectsOfType = Il2CppArray* (*)(void* reflection_type, const void* method);
using ComponentGetComponent = void* (*)(void* component, void* reflection_type, const void* method);
using ComponentGetComponentInChildren = void* (*)(void* component, void* reflection_type, bool include_inactive, const void* method);
using ComponentGetComponentInParent = void* (*)(void* component, void* reflection_type, const void* method);
using ComponentGetTransform = void* (*)(void* component, const void* method);
using ComponentGetGameObject = void* (*)(void* component, const void* method);
using GameObjectGetTransform = void* (*)(void* game_object, const void* method);
using GameObjectSetActive = void (*)(void* game_object, bool active, const void* method);
using GameObjectGetActiveSelf = bool (*)(void* game_object, const void* method);
using TransformGetPosition = Vector3 (*)(void* transform, const void* method);
using CameraGetMain = void* (*)(const void* method);
using CameraWorldToScreenPoint = Vector3 (*)(void* camera, Vector3 position, const void* method);
using GameObjectGetActiveInHierarchy = bool (*)(void* game_object, const void* method);
using RendererGetEnabled = bool (*)(void* renderer, const void* method);
using RendererSetEnabled = void (*)(void* renderer, bool enabled, const void* method);
using RendererGetMaterial = void* (*)(void* renderer, const void* method);
using RendererGetSharedMaterial = void* (*)(void* renderer, const void* method);
using MaterialGetColor = Color (*)(void* material, const void* method);
using MaterialSetColor = void (*)(void* material, Color color, const void* method);
using ColliderGetBounds = Bounds (*)(void* collider, const void* method);
using PhysicsLinecast = bool (*)(const Vector3* start, const Vector3* end, int layer_mask,
                                 int query_trigger_interaction, const void* method);
using NetworkTryGetInt = bool (*)(void* network_object, int key, int* value, const void* method);
using PlayerGetCurrentItem = void* (*)(void* player, const void* method);
using PlayerApplyDamage = int (*)(void* player, const void* damage, const void* method);

Settings g_allies{false, true, true, true, true, false, false, true, false};
Settings g_enemies{true, true, true, true, true, false, false, true, false};
aimbot::Settings& g_aim = *aimbot::GetSettings();
WeaponSettings g_weapon{false, false, false, false, false};
HMODULE g_game_assembly = nullptr;
void* g_player_reflection_type = nullptr;
void* g_team_outline_reflection_type = nullptr;
void* g_username_reflection_type = nullptr;
void* g_collider_reflection_type = nullptr;
void* g_any_collider_reflection_type = nullptr;
void* g_local_instance_field = nullptr;

Il2CppDomainGet g_domain_get = nullptr;
Il2CppThreadCurrent g_thread_current = nullptr;
Il2CppThreadAttach g_thread_attach = nullptr;
Il2CppDomainAssemblyOpen g_domain_assembly_open = nullptr;
Il2CppAssemblyGetImage g_assembly_get_image = nullptr;
Il2CppClassFromName g_class_from_name = nullptr;
Il2CppClassGetType g_class_get_type = nullptr;
Il2CppTypeGetObject g_type_get_object = nullptr;
Il2CppClassGetFieldFromName g_class_get_field_from_name = nullptr;
Il2CppFieldStaticGetValue g_field_static_get_value = nullptr;

FindObjectsOfType g_find_objects_of_type = nullptr;
ComponentGetComponent g_component_get_component = nullptr;
ComponentGetComponentInChildren g_component_get_component_in_children = nullptr;
ComponentGetComponentInParent g_component_get_component_in_parent = nullptr;
ComponentGetTransform g_component_get_transform = nullptr;
ComponentGetGameObject g_component_get_game_object = nullptr;
GameObjectGetTransform g_game_object_get_transform = nullptr;
GameObjectSetActive g_game_object_set_active = nullptr;
GameObjectGetActiveSelf g_game_object_get_active_self = nullptr;
TransformGetPosition g_transform_get_position = nullptr;
CameraGetMain g_camera_get_main = nullptr;
CameraWorldToScreenPoint g_world_to_screen = nullptr;
GameObjectGetActiveInHierarchy g_game_object_get_active = nullptr;
RendererGetEnabled g_renderer_get_enabled = nullptr;
RendererSetEnabled g_renderer_set_enabled = nullptr;
RendererGetMaterial g_renderer_get_material = nullptr;
RendererGetSharedMaterial g_renderer_get_shared_material = nullptr;
MaterialGetColor g_material_get_color = nullptr;
MaterialSetColor g_material_set_color = nullptr;
ColliderGetBounds g_collider_get_bounds = nullptr;
PhysicsLinecast g_physics_linecast = nullptr;
PlayerGetCurrentItem g_player_get_current_item = nullptr;
NetworkTryGetInt g_network_try_get_int = nullptr;
bool g_no_spread_was_enabled = false;
struct ShotInfoBackup {
    void* object = nullptr;
    bool apply_spread_before_shooting = false;
    float bullet_spread = 0.0f;
    float ads_bullet_spread = 0.0f;
    float camera_shake = 0.0f;
    float ads_camera_shake = 0.0f;
};
struct NoSpreadBackup {
    void* player = nullptr;
    void* item = nullptr;
    void* info = nullptr;
    float player_spread = 0.0f;
    float item_spread = 0.0f;
    float max_bullet_spread = 0.0f;
    float normal_spread = 0.0f;
    float ads_spread = 0.0f;
    float movement_spread_multiplier = 0.0f;
    float ads_movement_spread_multiplier = 0.0f;
    ShotInfoBackup primary{};
    ShotInfoBackup secondary{};
};
NoSpreadBackup g_no_spread_backup{};
PlayerCache g_player_cache[kMaxPlayers + 1]{};
ScreenBox g_cached_boxes[kMaxPlayers]{};
std::size_t g_cached_box_count = 0;
ULONGLONG g_last_esp_update = 0;
float g_cached_display_width = 0.0f;
float g_cached_display_height = 0.0f;
void* g_player_snapshot[kMaxPlayers]{};
std::size_t g_player_snapshot_count = 0;
ULONGLONG g_last_player_scan = 0;
std::atomic<void*> g_local_player{nullptr};
std::atomic_ulong g_damage_hook_calls{0};
PlayerApplyDamage g_original_player_apply_damage = nullptr;
void* g_player_apply_damage_target = nullptr;
void* g_player_apply_damage_trampoline = nullptr;
std::uint8_t g_player_apply_damage_original[kPlayerApplyDamagePatchSize]{};
std::atomic_bool g_one_hit_enabled{false};

void RestoreChams(PlayerCache& cache);
bool IsSpawnProtected(void* player);

void WriteAbsoluteJump(void* destination, const void* target) {
    auto* bytes = static_cast<std::uint8_t*>(destination);
    bytes[0] = 0xFF;
    bytes[1] = 0x25;
    *reinterpret_cast<std::uint32_t*>(bytes + 2) = 0;
    *reinterpret_cast<std::uintptr_t*>(bytes + 6) = reinterpret_cast<std::uintptr_t>(target);
}


int HookedPlayerApplyDamage(void* player, const void* damage, const void* method) {
    g_damage_hook_calls.fetch_add(1, std::memory_order_acquire);
    PlayerApplyDamage original = g_original_player_apply_damage;
    if (g_one_hit_enabled.load(std::memory_order_relaxed) && aimbot::IsLocalShotWindow() &&
        player != nullptr && damage != nullptr &&
        player != g_local_player.load(std::memory_order_relaxed) &&
        !IsSpawnProtected(player)) {
        __try {
            // Damage::amount is the second UInt16 in the packet assembled by
            // PlayerController. Preserve the event id and damage type list.
            auto* amount = reinterpret_cast<std::uint16_t*>(
                const_cast<std::uint8_t*>(static_cast<const std::uint8_t*>(damage)) + 2);
            if (*amount > 0) *amount = 32767;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    const int result = original != nullptr ? original(player, damage, method) : 0;
    g_damage_hook_calls.fetch_sub(1, std::memory_order_release);
    return result;
}

bool InstallDamageHook() {
    if (g_original_player_apply_damage != nullptr) return true;
    if (g_game_assembly == nullptr) return false;

    auto* target = reinterpret_cast<std::uint8_t*>(
        reinterpret_cast<std::uintptr_t>(g_game_assembly) + kPlayerApplyDamageRva);
    constexpr std::uint8_t expected[kPlayerApplyDamagePatchSize] = {
        0x48, 0x89, 0x5C, 0x24, 0x18, 0x56, 0x48, 0x83, 0xEC,
        0x20, 0x80, 0x3D, 0x6E, 0xCA, 0xC1, 0x01, 0x00
    };
    if (std::memcmp(target, expected, sizeof(expected)) != 0) return false;

    constexpr std::size_t kRelocatedPrologueSize = 10 + 10 + 3;
    void* trampoline = VirtualAlloc(nullptr, kRelocatedPrologueSize + 14,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (trampoline == nullptr) return false;
    std::memcpy(g_player_apply_damage_original, target, kPlayerApplyDamagePatchSize);
    auto* relocated = static_cast<std::uint8_t*>(trampoline);
    std::memcpy(relocated, target, 10);
    const std::int32_t flag_displacement = *reinterpret_cast<const std::int32_t*>(target + 12);
    const auto flag_address = reinterpret_cast<std::uintptr_t>(target + 17) + flag_displacement;
    relocated[10] = 0x48;
    relocated[11] = 0xB8;
    *reinterpret_cast<std::uintptr_t*>(relocated + 12) = flag_address;
    relocated[20] = 0x80;
    relocated[21] = 0x38;
    relocated[22] = 0x00;
    WriteAbsoluteJump(relocated + kRelocatedPrologueSize,
        target + kPlayerApplyDamagePatchSize);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, kPlayerApplyDamagePatchSize, PAGE_EXECUTE_READWRITE, &old_protect)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }
    g_player_apply_damage_target = target;
    g_player_apply_damage_trampoline = trampoline;
    g_original_player_apply_damage = reinterpret_cast<PlayerApplyDamage>(trampoline);
    WriteAbsoluteJump(target, reinterpret_cast<void*>(HookedPlayerApplyDamage));
    std::memset(target + 14, 0x90, kPlayerApplyDamagePatchSize - 14);
    DWORD ignored = 0;
    VirtualProtect(target, kPlayerApplyDamagePatchSize, old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), target, kPlayerApplyDamagePatchSize);
    return true;
}

void RemoveDamageHook() {
    if (g_player_apply_damage_target != nullptr) {
        DWORD old_protect = 0;
        if (VirtualProtect(g_player_apply_damage_target, kPlayerApplyDamagePatchSize,
                PAGE_EXECUTE_READWRITE, &old_protect)) {
            std::memcpy(g_player_apply_damage_target, g_player_apply_damage_original,
                kPlayerApplyDamagePatchSize);
            DWORD ignored = 0;
            VirtualProtect(g_player_apply_damage_target, kPlayerApplyDamagePatchSize, old_protect, &ignored);
            FlushInstructionCache(GetCurrentProcess(), g_player_apply_damage_target,
                kPlayerApplyDamagePatchSize);
        }
    }
    while (g_damage_hook_calls.load(std::memory_order_acquire) != 0) Sleep(1);
    g_original_player_apply_damage = nullptr;
    g_player_apply_damage_target = nullptr;
    if (g_player_apply_damage_trampoline != nullptr) {
        VirtualFree(g_player_apply_damage_trampoline, 0, MEM_RELEASE);
        g_player_apply_damage_trampoline = nullptr;
    }
}

void ResetEspCaches() {
    for (PlayerCache& cache : g_player_cache) RestoreChams(cache);
    std::memset(g_player_cache, 0, sizeof(g_player_cache));
    std::memset(g_cached_boxes, 0, sizeof(g_cached_boxes));
    g_cached_box_count = 0;
    g_last_esp_update = 0;
    g_cached_display_width = 0.0f;
    g_cached_display_height = 0.0f;
    std::memset(g_player_snapshot, 0, sizeof(g_player_snapshot));
    g_player_snapshot_count = 0;
    g_last_player_scan = 0;
}

template <typename T>
bool ResolveExport(T& output, const char* name) {
    output = reinterpret_cast<T>(GetProcAddress(g_game_assembly, name));
    return output != nullptr;
}

void* MakeReflectionType(const void* image, const char* namespaze, const char* name) {
    void* klass = g_class_from_name(image, namespaze, name);
    if (klass == nullptr) {
        return nullptr;
    }
    const void* type = g_class_get_type(klass);
    return type != nullptr ? g_type_get_object(type) : nullptr;
}

bool InitializeRuntime() {
    if (g_player_reflection_type != nullptr && g_team_outline_reflection_type != nullptr &&
        g_username_reflection_type != nullptr &&
        g_collider_reflection_type != nullptr && g_any_collider_reflection_type != nullptr &&
        g_local_instance_field != nullptr) {
        aimbot::Initialize(g_game_assembly);
        movement::Initialize(g_game_assembly);
        InstallDamageHook();
        return true;
    }

    g_game_assembly = GetModuleHandleW(L"GameAssembly.dll");
    if (g_game_assembly == nullptr) {
        return false;
    }

    if (!ResolveExport(g_domain_get, "il2cpp_domain_get") ||
        !ResolveExport(g_thread_current, "il2cpp_thread_current") ||
        !ResolveExport(g_thread_attach, "il2cpp_thread_attach") ||
        !ResolveExport(g_domain_assembly_open, "il2cpp_domain_assembly_open") ||
        !ResolveExport(g_assembly_get_image, "il2cpp_assembly_get_image") ||
        !ResolveExport(g_class_from_name, "il2cpp_class_from_name") ||
        !ResolveExport(g_class_get_type, "il2cpp_class_get_type") ||
        !ResolveExport(g_type_get_object, "il2cpp_type_get_object") ||
        !ResolveExport(g_class_get_field_from_name, "il2cpp_class_get_field_from_name") ||
        !ResolveExport(g_field_static_get_value, "il2cpp_field_static_get_value")) {
        return false;
    }

    void* domain = g_domain_get();
    if (domain == nullptr) {
        return false;
    }
    if (g_thread_current() == nullptr && g_thread_attach(domain) == nullptr) {
        return false;
    }

    void* assembly = g_domain_assembly_open(domain, "Assembly-CSharp");
    if (assembly == nullptr) {
        return false;
    }

    const void* image = g_assembly_get_image(assembly);
    void* player_class = image != nullptr ? g_class_from_name(image, "", "PlayerController") : nullptr;
    if (player_class == nullptr) {
        return false;
    }

    void* physics_assembly = g_domain_assembly_open(domain, "UnityEngine.PhysicsModule");
    const void* physics_image = physics_assembly != nullptr ? g_assembly_get_image(physics_assembly) : nullptr;

    g_player_reflection_type = MakeReflectionType(image, "", "PlayerController");
    g_team_outline_reflection_type = MakeReflectionType(image, "", "PlayerTeamOutline");
    g_username_reflection_type = MakeReflectionType(image, "", "PlayerUsername");
    g_collider_reflection_type = physics_image != nullptr
        ? MakeReflectionType(physics_image, "UnityEngine", "CapsuleCollider") : nullptr;
    g_any_collider_reflection_type = physics_image != nullptr
        ? MakeReflectionType(physics_image, "UnityEngine", "Collider") : nullptr;
    g_local_instance_field = g_class_get_field_from_name(player_class, "LocalInstance");
    if (g_player_reflection_type == nullptr || g_team_outline_reflection_type == nullptr ||
        g_username_reflection_type == nullptr ||
        g_collider_reflection_type == nullptr || g_any_collider_reflection_type == nullptr ||
        g_local_instance_field == nullptr) {
        g_player_reflection_type = nullptr;
        g_team_outline_reflection_type = nullptr;
        g_username_reflection_type = nullptr;
        g_collider_reflection_type = nullptr;
        g_any_collider_reflection_type = nullptr;
        g_local_instance_field = nullptr;
        return false;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(g_game_assembly);
    g_find_objects_of_type = reinterpret_cast<FindObjectsOfType>(base + kFindObjectsOfTypeRva);
    g_component_get_component = reinterpret_cast<ComponentGetComponent>(base + kComponentGetComponentRva);
    g_component_get_component_in_children = reinterpret_cast<ComponentGetComponentInChildren>(base + kComponentGetComponentInChildrenRva);
    g_component_get_component_in_parent = reinterpret_cast<ComponentGetComponentInParent>(base + kComponentGetComponentInParentRva);
    g_component_get_transform = reinterpret_cast<ComponentGetTransform>(base + kComponentGetTransformRva);
    g_component_get_game_object = reinterpret_cast<ComponentGetGameObject>(base + kComponentGetGameObjectRva);
    g_game_object_get_transform = reinterpret_cast<GameObjectGetTransform>(base + kGameObjectGetTransformRva);
    g_game_object_set_active = reinterpret_cast<GameObjectSetActive>(base + kGameObjectSetActiveRva);
    g_game_object_get_active_self = reinterpret_cast<GameObjectGetActiveSelf>(base + kGameObjectGetActiveSelfRva);
    g_transform_get_position = reinterpret_cast<TransformGetPosition>(base + kTransformGetPositionRva);
    g_camera_get_main = reinterpret_cast<CameraGetMain>(base + kCameraGetMainRva);
    g_world_to_screen = reinterpret_cast<CameraWorldToScreenPoint>(base + kCameraWorldToScreenPointRva);
    g_game_object_get_active = reinterpret_cast<GameObjectGetActiveInHierarchy>(base + kGameObjectGetActiveInHierarchyRva);
    g_renderer_get_enabled = reinterpret_cast<RendererGetEnabled>(base + kRendererGetEnabledRva);
    g_renderer_set_enabled = reinterpret_cast<RendererSetEnabled>(base + kRendererSetEnabledRva);
    g_renderer_get_material = reinterpret_cast<RendererGetMaterial>(base + kRendererGetMaterialRva);
    g_renderer_get_shared_material = reinterpret_cast<RendererGetSharedMaterial>(base + kRendererGetSharedMaterialRva);
    g_material_get_color = reinterpret_cast<MaterialGetColor>(base + kMaterialGetColorRva);
    g_material_set_color = reinterpret_cast<MaterialSetColor>(base + kMaterialSetColorRva);
    g_collider_get_bounds = reinterpret_cast<ColliderGetBounds>(base + kColliderGetBoundsRva);
    g_physics_linecast = reinterpret_cast<PhysicsLinecast>(base + kPhysicsLinecastRva);
    g_player_get_current_item = reinterpret_cast<PlayerGetCurrentItem>(base + kPlayerGetCurrentItemRva);
    g_network_try_get_int = reinterpret_cast<NetworkTryGetInt>(base + kNetworkTryGetIntRva);
    aimbot::Initialize(g_game_assembly);
    movement::Initialize(g_game_assembly);
    InstallDamageHook();
    return true;
}

int DecodeObfuscatedInt(const void* address) {
    const auto* values = static_cast<const std::int32_t*>(address);
    return values[1] - values[0];
}

void* FindRelatedComponent(void* component, void* reflection_type) {
    if (component == nullptr || reflection_type == nullptr) return nullptr;
    void* result = g_component_get_component(component, reflection_type, nullptr);
    if (result == nullptr) {
        result = g_component_get_component_in_children(component, reflection_type, true, nullptr);
    }
    if (result == nullptr) {
        result = g_component_get_component_in_parent(component, reflection_type, nullptr);
    }
    return result;
}

void CopyUsername(void* player, char* output, int capacity) {
    output[0] = '\0';
    void* username = FindRelatedComponent(player, g_username_reflection_type);
    if (username == nullptr) {
        return;
    }
    void* text_component = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(username) + 0x28);
    if (text_component == nullptr) {
        return;
    }
    auto* text = *reinterpret_cast<Il2CppString**>(reinterpret_cast<std::uintptr_t>(text_component) + 0xC8);
    if (text == nullptr || text->length <= 0 || text->length > 128) {
        return;
    }
    WideCharToMultiByte(CP_UTF8, 0, text->chars, text->length, output, capacity - 1, nullptr, nullptr);
    output[capacity - 1] = '\0';
}

void ReadAmmo(void* player, int& ammo, int& total_ammo, int& magazine_size) {
    ammo = -1;
    total_ammo = -1;
    magazine_size = -1;
    void* current_item = g_player_get_current_item != nullptr ? g_player_get_current_item(player, nullptr) : nullptr;
    if (current_item != nullptr) {
        ammo = DecodeObfuscatedInt(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(current_item) + 0x34));
        total_ammo = DecodeObfuscatedInt(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(current_item) + 0x3C));
        void* info = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(current_item) + 0x18);
        if (info != nullptr) magazine_size = *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(info) + 0x48);
        if (ammo >= 0 && ammo <= 10000 && total_ammo >= 0 && total_ammo <= 100000 &&
            magazine_size > 0 && magazine_size <= 10000) return;
        ammo = total_ammo = magazine_size = -1;
    }
    auto* items = *reinterpret_cast<Il2CppArray**>(reinterpret_cast<std::uintptr_t>(player) + 0x60);
    if (items == nullptr || items->max_length > 64) {
        return;
    }
    for (std::size_t i = 0; i < static_cast<std::size_t>(items->max_length); ++i) {
        void* item = items->vector[i];
        if (item == nullptr) {
            continue;
        }
        void* graphics = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(item) + 0x20);
        if (graphics == nullptr || !g_game_object_get_active(graphics, nullptr)) {
            continue;
        }
        ammo = DecodeObfuscatedInt(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(item) + 0x34));
        total_ammo = DecodeObfuscatedInt(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(item) + 0x3C));
        void* info = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(item) + 0x18);
        if (info != nullptr) {
            magazine_size = *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(info) + 0x48);
        }
        if (ammo < 0 || ammo > 10000 || total_ammo < 0 || total_ammo > 100000) {
            ammo = -1;
            total_ammo = -1;
        }
        if (magazine_size <= 0 || magazine_size > 10000) magazine_size = -1;
        return;
    }
}

void ReadAmmoSafe(void* player, int& ammo, int& total_ammo, int& magazine_size) {
    __try {
        ReadAmmo(player, ammo, total_ammo, magazine_size);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ammo = -1;
        total_ammo = -1;
        magazine_size = -1;
    }
}

bool IsGameObjectActiveSafe(void* object) {
    if (object == nullptr || g_game_object_get_active == nullptr) return false;
    __try {
        return g_game_object_get_active(object, nullptr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void* FindActiveItem(void* player) {
    if (player == nullptr) return nullptr;
    if (g_player_get_current_item != nullptr) {
        void* current = g_player_get_current_item(player, nullptr);
        if (current != nullptr) return current;
    }
    auto* items = *reinterpret_cast<Il2CppArray**>(reinterpret_cast<std::uintptr_t>(player) + 0x60);
    if (items == nullptr || items->max_length > 64) return nullptr;
    for (std::size_t i = 0; i < static_cast<std::size_t>(items->max_length); ++i) {
        void* item = items->vector[i];
        if (item == nullptr) continue;
        void* graphics = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(item) + 0x20);
        if (graphics != nullptr && g_game_object_get_active(graphics, nullptr)) return item;
    }
    return nullptr;
}

void WriteObfuscatedInt(void* address, int value) {
    auto* values = static_cast<std::uint32_t*>(address);
    values[1] = values[0] + static_cast<std::uint32_t>(value);
}

bool GetOutlineSignature(void* player, void*& renderer, void*& material, Color& color) {
    renderer = nullptr;
    material = nullptr;
    color = {};
    if (player == nullptr) return false;
    void* outline = FindRelatedComponent(player, g_team_outline_reflection_type);
    if (outline == nullptr) return false;
    renderer = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(outline) + 0x18);
    if (renderer == nullptr) return false;
    material = g_renderer_get_shared_material(renderer, nullptr);
    if (material == nullptr) return false;
    color = g_material_get_color(material, nullptr);
    return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b);
}

bool ReadTeamIdSafe(void* player, int& team_id) {
    team_id = 0;
    if (player == nullptr || g_network_try_get_int == nullptr || g_game_assembly == nullptr) return false;
    __try {
        void* outline = FindRelatedComponent(player, g_team_outline_reflection_type);
        if (outline == nullptr) return false;
        void* identity = *reinterpret_cast<void**>(
            static_cast<std::uint8_t*>(outline) + 0x20);
        if (identity == nullptr) return false;
        void* network_object = *reinterpret_cast<void**>(
            static_cast<std::uint8_t*>(identity) + 0x20);
        if (network_object == nullptr) return false;
        const void* method = *reinterpret_cast<void**>(
            reinterpret_cast<std::uintptr_t>(g_game_assembly) + kTeamIntMethodInfoPtrRva);
        if (method == nullptr) return false;
        // PlayerTeamOutline.Start reads synced integer key 4 and passes it to
        // TeamInfoManager. This is the actual team id used by the game.
        return g_network_try_get_int(network_object, 4, &team_id, method);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        team_id = 0;
        return false;
    }
}

bool IsSameTeam(void* player_material, const Color& player_color,
                void* local_material, const Color& local_color) {
    if (player_material == nullptr || local_material == nullptr) return false;
    if (player_material == local_material) return true;

    const float player_max = std::max({player_color.r, player_color.g, player_color.b});
    const float player_min = std::min({player_color.r, player_color.g, player_color.b});
    const float local_max = std::max({local_color.r, local_color.g, local_color.b});
    const float local_min = std::min({local_color.r, local_color.g, local_color.b});
    if (player_max - player_min < 0.06f || local_max - local_min < 0.06f) return false;

    const float dr = player_color.r - local_color.r;
    const float dg = player_color.g - local_color.g;
    const float db = player_color.b - local_color.b;
    return dr * dr + dg * dg + db * db < 0.02f;
}

void RestoreChams(PlayerCache& cache) {
    if (!cache.chams_applied) return;
    __try {
        if (cache.chams_material != nullptr && g_material_set_color != nullptr) {
            g_material_set_color(cache.chams_material, cache.original_chams_color, nullptr);
        }
        if (cache.outline_renderer != nullptr && g_renderer_set_enabled != nullptr) {
            g_renderer_set_enabled(cache.outline_renderer, cache.original_renderer_enabled, nullptr);
        }
        if (cache.outline_game_object != nullptr && g_game_object_set_active != nullptr) {
            g_game_object_set_active(cache.outline_game_object, cache.original_outline_active, nullptr);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    cache.chams_applied = false;
}

void ApplyChams(PlayerCache& cache, bool enabled, bool ally, bool spawn_protected, ULONGLONG now) {
    if (!enabled || cache.outline_renderer == nullptr || g_component_get_game_object == nullptr ||
        g_game_object_get_active_self == nullptr || g_game_object_set_active == nullptr ||
        g_renderer_get_enabled == nullptr || g_renderer_set_enabled == nullptr ||
        g_renderer_get_material == nullptr || g_material_get_color == nullptr ||
        g_material_set_color == nullptr) {
        RestoreChams(cache);
        return;
    }
    __try {
        if (!cache.chams_applied) {
            cache.outline_game_object = g_component_get_game_object(cache.outline_renderer, nullptr);
            if (cache.outline_game_object == nullptr) return;
            cache.original_outline_active = g_game_object_get_active_self(
                cache.outline_game_object, nullptr);
            cache.original_renderer_enabled = g_renderer_get_enabled(cache.outline_renderer, nullptr);
            cache.chams_material = g_renderer_get_material(cache.outline_renderer, nullptr);
            if (cache.chams_material == nullptr) return;
            cache.original_chams_color = g_material_get_color(cache.chams_material, nullptr);
            cache.chams_applied = true;
        }
        if (now - cache.chams_at < 80) return;
        cache.chams_at = now;
        const float pulse = 0.86f + 0.14f * std::sin(static_cast<float>(now % 2400) * 0.002618f);
        Color color = ally ? Color{0.05f, 0.72f * pulse, 1.0f * pulse, 1.0f}
                           : Color{1.0f * pulse, 0.04f, 0.22f, 1.0f};
        if (spawn_protected) color = {1.0f, 0.62f * pulse, 0.05f, 1.0f};
        // PlayerTeamOutline already owns the game's through-wall material.
        // Its GameObject is normally disabled by team logic, so both the
        // object and renderer must be kept active.
        g_game_object_set_active(cache.outline_game_object, true, nullptr);
        g_renderer_set_enabled(cache.outline_renderer, true, nullptr);
        g_material_set_color(cache.chams_material, color, nullptr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        RestoreChams(cache);
    }
}

bool IsSpawnProtected(void* player) {
    if (player == nullptr) return false;
    void* shield = *reinterpret_cast<void**>(static_cast<std::uint8_t*>(player) + 0x38);
    return IsGameObjectActiveSafe(shield);
}

bool HasClearLine(const Vector3& start, const Vector3& end, int collision_mask, int damage_mask) {
    if (g_physics_linecast == nullptr) return true;
    int world_mask = collision_mask & ~damage_mask;
    if (world_mask == 0) world_mask = collision_mask;
    if (world_mask == 0) return true;
    return !g_physics_linecast(&start, &end, world_mask, 1, nullptr);
}

PlayerCache& GetPlayerCache(void* player, ULONGLONG now) {
    PlayerCache* selected = nullptr;
    PlayerCache* oldest = &g_player_cache[0];
    for (PlayerCache& entry : g_player_cache) {
        if (entry.player == player) {
            selected = &entry;
            break;
        }
        if (entry.player == nullptr && selected == nullptr) selected = &entry;
        if (entry.last_seen_at < oldest->last_seen_at) oldest = &entry;
    }
    if (selected == nullptr) selected = oldest;

    const bool player_changed = selected->player != player;
    if (player_changed) {
        RestoreChams(*selected);
        *selected = {};
        selected->player = player;
        selected->visible = true;
    }
    selected->last_seen_at = now;

    const ULONGLONG refresh_interval = 1000 +
        ((reinterpret_cast<std::uintptr_t>(player) >> 4) & 0x1FFu);
    if (player_changed || now - selected->refreshed_at >= refresh_interval) {
        selected->transform = g_component_get_transform(player, nullptr);
        selected->collider = FindRelatedComponent(player, g_collider_reflection_type);
        if (selected->collider == nullptr) {
            selected->collider = FindRelatedComponent(player, g_any_collider_reflection_type);
        }
        if (!selected->chams_applied) {
            GetOutlineSignature(player, selected->outline_renderer,
                selected->outline_material, selected->outline_color);
        }
        selected->has_team_id = ReadTeamIdSafe(player, selected->team_id);
        CopyUsername(player, selected->name, static_cast<int>(sizeof(selected->name)));
        selected->refreshed_at = now;
    }
    return *selected;
}

bool ProjectBounds(void* player, void* collider, void* camera, float display_width, float display_height,
                   float& left, float& top, float& right, float& bottom, Vector3& head, Vector3& aim_point) {
    const auto project_fallback = [&]() -> bool {
        void* transform = g_component_get_transform(player, nullptr);
        if (transform == nullptr) return false;
        const Vector3 feet = g_transform_get_position(transform, nullptr);
        head = {feet.x, feet.y + 1.7f, feet.z};
        aim_point = {feet.x, feet.y + 1.0f, feet.z};
        Vector3 feet_screen = g_world_to_screen(camera, feet, nullptr);
        Vector3 head_screen = g_world_to_screen(camera, head, nullptr);
        if (feet_screen.z <= 0.15f || head_screen.z <= 0.15f) return false;
        feet_screen.y = display_height - feet_screen.y;
        head_screen.y = display_height - head_screen.y;
        const float height = std::fabs(feet_screen.y - head_screen.y);
        if (!std::isfinite(height) || height < 5.0f || height > display_height * 0.78f) return false;
        const float width = height * 0.46f;
        const float center = (feet_screen.x + head_screen.x) * 0.5f;
        left = center - width * 0.5f;
        right = center + width * 0.5f;
        top = std::min(feet_screen.y, head_screen.y);
        bottom = std::max(feet_screen.y, head_screen.y);
        return right >= 0.0f && left <= display_width && bottom >= 0.0f && top <= display_height;
    };

    if (collider == nullptr) return project_fallback();
    const Bounds bounds = g_collider_get_bounds(collider, nullptr);
    if (!std::isfinite(bounds.center.x) || !std::isfinite(bounds.center.y) || !std::isfinite(bounds.center.z) ||
        !std::isfinite(bounds.extents.x) || !std::isfinite(bounds.extents.y) || !std::isfinite(bounds.extents.z) ||
        bounds.extents.x <= 0.01f || bounds.extents.y <= 0.05f || bounds.extents.z <= 0.01f ||
        bounds.extents.x > 1.5f || bounds.extents.y > 3.0f || bounds.extents.z > 1.5f) {
        return project_fallback();
    }

    const Vector3 feet{bounds.center.x, bounds.center.y - bounds.extents.y, bounds.center.z};
    head = {bounds.center.x, bounds.center.y + bounds.extents.y, bounds.center.z};
    // Keep the ray safely inside the damage collider. Aiming exactly at the
    // top edge can produce a visual hit while the authoritative ray misses.
    aim_point = {bounds.center.x, bounds.center.y + bounds.extents.y * 0.22f, bounds.center.z};
    Vector3 feet_screen = g_world_to_screen(camera, feet, nullptr);
    Vector3 head_screen = g_world_to_screen(camera, head, nullptr);
    if (!std::isfinite(feet_screen.x) || !std::isfinite(feet_screen.y) || feet_screen.z <= 0.15f ||
        !std::isfinite(head_screen.x) || !std::isfinite(head_screen.y) || head_screen.z <= 0.15f) {
        return project_fallback();
    }
    feet_screen.y = display_height - feet_screen.y;
    head_screen.y = display_height - head_screen.y;

    const float height = std::fabs(feet_screen.y - head_screen.y);
    const float aspect = std::clamp(bounds.extents.x / bounds.extents.y, 0.38f, 0.58f);
    const float width = height * aspect;
    const float center_x = (feet_screen.x + head_screen.x) * 0.5f;
    left = center_x - width * 0.5f;
    right = center_x + width * 0.5f;
    top = std::min(feet_screen.y, head_screen.y);
    bottom = std::max(feet_screen.y, head_screen.y);
    if (!std::isfinite(width) || !std::isfinite(height) || width < 3.0f || height < 5.0f ||
        width > display_width * 0.70f || height > display_height * 0.92f ||
        right < 0.0f || left > display_width || bottom < 0.0f || top > display_height) {
        return project_fallback();
    }
    left = std::clamp(left, 0.0f, display_width);
    right = std::clamp(right, 0.0f, display_width);
    top = std::clamp(top, 0.0f, display_height);
    bottom = std::clamp(bottom, 0.0f, display_height);
    return right > left && bottom > top;
}

ShotInfoBackup CaptureShotInfo(void* shot_info) {
    ShotInfoBackup backup{};
    backup.object = shot_info;
    if (shot_info == nullptr) return backup;
    const auto address = reinterpret_cast<std::uintptr_t>(shot_info);
    backup.apply_spread_before_shooting = *reinterpret_cast<bool*>(address + 0x18);
    backup.bullet_spread = *reinterpret_cast<float*>(address + 0x1C);
    backup.ads_bullet_spread = *reinterpret_cast<float*>(address + 0x20);
    backup.camera_shake = *reinterpret_cast<float*>(address + 0x30);
    backup.ads_camera_shake = *reinterpret_cast<float*>(address + 0x34);
    return backup;
}

void ZeroShotInfo(const ShotInfoBackup& backup) {
    if (backup.object == nullptr) return;
    const auto address = reinterpret_cast<std::uintptr_t>(backup.object);
    *reinterpret_cast<bool*>(address + 0x18) = false;
    *reinterpret_cast<float*>(address + 0x1C) = 0.0f;
    *reinterpret_cast<float*>(address + 0x20) = 0.0f;
    *reinterpret_cast<float*>(address + 0x30) = 0.0f;
    *reinterpret_cast<float*>(address + 0x34) = 0.0f;
}

void RestoreShotInfo(const ShotInfoBackup& backup) {
    if (backup.object == nullptr) return;
    const auto address = reinterpret_cast<std::uintptr_t>(backup.object);
    *reinterpret_cast<bool*>(address + 0x18) = backup.apply_spread_before_shooting;
    *reinterpret_cast<float*>(address + 0x1C) = backup.bullet_spread;
    *reinterpret_cast<float*>(address + 0x20) = backup.ads_bullet_spread;
    *reinterpret_cast<float*>(address + 0x30) = backup.camera_shake;
    *reinterpret_cast<float*>(address + 0x34) = backup.ads_camera_shake;
}

void RestoreNoSpreadState() {
    __try {
        if (g_no_spread_backup.player != nullptr) {
            *reinterpret_cast<float*>(reinterpret_cast<std::uintptr_t>(g_no_spread_backup.player) + 0x168) =
                g_no_spread_backup.player_spread;
        }
        if (g_no_spread_backup.item != nullptr) {
            *reinterpret_cast<float*>(reinterpret_cast<std::uintptr_t>(g_no_spread_backup.item) + 0x30) =
                g_no_spread_backup.item_spread;
        }
        if (g_no_spread_backup.info != nullptr) {
            const auto info = reinterpret_cast<std::uintptr_t>(g_no_spread_backup.info);
            *reinterpret_cast<float*>(info + 0x60) = g_no_spread_backup.max_bullet_spread;
            *reinterpret_cast<float*>(info + 0x68) = g_no_spread_backup.normal_spread;
            *reinterpret_cast<float*>(info + 0x6C) = g_no_spread_backup.ads_spread;
            *reinterpret_cast<float*>(info + 0x70) = g_no_spread_backup.movement_spread_multiplier;
            *reinterpret_cast<float*>(info + 0x74) = g_no_spread_backup.ads_movement_spread_multiplier;
        }
        RestoreShotInfo(g_no_spread_backup.primary);
        RestoreShotInfo(g_no_spread_backup.secondary);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    g_no_spread_backup = {};
    g_no_spread_was_enabled = false;
}

void CaptureNoSpreadState(void* player, void* item) {
    RestoreNoSpreadState();
    g_no_spread_backup.player = player;
    g_no_spread_backup.item = item;
    g_no_spread_backup.player_spread = *reinterpret_cast<float*>(
        reinterpret_cast<std::uintptr_t>(player) + 0x168);
    if (item != nullptr) {
        const auto item_address = reinterpret_cast<std::uintptr_t>(item);
        g_no_spread_backup.item_spread = *reinterpret_cast<float*>(item_address + 0x30);
        g_no_spread_backup.info = *reinterpret_cast<void**>(item_address + 0x18);
    }
    if (g_no_spread_backup.info != nullptr) {
        const auto info = reinterpret_cast<std::uintptr_t>(g_no_spread_backup.info);
        g_no_spread_backup.max_bullet_spread = *reinterpret_cast<float*>(info + 0x60);
        g_no_spread_backup.normal_spread = *reinterpret_cast<float*>(info + 0x68);
        g_no_spread_backup.ads_spread = *reinterpret_cast<float*>(info + 0x6C);
        g_no_spread_backup.movement_spread_multiplier = *reinterpret_cast<float*>(info + 0x70);
        g_no_spread_backup.ads_movement_spread_multiplier = *reinterpret_cast<float*>(info + 0x74);
        g_no_spread_backup.primary = CaptureShotInfo(*reinterpret_cast<void**>(info + 0x30));
        g_no_spread_backup.secondary = CaptureShotInfo(*reinterpret_cast<void**>(info + 0x38));
    }
    g_no_spread_was_enabled = true;
}

void ApplyNoSpreadState() {
    if (!g_no_spread_was_enabled) return;
    if (g_no_spread_backup.player != nullptr) {
        *reinterpret_cast<float*>(reinterpret_cast<std::uintptr_t>(g_no_spread_backup.player) + 0x168) = 0.0f;
    }
    if (g_no_spread_backup.item != nullptr) {
        *reinterpret_cast<float*>(reinterpret_cast<std::uintptr_t>(g_no_spread_backup.item) + 0x30) = 0.0f;
    }
    if (g_no_spread_backup.info != nullptr) {
        const auto info = reinterpret_cast<std::uintptr_t>(g_no_spread_backup.info);
        *reinterpret_cast<float*>(info + 0x60) = 0.0f;
        *reinterpret_cast<float*>(info + 0x68) = 0.0f;
        *reinterpret_cast<float*>(info + 0x6C) = 0.0f;
        *reinterpret_cast<float*>(info + 0x70) = 0.0f;
        *reinterpret_cast<float*>(info + 0x74) = 0.0f;
    }
    ZeroShotInfo(g_no_spread_backup.primary);
    ZeroShotInfo(g_no_spread_backup.secondary);
}

void ApplyLocalFeatures(void* local_player) {
    g_one_hit_enabled.store(g_weapon.one_hit, std::memory_order_relaxed);
    if (local_player == nullptr) {
        aimbot::SetWallShot(false, 0);
        movement::Apply(nullptr);
        return;
    }
    const int damage_layer_mask = *reinterpret_cast<int*>(
        static_cast<std::uint8_t*>(local_player) + 0x1C);
    aimbot::SetWallShot(g_weapon.wall_shot, damage_layer_mask);
    void* item = FindActiveItem(local_player);
    if (g_weapon.no_spread) {
        if (!g_no_spread_was_enabled || g_no_spread_backup.player != local_player ||
            g_no_spread_backup.item != item) {
            CaptureNoSpreadState(local_player, item);
        }
        ApplyNoSpreadState();
    } else if (g_no_spread_was_enabled) {
        RestoreNoSpreadState();
    }
    if (item != nullptr && g_weapon.infinite_ammo) {
        int magazine = 30;
        void* info = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(item) + 0x18);
        if (info != nullptr) {
            const int configured = *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(info) + 0x48);
            if (configured > 0 && configured < 10000) magazine = configured;
        }
        WriteObfuscatedInt(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(item) + 0x34), magazine);
        WriteObfuscatedInt(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(item) + 0x3C), 999);
    }
    if (item != nullptr && g_weapon.rapid_fire) {
        *reinterpret_cast<float*>(reinterpret_cast<std::uintptr_t>(item) + 0x44) = 0.0f;
    }
    movement::Apply(local_player);
}

std::size_t GatherBoxes(ScreenBox* boxes, std::size_t capacity, float display_width, float display_height) {
    std::size_t count = 0;
    Vector3 selected_origin{};
    Vector3 selected_target{};
    bool selected_valid = false;
    float selected_screen_distance = g_aim.fov_radius;
    const auto publish_target = [&]() {
        aimbot::SetTarget(
            {selected_origin.x, selected_origin.y, selected_origin.z},
            {selected_target.x, selected_target.y, selected_target.z},
            selected_valid);
    };

    __try {
        if (!InitializeRuntime()) {
            publish_target();
            return 0;
        }

        void* local_player = nullptr;
        g_field_static_get_value(g_local_instance_field, &local_player);
        g_local_player.store(local_player, std::memory_order_relaxed);
        movement::SetLocalPlayer(local_player);
        ApplyLocalFeatures(local_player);

        void* camera = g_camera_get_main(nullptr);
        if (camera == nullptr) {
            publish_target();
            return 0;
        }

        const ULONGLONG now = GetTickCount64();
        if (g_last_player_scan == 0 || now - g_last_player_scan >= 100) {
            Il2CppArray* players = g_find_objects_of_type(g_player_reflection_type, nullptr);
            if (players == nullptr || players->max_length > 4096) {
                publish_target();
                return 0;
            }
            g_player_snapshot_count = std::min<std::size_t>(
                static_cast<std::size_t>(players->max_length), kMaxPlayers);
            for (std::size_t i = 0; i < g_player_snapshot_count; ++i) {
                g_player_snapshot[i] = players->vector[i];
            }
            g_last_player_scan = now;
        }
        PlayerCache* local_cache = local_player != nullptr ? &GetPlayerCache(local_player, now) : nullptr;
        bool team_signatures_reliable = false;
        if (local_cache != nullptr && !local_cache->has_team_id && local_cache->outline_material != nullptr) {
            for (std::size_t i = 0; i < g_player_snapshot_count; ++i) {
                void* candidate = g_player_snapshot[i];
                if (candidate == nullptr || candidate == local_player) continue;
                PlayerCache& candidate_cache = GetPlayerCache(candidate, now);
                if (candidate_cache.outline_material != nullptr && !IsSameTeam(
                        candidate_cache.outline_material, candidate_cache.outline_color,
                        local_cache->outline_material, local_cache->outline_color)) {
                    team_signatures_reliable = true;
                    break;
                }
            }
        }

        Vector3 local_position{};
        bool have_local_position = false;
        if (local_cache != nullptr) {
            void* local_transform = local_cache->transform;
            if (local_transform != nullptr) {
                local_position = g_transform_get_position(local_transform, nullptr);
                have_local_position = true;
            }
        }

        Vector3 shot_origin{};
        bool have_shot_origin = false;
        if (local_player != nullptr) {
            // PlayerController::cameraHolder is at +0x30. The hooked shooting
            // routine starts its Physics.Raycast from this exact transform.
            void* camera_holder = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(local_player) + 0x30);
            if (camera_holder != nullptr) {
                void* origin_transform = g_game_object_get_transform(camera_holder, nullptr);
                if (origin_transform != nullptr) {
                    shot_origin = g_transform_get_position(origin_transform, nullptr);
                    have_shot_origin = true;
                }
            }
        }
        if (!have_shot_origin && have_local_position) {
            shot_origin = local_position;
            have_shot_origin = true;
        }

        int collision_mask = 0;
        int damage_mask = 0;
        if (local_player != nullptr) {
            collision_mask = *reinterpret_cast<int*>(static_cast<std::uint8_t*>(local_player) + 0x18);
            damage_mask = *reinterpret_cast<int*>(static_cast<std::uint8_t*>(local_player) + 0x1C);
            collision_mask = movement::EffectiveCollisionMask(local_player, collision_mask);
        }

        const std::size_t player_count = g_player_snapshot_count;
        for (std::size_t i = 0; i < player_count && count < capacity; ++i) {
            void* player = g_player_snapshot[i];
            if (player == nullptr || player == local_player) {
                continue;
            }

            PlayerCache& cache = GetPlayerCache(player, now);
            void* transform = cache.transform;
            if (transform == nullptr) {
                continue;
            }

            const Vector3 feet = g_transform_get_position(transform, nullptr);
            float left = 0.0f;
            float top = 0.0f;
            float right = 0.0f;
            float bottom = 0.0f;
            Vector3 head{};
            Vector3 aim_point{};
            if (!ProjectBounds(player, cache.collider, camera, display_width, display_height,
                               left, top, right, bottom, head, aim_point)) {
                continue;
            }
            ScreenBox& box = boxes[count++];
            box.left = left;
            box.top = top;
            box.right = right;
            box.bottom = bottom;
            box.distance = 0.0f;
            box.ammo = -1;
            box.total_ammo = -1;
            box.magazine_size = -1;
            box.ally = false;
            box.visible = true;
            box.spawn_protected = false;
            box.head_world = head;
            box.name[0] = '\0';

            if (have_local_position) {
                const float dx = feet.x - local_position.x;
                const float dy = feet.y - local_position.y;
                const float dz = feet.z - local_position.z;
                box.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            }

            if (cache.dynamic_at == 0 || now - cache.dynamic_at >= 50) {
                ReadAmmoSafe(player, cache.ammo, cache.total_ammo, cache.magazine_size);
                cache.dynamic_at = now;
            }
            box.ammo = cache.ammo;
            box.total_ammo = cache.total_ammo;
            box.magazine_size = cache.magazine_size;

            if (local_cache != nullptr && local_cache->has_team_id && cache.has_team_id) {
                box.ally = cache.team_id == local_cache->team_id;
            } else {
                box.ally = team_signatures_reliable && local_cache != nullptr && IsSameTeam(
                    cache.outline_material, cache.outline_color,
                    local_cache->outline_material, local_cache->outline_color);
            }
            std::memcpy(box.name, cache.name, sizeof(box.name));

            if (cache.visibility_at == 0 || now - cache.visibility_at >= 50) {
                cache.spawn_protected = IsSpawnProtected(player);
                cache.visible = !have_shot_origin ||
                    HasClearLine(shot_origin, aim_point, collision_mask, damage_mask);
                cache.visibility_at = now;
            }
            box.visible = cache.visible;
            box.spawn_protected = cache.spawn_protected;
            const Settings& team_settings = box.ally ? g_allies : g_enemies;
            ApplyChams(cache,
                team_settings.enabled && team_settings.chams &&
                    (!team_settings.visible_only || box.visible),
                box.ally, box.spawn_protected, now);

            if (g_aim.enabled && !box.ally && have_shot_origin &&
                (!g_aim.visible_only || box.visible || g_weapon.wall_shot) &&
                (!g_aim.ignore_spawn_shield || !box.spawn_protected) &&
                (g_aim.max_distance <= 0.0f || box.distance <= g_aim.max_distance)) {
                const float sx = (left + right) * 0.5f - display_width * 0.5f;
                const float sy = top - display_height * 0.5f;
                const float screen_distance = std::sqrt(sx * sx + sy * sy);
                if (screen_distance <= selected_screen_distance) {
                    selected_screen_distance = screen_distance;
                    selected_origin = shot_origin;
                    selected_target = aim_point;
                    selected_valid = local_player != nullptr;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        publish_target();
        ResetEspCaches();
        g_player_reflection_type = nullptr;
        g_team_outline_reflection_type = nullptr;
        g_username_reflection_type = nullptr;
        g_collider_reflection_type = nullptr;
        g_any_collider_reflection_type = nullptr;
        g_local_instance_field = nullptr;
        return 0;
    }

    publish_target();
    return count;
}

void DrawOutlinedText(ImDrawList* draw, const ImVec2& position, ImU32 color, const char* text, bool centered) {
    ImVec2 pos = position;
    if (centered) {
        pos.x -= ImGui::CalcTextSize(text).x * 0.5f;
    }
    const ImU32 shadow = IM_COL32(0, 0, 0, 235);
    draw->AddText(ImVec2(pos.x - 1.0f, pos.y), shadow, text);
    draw->AddText(ImVec2(pos.x + 1.0f, pos.y), shadow, text);
    draw->AddText(ImVec2(pos.x, pos.y - 1.0f), shadow, text);
    draw->AddText(ImVec2(pos.x, pos.y + 1.0f), shadow, text);
    draw->AddText(pos, color, text);
}

float EspElementScale(float height) {
    return std::clamp(height / 150.0f, 0.28f, 1.0f);
}

float EspBarThickness(float height) {
    return std::clamp(height * 0.026f, 1.25f, 4.0f);
}

float EspBarGap(float height) {
    return std::clamp(height * 0.028f, 2.0f, 5.0f);
}

void DrawGradientText(ImDrawList* draw, const ImVec2& pos, const char* text, ImU32 left, ImU32 right) {
    if (text == nullptr || *text == '\0') return;
    const ImVec2 size = ImGui::CalcTextSize(text);
    draw->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.35f), IM_COL32(0, 0, 0, 185), text);
    draw->AddText(ImVec2(pos.x + 2.0f, pos.y + 2.35f), IM_COL32(0, 0, 0, 105), text);
    const int start = draw->VtxBuffer.Size;
    draw->AddText(pos, IM_COL32_WHITE, text);
    const int end = draw->VtxBuffer.Size;
    const float phase = std::sin(static_cast<float>(ImGui::GetTime()) * 2.35f) * size.x * 0.35f;
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(
        draw, start, end, ImVec2(pos.x + phase, pos.y), ImVec2(pos.x + size.x + phase, pos.y), left, right);
}

void DrawStatusBar(ImDrawList* draw, float x, float y, float w, float h, float value, float maximum,
                   bool vertical, ImU32 top, ImU32 bottom) {
    if (maximum <= 0.0f) return;
    const float thickness = EspBarThickness(h);
    const float gap = EspBarGap(h);
    const float pct = std::clamp(value / maximum, 0.0f, 1.0f);
    const float inset = thickness >= 2.75f ? 1.0f : 0.0f;
    const float rounding = h < 45.0f ? 1.0f : 2.0f;
    const ImU32 bg = IM_COL32(0, 0, 0, h < 45.0f ? 95 : 135);
    const ImU32 border = IM_COL32(0, 0, 0, h < 45.0f ? 170 : 215);

    if (vertical) {
        const ImVec2 min(x - gap - thickness, y);
        const ImVec2 max(x - gap, y + h);
        draw->AddRectFilled(min, max, bg, rounding);
        draw->AddRect(ImVec2(min.x - 1.0f, min.y - 1.0f), ImVec2(max.x + 1.0f, max.y + 1.0f), border, rounding);
        if (pct > 0.0f) {
            const ImVec2 fill_min(min.x + inset, max.y - h * pct + inset);
            const ImVec2 fill_max(max.x - inset, max.y - inset);
            draw->AddRectFilledMultiColor(fill_min, fill_max, top, top, bottom, bottom);
        }
    } else {
        const ImVec2 min(x, y + h + gap);
        const ImVec2 max(x + w, min.y + thickness);
        draw->AddRectFilled(min, max, bg, rounding);
        draw->AddRect(ImVec2(min.x - 1.0f, min.y - 1.0f), ImVec2(max.x + 1.0f, max.y + 1.0f), border, rounding);
        if (pct > 0.0f) {
            ImVec2 fill_max(min.x + w * pct - inset, max.y - inset);
            const ImVec2 fill_min(min.x + inset, min.y + inset);
            if (fill_max.x < fill_min.x) fill_max.x = fill_min.x;
            draw->AddRectFilledMultiColor(fill_min, fill_max, top, bottom, bottom, top);
        }
    }
}
} // namespace

Settings* GetSettings(Team team) {
    return team == Team::Allies ? &g_allies : &g_enemies;
}

WeaponSettings* GetWeaponSettings() {
    return &g_weapon;
}

void Render() {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    aimbot::UpdateRuntimeSettings();
    aimbot::SetNoSpread(g_weapon.no_spread);
    const bool runtime_features = g_weapon.no_spread || g_weapon.infinite_ammo || g_weapon.rapid_fire ||
        g_weapon.one_hit || g_weapon.wall_shot ||
        g_no_spread_was_enabled || movement::IsActive();
    if (!g_allies.enabled && !g_enemies.enabled && !g_aim.enabled && !runtime_features) {
        for (PlayerCache& cache : g_player_cache) RestoreChams(cache);
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        return;
    }

    // Transform positions are cheap and must follow the render cadence. The
    // expensive object/component discovery remains cached inside GatherBoxes.
    g_cached_box_count = GatherBoxes(g_cached_boxes, kMaxPlayers, io.DisplaySize.x, io.DisplaySize.y);
    g_last_esp_update = GetTickCount64();
    g_cached_display_width = io.DisplaySize.x;
    g_cached_display_height = io.DisplaySize.y;
    const ScreenBox* boxes = g_cached_boxes;
    const std::size_t count = g_cached_box_count;
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (g_aim.enabled && g_aim.draw_fov) {
        const float alpha = !g_aim.require_rmb || (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 1.0f : 0.3f;
        draw->AddCircle(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), g_aim.fov_radius,
            IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha)), 64, 1.5f);
    }
    const ImU32 outline = IM_COL32(0, 0, 0, 255);
    for (std::size_t i = 0; i < count; ++i) {
        const ScreenBox& box = boxes[i];
        const Settings& settings = box.ally ? g_allies : g_enemies;
        if (!settings.enabled || (settings.visible_only && !box.visible)) {
            continue;
        }

        const ImU32 color = box.ally ? IM_COL32(51, 230, 148, 255) : IM_COL32(255, 255, 255, 255);
        const ImVec2 min(box.left, box.top);
        const ImVec2 max(box.right, box.bottom);
        if (settings.box) {
            draw->AddRect(ImVec2(box.left - 1.0f, box.top - 1.0f), ImVec2(box.right + 1.0f, box.bottom + 1.0f), outline);
            draw->AddRect(ImVec2(box.left + 1.0f, box.top + 1.0f), ImVec2(box.right - 1.0f, box.bottom - 1.0f), outline);
            draw->AddRect(min, max, color);
        }

        if (settings.name && box.name[0] != '\0') {
            ImFont* font = ImGui::GetFont();
            const float old_scale = font->Scale;
            font->Scale = std::clamp((box.right - box.left) / 105.0f, 0.68f, 0.96f);
            ImGui::PushFont(font);
            const ImVec2 text_size = ImGui::CalcTextSize(box.name);
            const ImVec2 text_pos((box.left + box.right - text_size.x) * 0.5f,
                box.top - text_size.y - std::max(4.0f, 6.0f * EspElementScale(box.bottom - box.top)));
            DrawGradientText(draw, text_pos, box.name, IM_COL32(242, 245, 255, 255), IM_COL32(163, 176, 255, 255));
            ImGui::PopFont();
            font->Scale = old_scale;
        }

        if (settings.ammo && box.ammo >= 0 && box.magazine_size > 0) {
            DrawStatusBar(draw, box.left, box.top, box.right - box.left, box.bottom - box.top,
                static_cast<float>(std::min(box.ammo, box.magazine_size)), static_cast<float>(box.magazine_size), false,
                IM_COL32(255, 209, 71, 255), IM_COL32(255, 117, 41, 255));
            char ammo_value[32]{};
            _snprintf_s(ammo_value, sizeof(ammo_value), _TRUNCATE, "AMMO %d/%d", box.ammo, box.total_ammo);
            DrawOutlinedText(draw, ImVec2(box.right + 5.0f, box.top),
                IM_COL32(255, 209, 71, 255), ammo_value, false);
        }
        if (settings.spawn_shield && box.spawn_protected) {
            DrawOutlinedText(draw, ImVec2(box.right + 5.0f, box.top + 16.0f),
                IM_COL32(255, 177, 45, 255), "SPAWN SHIELD", false);
        }
        float bottom_text_y = box.bottom + (settings.ammo ? EspBarGap(box.bottom - box.top) + EspBarThickness(box.bottom - box.top) + 4.0f : 3.0f);
        if (settings.distance && box.distance > 0.0f && std::isfinite(box.distance)) {
            char value[32]{};
            _snprintf_s(value, sizeof(value), _TRUNCATE, "[%.0fm]", box.distance);
            DrawOutlinedText(draw, ImVec2((box.left + box.right) * 0.5f, bottom_text_y), IM_COL32(205, 207, 216, 255), value, true);
        }
        if (settings.snapline) {
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            draw->AddLine(ImVec2(screen.x * 0.5f, screen.y - 2.0f), ImVec2((box.left + box.right) * 0.5f, box.bottom), outline, 3.0f);
            draw->AddLine(ImVec2(screen.x * 0.5f, screen.y - 2.0f), ImVec2((box.left + box.right) * 0.5f, box.bottom), color, 1.0f);
        }
    }
}

void Shutdown() {
    aimbot::Shutdown();
    g_local_player.store(nullptr, std::memory_order_release);
    movement::SetLocalPlayer(nullptr);
    g_allies.enabled = false;
    g_enemies.enabled = false;
    g_weapon = {};
    __try {
        if (g_local_instance_field != nullptr && g_camera_get_main != nullptr) {
            void* local_player = nullptr;
            g_field_static_get_value(g_local_instance_field, &local_player);
            ApplyLocalFeatures(local_player);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    RestoreNoSpreadState();
    movement::Shutdown();
    RemoveDamageHook();
    ResetEspCaches();
    g_game_assembly = nullptr;
    g_player_reflection_type = nullptr;
    g_team_outline_reflection_type = nullptr;
    g_username_reflection_type = nullptr;
    g_collider_reflection_type = nullptr;
    g_any_collider_reflection_type = nullptr;
    g_local_instance_field = nullptr;
}

} // namespace esp
#else
namespace esp {
namespace {
Settings g_allies{};
Settings g_enemies{};
WeaponSettings g_weapon{};
}
Settings* GetSettings(Team team) { return team == Team::Allies ? &g_allies : &g_enemies; }
WeaponSettings* GetWeaponSettings() { return &g_weapon; }
void Render() {}
void Shutdown() { g_allies.enabled = false; g_enemies.enabled = false; }
} // namespace esp
#endif
