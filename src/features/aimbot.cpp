#include "features/aimbot.h"

#if defined(_WIN32)
#include <Windows.h>
#include <intrin.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#endif

namespace aimbot {
namespace {
Settings g_settings{false, false, false, true, true, 100.0f, 300.0f, 100};

#if defined(_WIN32)
constexpr std::uintptr_t kProcessShotDirectionsRva = 0x1106BA0;
constexpr std::uintptr_t kPhysicsRaycastAllRva = 0x1094700;
constexpr std::size_t kShotHookPatchSize = 14;
constexpr std::size_t kRaycastHookPatchSize = 17;

struct Il2CppArray {
    void* klass;
    void* monitor;
    void* bounds;
    std::uintptr_t max_length;
    void* vector[1];
};

struct Target {
    Vector3 origin{};
    Vector3 position{};
    bool valid = false;
};

using ProcessShotDirections = void (*)(void* player, Il2CppArray* directions, const void* method);
using PhysicsRaycastAll = Il2CppArray* (*)(const Vector3* origin, const Vector3* direction,
                                           float max_distance, int layer_mask, const void* method);

HMODULE g_game_assembly = nullptr;
std::mutex g_target_mutex;
Target g_target{};
std::atomic_bool g_enabled{false};
std::atomic_bool g_no_spread{false};
std::atomic_bool g_wall_shot{false};
std::atomic_int g_damage_layer_mask{0};
std::atomic<ULONGLONG> g_last_local_shot_at{0};
std::atomic_bool g_require_rmb{false};
std::atomic_int g_hit_chance{100};
std::atomic_bool g_hook_stopping{false};
std::atomic_ulong g_hook_calls{0};

ProcessShotDirections g_original_process_shots = nullptr;
void* g_process_shots_target = nullptr;
void* g_process_shots_trampoline = nullptr;
std::uint8_t g_process_shots_original[kShotHookPatchSize]{};

PhysicsRaycastAll g_original_raycast_all = nullptr;
void* g_raycast_all_target = nullptr;
void* g_raycast_all_trampoline = nullptr;
std::uint8_t g_raycast_all_original[kRaycastHookPatchSize]{};

void WriteAbsoluteJump(void* destination, const void* target) {
    auto* bytes = static_cast<std::uint8_t*>(destination);
    bytes[0] = 0xFF;
    bytes[1] = 0x25;
    *reinterpret_cast<std::uint32_t*>(bytes + 2) = 0;
    *reinterpret_cast<std::uintptr_t*>(bytes + 6) = reinterpret_cast<std::uintptr_t>(target);
}

std::uint32_t NextRandom() {
    static thread_local std::uint32_t state = 0xA341316Cu ^ GetCurrentThreadId();
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

Target CopyTarget() {
    std::lock_guard<std::mutex> lock(g_target_mutex);
    return g_target;
}

bool ShouldRedirect() {
    if (g_hook_stopping.load(std::memory_order_relaxed) ||
        !g_enabled.load(std::memory_order_relaxed)) {
        return false;
    }
    if (g_require_rmb.load(std::memory_order_relaxed) &&
        !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
        return false;
    }
    const int chance = std::clamp(g_hit_chance.load(std::memory_order_relaxed), 1, 100);
    return static_cast<int>(NextRandom() % 100u) < chance;
}

bool NormalizeTowards(const Vector3& origin, const Vector3& target, Vector3& direction) {
    const float dx = target.x - origin.x;
    const float dy = target.y - origin.y;
    const float dz = target.z - origin.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(length) || length <= 0.001f) return false;
    direction = {dx / length, dy / length, dz / length};
    return true;
}

void HookedProcessShotDirections(void* player, Il2CppArray* directions, const void* method) {
    g_hook_calls.fetch_add(1, std::memory_order_acquire);

    if (directions != nullptr && directions->max_length > 0 && directions->max_length <= 64) {
        auto* values = reinterpret_cast<Vector3*>(directions->vector);
        Vector3 effective_direction{};
        bool replace_directions = false;
        if (ShouldRedirect()) {
            const Target target = CopyTarget();
            replace_directions = target.valid && NormalizeTowards(
                target.origin, target.position, effective_direction);
        }
        if (!replace_directions && g_no_spread.load(std::memory_order_relaxed)) {
            effective_direction = values[0];
            replace_directions = std::isfinite(effective_direction.x) &&
                std::isfinite(effective_direction.y) && std::isfinite(effective_direction.z);
        }
        if (replace_directions) {
            for (std::size_t i = 0; i < static_cast<std::size_t>(directions->max_length); ++i) {
                values[i] = effective_direction;
            }
        }
    }

    ProcessShotDirections original = g_original_process_shots;
    if (original != nullptr) original(player, directions, method);
    g_hook_calls.fetch_sub(1, std::memory_order_release);
}

bool IsLocalShotRaycastCaller(const void* caller) {
    if (g_game_assembly == nullptr || caller == nullptr) return false;
    const std::uintptr_t rva = reinterpret_cast<std::uintptr_t>(caller) -
        reinterpret_cast<std::uintptr_t>(g_game_assembly);
    return rva == 0x11090BC || rva == 0x110D64A;
}

Il2CppArray* HookedRaycastAll(const Vector3* origin, const Vector3* direction,
                              float max_distance, int layer_mask, const void* method) {
    const void* caller = _ReturnAddress();
    g_hook_calls.fetch_add(1, std::memory_order_acquire);

    Vector3 redirected{};
    const Vector3* effective_direction = direction;
    const bool local_shot = IsLocalShotRaycastCaller(caller);
    if (local_shot) g_last_local_shot_at.store(GetTickCount64(), std::memory_order_relaxed);
    if (origin != nullptr && local_shot && ShouldRedirect()) {
        const Target target = CopyTarget();
        if (target.valid && NormalizeTowards(*origin, target.position, redirected)) {
            effective_direction = &redirected;
        }
    }

    int effective_layer_mask = layer_mask;
    if (local_shot && g_wall_shot.load(std::memory_order_relaxed)) {
        const int damage_mask = g_damage_layer_mask.load(std::memory_order_relaxed);
        if (damage_mask != 0) effective_layer_mask = damage_mask;
    }

    PhysicsRaycastAll original = g_original_raycast_all;
    Il2CppArray* result = original != nullptr
        ? original(origin, effective_direction, max_distance, effective_layer_mask, method) : nullptr;
    g_hook_calls.fetch_sub(1, std::memory_order_release);
    return result;
}

bool InstallRaycastHook() {
    if (g_original_raycast_all != nullptr) return true;
    if (g_game_assembly == nullptr) return false;

    auto* target = reinterpret_cast<std::uint8_t*>(
        reinterpret_cast<std::uintptr_t>(g_game_assembly) + kPhysicsRaycastAllRva);
    constexpr std::uint8_t expected[kRaycastHookPatchSize] = {
        0x48, 0x83, 0xEC, 0x58, 0xF2, 0x0F, 0x10, 0x02, 0x8B, 0x42, 0x08,
        0xF2, 0x0F, 0x11, 0x44, 0x24, 0x30
    };
    if (std::memcmp(target, expected, sizeof(expected)) != 0) return false;

    void* trampoline = VirtualAlloc(nullptr, kRaycastHookPatchSize + 14,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (trampoline == nullptr) return false;
    std::memcpy(g_raycast_all_original, target, kRaycastHookPatchSize);
    std::memcpy(trampoline, target, kRaycastHookPatchSize);
    WriteAbsoluteJump(static_cast<std::uint8_t*>(trampoline) + kRaycastHookPatchSize,
        target + kRaycastHookPatchSize);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, kRaycastHookPatchSize, PAGE_EXECUTE_READWRITE, &old_protect)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }
    g_raycast_all_target = target;
    g_raycast_all_trampoline = trampoline;
    g_original_raycast_all = reinterpret_cast<PhysicsRaycastAll>(trampoline);
    WriteAbsoluteJump(target, reinterpret_cast<void*>(HookedRaycastAll));
    DWORD ignored = 0;
    VirtualProtect(target, kRaycastHookPatchSize, old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), target, kRaycastHookPatchSize);
    return true;
}

