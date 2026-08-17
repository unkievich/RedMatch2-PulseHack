#include "features/movement.h"

#if defined(_WIN32)
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace movement {
namespace {
constexpr std::uintptr_t kComponentGetTransformRva = 0xBA5930;
constexpr std::uintptr_t kComponentGetGameObjectRva = 0xBA58F0;
constexpr std::uintptr_t kGameObjectGetTransformRva = 0xBA9EB0;
constexpr std::uintptr_t kGameObjectGetLayerRva = 0xBA9E70;
constexpr std::uintptr_t kTransformGetLocalPositionRva = 0xB3DA90;
constexpr std::uintptr_t kTransformSetLocalPositionRva = 0xB3E420;
constexpr std::uintptr_t kTransformGetLocalRotationRva = 0xB3DB30;
constexpr std::uintptr_t kTransformSetLocalRotationRva = 0xB3E4C0;
constexpr std::uintptr_t kTransformGetRightRva = 0xB3DE10;
constexpr std::uintptr_t kTransformGetForwardRva = 0xB3D850;
constexpr std::uintptr_t kCameraGetCullingMaskRva = 0xBA2A00;
constexpr std::uintptr_t kCameraSetCullingMaskRva = 0xBA34D0;
constexpr std::uintptr_t kRendererGetEnabledRva = 0xB322C0;
constexpr std::uintptr_t kRendererSetEnabledRva = 0xB32400;
constexpr std::uintptr_t kRendererGetShadowCastingModeRva = 0xB32340;
constexpr std::uintptr_t kRendererSetShadowCastingModeRva = 0xB324A0;
constexpr std::uintptr_t kRigidbodyGetVelocityRva = 0x10974F0;
constexpr std::uintptr_t kRigidbodySetVelocityRva = 0x1097990;
constexpr std::uintptr_t kRigidbodySetUseGravityRva = 0x10978F0;
constexpr std::uintptr_t kRigidbodySetDetectCollisionsRva = 0x1097670;
constexpr std::uintptr_t kPlayerFixedUpdateRva = 0x1103FF0;
constexpr std::size_t kPlayerFixedUpdatePatchSize = 18;
constexpr std::size_t kMaxLocalBodyRenderers = 32;

struct Vector3 {
    float x;
    float y;
    float z;
};

struct Quaternion {
    float x;
    float y;
    float z;
    float w;
};

struct Il2CppArray {
    void* klass;
    void* monitor;
    void* bounds;
    std::uintptr_t max_length;
    void* vector[1];
};

using ComponentGetTransform = void* (*)(void* component, const void* method);
using ComponentGetGameObject = void* (*)(void* component, const void* method);
using GameObjectGetTransform = void* (*)(void* game_object, const void* method);
using GameObjectGetLayer = int (*)(void* game_object, const void* method);
using TransformGetPosition = Vector3 (*)(void* transform, const void* method);
using TransformSetPosition = void (*)(void* transform, Vector3 position, const void* method);
using TransformGetRotation = Quaternion (*)(void* transform, const void* method);
using TransformSetRotation = void (*)(void* transform, Quaternion rotation, const void* method);
using TransformGetDirection = Vector3 (*)(void* transform, const void* method);
using CameraGetCullingMask = int (*)(void* camera, const void* method);
using CameraSetCullingMask = void (*)(void* camera, int mask, const void* method);
using RendererGetEnabled = bool (*)(void* renderer, const void* method);
using RendererSetEnabled = void (*)(void* renderer, bool enabled, const void* method);
using RendererGetShadowCastingMode = int (*)(void* renderer, const void* method);
using RendererSetShadowCastingMode = void (*)(void* renderer, int mode, const void* method);
using RigidbodyGetVelocity = Vector3 (*)(void* rigidbody, const void* method);
using RigidbodySetVelocity = void (*)(void* rigidbody, Vector3 value, const void* method);
using RigidbodySetUseGravity = void (*)(void* rigidbody, bool value, const void* method);
using RigidbodySetDetectCollisions = void (*)(void* rigidbody, bool value, const void* method);
using PlayerFixedUpdate = void (*)(void* player, const void* method);

struct LocalBodyRendererState {
    void* renderer = nullptr;
    bool enabled = false;
    int shadow_casting_mode = 0;
};

Settings g_settings{false, 12.0f, false, 18.0f, false, 3.0f, false, 720.0f};
HMODULE g_game_assembly = nullptr;
ComponentGetTransform g_component_get_transform = nullptr;
ComponentGetGameObject g_component_get_game_object = nullptr;
GameObjectGetTransform g_game_object_get_transform = nullptr;
GameObjectGetLayer g_game_object_get_layer = nullptr;
TransformGetPosition g_transform_get_local_position = nullptr;
TransformSetPosition g_transform_set_local_position = nullptr;
TransformGetRotation g_transform_get_local_rotation = nullptr;
TransformSetRotation g_transform_set_local_rotation = nullptr;
TransformGetDirection g_transform_get_right = nullptr;
TransformGetDirection g_transform_get_forward = nullptr;
CameraGetCullingMask g_camera_get_culling_mask = nullptr;
CameraSetCullingMask g_camera_set_culling_mask = nullptr;
RendererGetEnabled g_renderer_get_enabled = nullptr;
RendererSetEnabled g_renderer_set_enabled = nullptr;
RendererGetShadowCastingMode g_renderer_get_shadow_casting_mode = nullptr;
RendererSetShadowCastingMode g_renderer_set_shadow_casting_mode = nullptr;
RigidbodyGetVelocity g_rigidbody_get_velocity = nullptr;
RigidbodySetVelocity g_rigidbody_set_velocity = nullptr;
RigidbodySetUseGravity g_rigidbody_set_use_gravity = nullptr;
RigidbodySetDetectCollisions g_rigidbody_set_detect_collisions = nullptr;

std::atomic<void*> g_local_player{nullptr};
std::atomic_bool g_hook_stopping{false};
std::atomic_ulong g_hook_calls{0};
PlayerFixedUpdate g_original_fixed_update = nullptr;
void* g_fixed_update_target = nullptr;
void* g_fixed_update_trampoline = nullptr;
std::uint8_t g_fixed_update_original[kPlayerFixedUpdatePatchSize]{};

bool g_gravity_was_disabled = false;
bool g_collisions_were_disabled = false;
void* g_fly_player = nullptr;
void* g_fly_rigidbody = nullptr;
int g_original_collision_mask = 0;

void* g_third_person_player = nullptr;
void* g_third_person_camera_transform = nullptr;
void* g_third_person_camera = nullptr;
Vector3 g_original_camera_local_position{};
int g_original_camera_culling_mask = 0;
bool g_third_person_applied = false;
LocalBodyRendererState g_local_body_renderers[kMaxLocalBodyRenderers]{};
std::size_t g_local_body_renderer_count = 0;

void* g_spin_visual_transform = nullptr;
Quaternion g_original_spin_visual_rotation{};
bool g_spin_visual_applied = false;
ULONGLONG g_spin_started_at = 0;

void WriteAbsoluteJump(void* destination, const void* target) {
    auto* bytes = static_cast<std::uint8_t*>(destination);
    bytes[0] = 0xFF;
    bytes[1] = 0x25;
    *reinterpret_cast<std::uint32_t*>(bytes + 2) = 0;
    *reinterpret_cast<std::uintptr_t*>(bytes + 6) = reinterpret_cast<std::uintptr_t>(target);
}

bool IsMovementKeyDown(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

float VectorLengthSquared(const Vector3& value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

Vector3 NormalizeVector(Vector3 value) {
    const float length_squared = VectorLengthSquared(value);
    if (length_squared <= 0.0001f || !std::isfinite(length_squared)) return {};
    const float inverse_length = 1.0f / std::sqrt(length_squared);
    value.x *= inverse_length;
    value.y *= inverse_length;
    value.z *= inverse_length;
    return value;
}

Vector3 ReadMovementDirection(void* local_player, bool include_vertical) {
    void* direction_transform = nullptr;
    void* camera_holder = *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(local_player) + 0x30);
    if (camera_holder != nullptr) {
        direction_transform = g_game_object_get_transform(camera_holder, nullptr);
    }
    if (direction_transform == nullptr) {
        direction_transform = g_component_get_transform(local_player, nullptr);
    }
    if (direction_transform == nullptr) return {};

    Vector3 forward = g_transform_get_forward(direction_transform, nullptr);
    Vector3 right = g_transform_get_right(direction_transform, nullptr);
    forward.y = 0.0f;
    right.y = 0.0f;
    forward = NormalizeVector(forward);
    right = NormalizeVector(right);

    const float forward_input = (IsMovementKeyDown('W') ? 1.0f : 0.0f) -
        (IsMovementKeyDown('S') ? 1.0f : 0.0f);
    const float side_input = (IsMovementKeyDown('D') ? 1.0f : 0.0f) -
        (IsMovementKeyDown('A') ? 1.0f : 0.0f);
    const float vertical_input = include_vertical
        ? ((IsMovementKeyDown(VK_SPACE) ? 1.0f : 0.0f) -
           (IsMovementKeyDown(VK_CONTROL) ? 1.0f : 0.0f))
        : 0.0f;
    return NormalizeVector({
        forward.x * forward_input + right.x * side_input,
        vertical_input,
        forward.z * forward_input + right.z * side_input
    });
}

Quaternion CurrentSpinQuaternion() {
    const ULONGLONG now = GetTickCount64();
    if (g_spin_started_at == 0) g_spin_started_at = now;
    const float elapsed = static_cast<float>(now - g_spin_started_at) * 0.001f;
    const float yaw = std::fmod(
        elapsed * std::clamp(g_settings.spin_speed, 90.0f, 2160.0f), 360.0f);
    const float half_angle = yaw * 0.00872664626f;
    return {0.0f, std::sin(half_angle), 0.0f, std::cos(half_angle)};
}

Quaternion MultiplyQuaternion(const Quaternion& lhs, const Quaternion& rhs) {
    return {
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z
    };
}

void AddLocalBodyRenderer(void* renderer) {
    if (renderer == nullptr || g_local_body_renderer_count >= kMaxLocalBodyRenderers) return;
    for (std::size_t i = 0; i < g_local_body_renderer_count; ++i) {
        if (g_local_body_renderers[i].renderer == renderer) return;
    }
    LocalBodyRendererState& state = g_local_body_renderers[g_local_body_renderer_count++];
    state.renderer = renderer;
    state.enabled = g_renderer_get_enabled(renderer, nullptr);
    state.shadow_casting_mode = g_renderer_get_shadow_casting_mode(renderer, nullptr);
}

void CaptureLocalBodyRenderers(void* local_player) {
    g_local_body_renderer_count = 0;
    std::memset(g_local_body_renderers, 0, sizeof(g_local_body_renderers));
    AddLocalBodyRenderer(*reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(local_player) + 0xB8));
    AddLocalBodyRenderer(*reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(local_player) + 0xC0));

    auto* shadow_components = *reinterpret_cast<Il2CppArray**>(
        static_cast<std::uint8_t*>(local_player) + 0x240);
    if (shadow_components == nullptr || shadow_components->max_length > 128) return;
    for (std::size_t i = 0;
         i < static_cast<std::size_t>(shadow_components->max_length); ++i) {
        void* component = shadow_components->vector[i];
        if (component == nullptr) continue;
        AddLocalBodyRenderer(*reinterpret_cast<void**>(
            static_cast<std::uint8_t*>(component) + 0x20));
    }
}

void RestoreThirdPerson() {
    if (!g_third_person_applied) return;
    __try {
        if (g_third_person_camera_transform != nullptr) {
            g_transform_set_local_position(g_third_person_camera_transform,
                g_original_camera_local_position, nullptr);
        }
        for (std::size_t i = 0; i < g_local_body_renderer_count; ++i) {
            const LocalBodyRendererState& state = g_local_body_renderers[i];
            if (state.renderer == nullptr) continue;
            g_renderer_set_enabled(state.renderer, state.enabled, nullptr);
            g_renderer_set_shadow_casting_mode(
                state.renderer, state.shadow_casting_mode, nullptr);
        }
        if (g_third_person_camera != nullptr) {
            g_camera_set_culling_mask(
                g_third_person_camera, g_original_camera_culling_mask, nullptr);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    std::memset(g_local_body_renderers, 0, sizeof(g_local_body_renderers));
    g_local_body_renderer_count = 0;
    g_third_person_player = nullptr;
    g_third_person_camera_transform = nullptr;
    g_third_person_camera = nullptr;
    g_third_person_applied = false;
}

void RestoreSpinVisual() {
    if (!g_spin_visual_applied) return;
    __try {
        if (g_spin_visual_transform != nullptr) {
            g_transform_set_local_rotation(
                g_spin_visual_transform, g_original_spin_visual_rotation, nullptr);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    g_spin_visual_transform = nullptr;
    g_spin_visual_applied = false;
}

void ApplyViewFeatures(void* local_player) {
    if (local_player == nullptr) {
        RestoreThirdPerson();
        RestoreSpinVisual();
        g_spin_started_at = 0;
        return;
    }

    void* camera_holder = *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(local_player) + 0x30);
    void* camera_transform = camera_holder != nullptr
        ? g_game_object_get_transform(camera_holder, nullptr) : nullptr;
    void* local_renderer = *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(local_player) + 0xB8);
    void* camera = *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(local_player) + 0x40);

    if (g_settings.third_person && camera_transform != nullptr) {
        if (!g_third_person_applied || g_third_person_player != local_player ||
            g_third_person_camera_transform != camera_transform) {
            RestoreThirdPerson();
            g_third_person_player = local_player;
            g_third_person_camera_transform = camera_transform;
            g_third_person_camera = camera;
            g_original_camera_local_position =
                g_transform_get_local_position(camera_transform, nullptr);
            if (camera != nullptr) {
                g_original_camera_culling_mask = g_camera_get_culling_mask(camera, nullptr);
            }
            CaptureLocalBodyRenderers(local_player);
            g_third_person_applied = true;
        }

        Vector3 offset = g_original_camera_local_position;
        offset.y += 0.35f;
        offset.z -= std::clamp(g_settings.third_person_distance, 1.0f, 8.0f);
        g_transform_set_local_position(camera_transform, offset, nullptr);

        int culling_mask = g_original_camera_culling_mask;
        for (std::size_t i = 0; i < g_local_body_renderer_count; ++i) {
            void* renderer = g_local_body_renderers[i].renderer;
            if (renderer == nullptr) continue;
            g_renderer_set_enabled(renderer, true, nullptr);
            g_renderer_set_shadow_casting_mode(renderer, 1, nullptr);
            void* body_object = g_component_get_game_object(renderer, nullptr);
            const int layer = body_object != nullptr
                ? g_game_object_get_layer(body_object, nullptr) : -1;
            if (layer >= 0 && layer < 32) {
                culling_mask |= static_cast<int>(std::uint32_t{1} << layer);
            }
        }
        if (camera != nullptr) {
            g_camera_set_culling_mask(camera, culling_mask, nullptr);
        }
    } else {
        RestoreThirdPerson();
    }

    if (g_settings.spinbot && local_renderer != nullptr) {
        void* visual_transform = g_component_get_transform(local_renderer, nullptr);
        if (visual_transform != nullptr) {
            if (!g_spin_visual_applied || g_spin_visual_transform != visual_transform) {
                RestoreSpinVisual();
                g_spin_visual_transform = visual_transform;
                g_original_spin_visual_rotation =
                    g_transform_get_local_rotation(visual_transform, nullptr);
                g_spin_visual_applied = true;
            }
            g_transform_set_local_rotation(visual_transform,
                MultiplyQuaternion(g_original_spin_visual_rotation, CurrentSpinQuaternion()), nullptr);
        }
    } else {
        RestoreSpinVisual();
        g_spin_started_at = 0;
    }
}

void RestoreFlyPhysics() {
    __try {
        if (g_fly_rigidbody != nullptr) {
            g_rigidbody_set_use_gravity(g_fly_rigidbody, true, nullptr);
            g_rigidbody_set_detect_collisions(g_fly_rigidbody, true, nullptr);
        }
        if (g_fly_player != nullptr) {
            *reinterpret_cast<int*>(
                static_cast<std::uint8_t*>(g_fly_player) + 0x18) = g_original_collision_mask;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    g_gravity_was_disabled = false;
    g_collisions_were_disabled = false;
    g_fly_player = nullptr;
    g_fly_rigidbody = nullptr;
    g_original_collision_mask = 0;
}

void ApplyMovementFeatures(void* local_player) {
    ApplyViewFeatures(local_player);
    if (local_player == nullptr) {
        if (g_gravity_was_disabled || g_collisions_were_disabled) RestoreFlyPhysics();
        return;
    }
    void* rigidbody = *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(local_player) + 0x180);
    if (rigidbody == nullptr) {
        if (g_gravity_was_disabled || g_collisions_were_disabled) RestoreFlyPhysics();
        return;
    }

    if (g_settings.fly) {
        if (g_fly_player != local_player || g_fly_rigidbody != rigidbody) {
            RestoreFlyPhysics();
            g_fly_player = local_player;
            g_fly_rigidbody = rigidbody;
            g_original_collision_mask = *reinterpret_cast<int*>(
                static_cast<std::uint8_t*>(local_player) + 0x18);
        }
        g_rigidbody_set_use_gravity(rigidbody, false, nullptr);
        g_rigidbody_set_detect_collisions(rigidbody, false, nullptr);
        *reinterpret_cast<int*>(static_cast<std::uint8_t*>(local_player) + 0x18) = 0;
        g_gravity_was_disabled = true;
        g_collisions_were_disabled = true;
    } else if (g_gravity_was_disabled || g_collisions_were_disabled) {
        RestoreFlyPhysics();
    }

    Vector3 velocity = g_rigidbody_get_velocity(rigidbody, nullptr);
    if (g_settings.fly) {
        const float fly_speed = std::clamp(g_settings.fly_speed, 2.0f, 200.0f);
        const Vector3 direction = ReadMovementDirection(local_player, true);
        velocity = {direction.x * fly_speed, direction.y * fly_speed, direction.z * fly_speed};
        g_rigidbody_set_velocity(rigidbody, velocity, nullptr);
    } else if (g_settings.speed_hack) {
        const Vector3 direction = ReadMovementDirection(local_player, false);
        if (VectorLengthSquared(direction) > 0.0001f) {
            const float speed = std::clamp(g_settings.move_speed, 2.0f, 100.0f);
            velocity.x = direction.x * speed;
            velocity.z = direction.z * speed;
            g_rigidbody_set_velocity(rigidbody, velocity, nullptr);
        }
    }
}

void HookedPlayerFixedUpdate(void* player, const void* method) {
    g_hook_calls.fetch_add(1, std::memory_order_acquire);
    PlayerFixedUpdate original = g_original_fixed_update;
    if (original != nullptr) original(player, method);
    if (!g_hook_stopping.load(std::memory_order_relaxed) && player != nullptr &&
        player == g_local_player.load(std::memory_order_relaxed)) {
        __try {
            ApplyMovementFeatures(player);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    g_hook_calls.fetch_sub(1, std::memory_order_release);
}

bool InstallMovementHook() {
    if (g_original_fixed_update != nullptr) return true;
    if (g_game_assembly == nullptr) return false;
    auto* target = reinterpret_cast<std::uint8_t*>(
        reinterpret_cast<std::uintptr_t>(g_game_assembly) + kPlayerFixedUpdateRva);
    constexpr std::uint8_t expected[kPlayerFixedUpdatePatchSize] = {
        0x48, 0x89, 0x74, 0x24, 0x20, 0x55, 0x48, 0x8D, 0x6C,
        0x24, 0xA9, 0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00
    };
    if (std::memcmp(target, expected, sizeof(expected)) != 0) return false;

    void* trampoline = VirtualAlloc(nullptr, kPlayerFixedUpdatePatchSize + 14,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (trampoline == nullptr) return false;
    std::memcpy(g_fixed_update_original, target, kPlayerFixedUpdatePatchSize);
    std::memcpy(trampoline, target, kPlayerFixedUpdatePatchSize);
    WriteAbsoluteJump(static_cast<std::uint8_t*>(trampoline) + kPlayerFixedUpdatePatchSize,
        target + kPlayerFixedUpdatePatchSize);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, kPlayerFixedUpdatePatchSize,
            PAGE_EXECUTE_READWRITE, &old_protect)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }
    g_fixed_update_target = target;
    g_fixed_update_trampoline = trampoline;
    g_original_fixed_update = reinterpret_cast<PlayerFixedUpdate>(trampoline);
    WriteAbsoluteJump(target, reinterpret_cast<void*>(HookedPlayerFixedUpdate));
    std::memset(target + 14, 0x90, kPlayerFixedUpdatePatchSize - 14);
    DWORD ignored = 0;
    VirtualProtect(target, kPlayerFixedUpdatePatchSize, old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), target, kPlayerFixedUpdatePatchSize);
    g_hook_stopping.store(false, std::memory_order_release);
    return true;
}

void RemoveMovementHook() {
    g_hook_stopping.store(true, std::memory_order_release);
    if (g_fixed_update_target != nullptr) {
        DWORD old_protect = 0;
        if (VirtualProtect(g_fixed_update_target, kPlayerFixedUpdatePatchSize,
                PAGE_EXECUTE_READWRITE, &old_protect)) {
            std::memcpy(g_fixed_update_target, g_fixed_update_original,
                kPlayerFixedUpdatePatchSize);
            DWORD ignored = 0;
            VirtualProtect(g_fixed_update_target, kPlayerFixedUpdatePatchSize,
                old_protect, &ignored);
            FlushInstructionCache(GetCurrentProcess(), g_fixed_update_target,
                kPlayerFixedUpdatePatchSize);
        }
    }
    while (g_hook_calls.load(std::memory_order_acquire) != 0) Sleep(1);
    g_original_fixed_update = nullptr;
    g_fixed_update_target = nullptr;
    if (g_fixed_update_trampoline != nullptr) {
        VirtualFree(g_fixed_update_trampoline, 0, MEM_RELEASE);
        g_fixed_update_trampoline = nullptr;
    }
}
} // namespace

Settings* GetSettings() {
    return &g_settings;
}

bool Initialize(void* game_assembly) {
    if (game_assembly == nullptr) return false;
    g_game_assembly = static_cast<HMODULE>(game_assembly);
    const auto base = reinterpret_cast<std::uintptr_t>(g_game_assembly);
    g_component_get_transform = reinterpret_cast<ComponentGetTransform>(
        base + kComponentGetTransformRva);
    g_component_get_game_object = reinterpret_cast<ComponentGetGameObject>(
        base + kComponentGetGameObjectRva);
    g_game_object_get_transform = reinterpret_cast<GameObjectGetTransform>(
        base + kGameObjectGetTransformRva);
    g_game_object_get_layer = reinterpret_cast<GameObjectGetLayer>(
        base + kGameObjectGetLayerRva);
    g_transform_get_local_position = reinterpret_cast<TransformGetPosition>(
        base + kTransformGetLocalPositionRva);
    g_transform_set_local_position = reinterpret_cast<TransformSetPosition>(
        base + kTransformSetLocalPositionRva);
    g_transform_get_local_rotation = reinterpret_cast<TransformGetRotation>(
        base + kTransformGetLocalRotationRva);
    g_transform_set_local_rotation = reinterpret_cast<TransformSetRotation>(
        base + kTransformSetLocalRotationRva);
    g_transform_get_right = reinterpret_cast<TransformGetDirection>(
        base + kTransformGetRightRva);
    g_transform_get_forward = reinterpret_cast<TransformGetDirection>(
        base + kTransformGetForwardRva);
    g_camera_get_culling_mask = reinterpret_cast<CameraGetCullingMask>(
        base + kCameraGetCullingMaskRva);
    g_camera_set_culling_mask = reinterpret_cast<CameraSetCullingMask>(
        base + kCameraSetCullingMaskRva);
    g_renderer_get_enabled = reinterpret_cast<RendererGetEnabled>(
        base + kRendererGetEnabledRva);
    g_renderer_set_enabled = reinterpret_cast<RendererSetEnabled>(
        base + kRendererSetEnabledRva);
    g_renderer_get_shadow_casting_mode = reinterpret_cast<RendererGetShadowCastingMode>(
        base + kRendererGetShadowCastingModeRva);
    g_renderer_set_shadow_casting_mode = reinterpret_cast<RendererSetShadowCastingMode>(
        base + kRendererSetShadowCastingModeRva);
    g_rigidbody_get_velocity = reinterpret_cast<RigidbodyGetVelocity>(
        base + kRigidbodyGetVelocityRva);
    g_rigidbody_set_velocity = reinterpret_cast<RigidbodySetVelocity>(
        base + kRigidbodySetVelocityRva);
    g_rigidbody_set_use_gravity = reinterpret_cast<RigidbodySetUseGravity>(
        base + kRigidbodySetUseGravityRva);
    g_rigidbody_set_detect_collisions = reinterpret_cast<RigidbodySetDetectCollisions>(
        base + kRigidbodySetDetectCollisionsRva);
    return InstallMovementHook();
}

void SetLocalPlayer(void* player) {
    g_local_player.store(player, std::memory_order_release);
}

void Apply(void* local_player) {
    if (g_game_assembly == nullptr) return;
    __try {
        ApplyMovementFeatures(local_player);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool IsActive() {
    return g_settings.speed_hack || g_settings.fly || g_settings.third_person ||
        g_settings.spinbot || g_gravity_was_disabled || g_collisions_were_disabled ||
        g_third_person_applied || g_spin_visual_applied;
}

int EffectiveCollisionMask(void* player, int current_mask) {
    if (player == g_fly_player && g_original_collision_mask != 0) {
        return g_original_collision_mask;
    }
    return current_mask;
}

void Shutdown() {
    g_settings.speed_hack = false;
    g_settings.fly = false;
    g_settings.third_person = false;
    g_settings.spinbot = false;
    SetLocalPlayer(nullptr);
    Apply(nullptr);
    RestoreThirdPerson();
    RestoreSpinVisual();
    RestoreFlyPhysics();
    RemoveMovementHook();
    g_game_assembly = nullptr;
}

} // namespace movement
#else
namespace movement {
namespace {
Settings g_settings{};
}
Settings* GetSettings() { return &g_settings; }
bool Initialize(void*) { return false; }
void SetLocalPlayer(void*) {}
void Apply(void*) {}
bool IsActive() { return false; }
int EffectiveCollisionMask(void*, int current_mask) { return current_mask; }
void Shutdown() {}
} // namespace movement
#endif