bool InstallHooks() {
    if (g_original_process_shots != nullptr) return InstallRaycastHook();
    if (g_game_assembly == nullptr) return false;

    auto* target = reinterpret_cast<std::uint8_t*>(
        reinterpret_cast<std::uintptr_t>(g_game_assembly) + kProcessShotDirectionsRva);
    constexpr std::uint8_t expected[kShotHookPatchSize] = {
        0x40, 0x55, 0x41, 0x55, 0x41, 0x56, 0x48, 0x8D,
        0xAC, 0x24, 0x20, 0xFF, 0xFF, 0xFF
    };
    if (std::memcmp(target, expected, sizeof(expected)) != 0) return false;

    void* trampoline = VirtualAlloc(nullptr, kShotHookPatchSize + 14,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (trampoline == nullptr) return false;
    std::memcpy(g_process_shots_original, target, kShotHookPatchSize);
    std::memcpy(trampoline, target, kShotHookPatchSize);
    WriteAbsoluteJump(static_cast<std::uint8_t*>(trampoline) + kShotHookPatchSize,
        target + kShotHookPatchSize);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, kShotHookPatchSize, PAGE_EXECUTE_READWRITE, &old_protect)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }
    g_process_shots_target = target;
    g_process_shots_trampoline = trampoline;
    g_original_process_shots = reinterpret_cast<ProcessShotDirections>(trampoline);
    WriteAbsoluteJump(target, reinterpret_cast<void*>(HookedProcessShotDirections));
    DWORD ignored = 0;
    VirtualProtect(target, kShotHookPatchSize, old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), target, kShotHookPatchSize);

    g_hook_stopping.store(false, std::memory_order_release);
    return InstallRaycastHook();
}

void RestoreHook(void* target, const std::uint8_t* original, std::size_t size) {
    if (target == nullptr) return;
    DWORD old_protect = 0;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old_protect)) return;
    std::memcpy(target, original, size);
    DWORD ignored = 0;
    VirtualProtect(target, size, old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), target, size);
}

void RemoveHooks() {
    g_hook_stopping.store(true, std::memory_order_release);
    RestoreHook(g_process_shots_target, g_process_shots_original, kShotHookPatchSize);
    RestoreHook(g_raycast_all_target, g_raycast_all_original, kRaycastHookPatchSize);
    while (g_hook_calls.load(std::memory_order_acquire) != 0) Sleep(1);

    g_original_process_shots = nullptr;
    g_process_shots_target = nullptr;
    if (g_process_shots_trampoline != nullptr) {
        VirtualFree(g_process_shots_trampoline, 0, MEM_RELEASE);
        g_process_shots_trampoline = nullptr;
    }
    g_original_raycast_all = nullptr;
    g_raycast_all_target = nullptr;
    if (g_raycast_all_trampoline != nullptr) {
        VirtualFree(g_raycast_all_trampoline, 0, MEM_RELEASE);
        g_raycast_all_trampoline = nullptr;
    }
}
#endif
} // namespace

Settings* GetSettings() {
    return &g_settings;
}

bool Initialize(void* game_assembly) {
#if defined(_WIN32)
    if (game_assembly == nullptr) return false;
    g_game_assembly = static_cast<HMODULE>(game_assembly);
    return InstallHooks();
#else
    (void)game_assembly;
    return false;
#endif
}

void UpdateRuntimeSettings() {
#if defined(_WIN32)
    g_enabled.store(g_settings.enabled, std::memory_order_relaxed);
    g_require_rmb.store(g_settings.require_rmb, std::memory_order_relaxed);
    g_hit_chance.store(g_settings.hit_chance, std::memory_order_relaxed);
#endif
}

void SetNoSpread(bool enabled) {
#if defined(_WIN32)
    g_no_spread.store(enabled, std::memory_order_relaxed);
#else
    (void)enabled;
#endif
}

void SetWallShot(bool enabled, int damage_layer_mask) {
#if defined(_WIN32)
    g_damage_layer_mask.store(damage_layer_mask, std::memory_order_relaxed);
    g_wall_shot.store(enabled, std::memory_order_relaxed);
#else
    (void)enabled;
    (void)damage_layer_mask;
#endif
}

bool IsLocalShotWindow() {
#if defined(_WIN32)
    const ULONGLONG shot_at = g_last_local_shot_at.load(std::memory_order_relaxed);
    return shot_at != 0 && GetTickCount64() - shot_at <= 250;
#else
    return false;
#endif
}

void SetTarget(const Vector3& origin, const Vector3& target, bool valid) {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(g_target_mutex);
    g_target = {origin, target, valid};
#else
    (void)origin;
    (void)target;
    (void)valid;
#endif
}

void ClearTarget() {
    SetTarget({}, {}, false);
}

void Shutdown() {
#if defined(_WIN32)
    g_settings.enabled = false;
    g_no_spread.store(false, std::memory_order_relaxed);
    g_wall_shot.store(false, std::memory_order_relaxed);
    g_damage_layer_mask.store(0, std::memory_order_relaxed);
    g_last_local_shot_at.store(0, std::memory_order_relaxed);
    UpdateRuntimeSettings();
    ClearTarget();
    RemoveHooks();
    g_game_assembly = nullptr;
#endif
}

} // namespace aimbot
