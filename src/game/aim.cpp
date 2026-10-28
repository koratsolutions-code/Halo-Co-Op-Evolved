#include "aim.h"
#include "halo1_players.h"
#include "log.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr size_t k_stride = 0x58;
constexpr float k_two_pi = 6.2831853f;
constexpr float k_max_pitch = 1.4835f;  // 85°, como el límite de Halo

std::mutex g_mutex;
uintptr_t g_cameras = 0;  // cámara de J1 (las demás cada k_stride); 0: sin encontrar
std::atomic<bool> g_ready{false};

bool safe_read(uintptr_t address, void* out, size_t size) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), out, size, &read) &&
           read == size;
}

bool safe_write(uintptr_t address, const void* data, size_t size) {
    SIZE_T written = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), data, size, &written) &&
           written == size;
}

void look_to_angles(const float look[3], float* yaw, float* pitch) {
    *yaw = std::atan2(look[1], look[0]);
    if (*yaw < 0) *yaw += k_two_pi;
    *pitch = std::asin(std::clamp(look[2], -1.0f, 1.0f));
}

float yaw_distance(float a, float b) {
    float d = std::fmod(std::fabs(a - b), k_two_pi);
    return std::min(d, k_two_pi - d);
}

// ¿Las cámaras de J2-J4 vivos coinciden con su mira? J1 no cuenta: su mouse pasa por AlphaRing.
// Hacen falta al menos dos, para no confundirse con otro par de floats parecido.
bool cameras_match(uintptr_t cameras, const Halo1Player players[4], float tolerance) {
    int checked = 0;
    for (int p = 1; p < 4; ++p) {
        if (!players[p].alive) continue;
        float yaw, pitch, camera[2];
        look_to_angles(players[p].look, &yaw, &pitch);
        if (!safe_read(cameras + p * k_stride, camera, 8) || !std::isfinite(camera[0]) ||
            yaw_distance(camera[0], yaw) > tolerance || std::fabs(camera[1] - pitch) > tolerance)
            return false;
        ++checked;
    }
    return checked >= 2;
}

uintptr_t find_cameras(const Halo1Player players[4]) {
    int first = -1;
    float yaw = 0, pitch = 0;
    for (int p = 1; p < 4 && first < 0; ++p)
        if (players[p].alive) {
            first = p;
            look_to_angles(players[p].look, &yaw, &pitch);
        }
    if (first < 0) return 0;
    constexpr float k_tolerance = 0.002f;
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t address = 0;
    std::vector<uint8_t> buffer;
    while (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi))) {
        uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        bool writable = mbi.State == MEM_COMMIT && mbi.Type != MEM_MAPPED &&
                        !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                        (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE));
        if (writable && mbi.RegionSize < (1ull << 30)) {
            buffer.resize(mbi.RegionSize);
            if (safe_read(base, buffer.data(), buffer.size())) {
                const float* values = reinterpret_cast<const float*>(buffer.data());
                size_t count = buffer.size() / 4;
                for (size_t i = 0; i + 1 < count; ++i) {
                    if (std::fabs(values[i + 1] - pitch) >= k_tolerance) continue;
                    if (std::fabs(values[i] - yaw) >= k_tolerance) continue;
                    uintptr_t at = base + i * 4;
                    if (at < first * k_stride) continue;
                    uintptr_t cameras = at - first * k_stride;
                    if (cameras_match(cameras, players, k_tolerance)) return cameras;
                }
            }
        }
        address = base + mbi.RegionSize;
        if (address < base) break;
    }
    return 0;
}

}  // namespace

void aim_start() {
    std::thread([] {
        int misses = 0;
        ULONGLONG next_search = 0;
        while (true) {
            Sleep(1000);
            Halo1Player players[4];
            if (!halo1_players_read(players)) continue;
            uintptr_t cameras;
            {
                std::lock_guard lock(g_mutex);
                cameras = g_cameras;
            }
            if (cameras) {
                // Al girar rápido la mira del biped va un poco atrás de la cámara: margen amplio y varias fallas
                // seguidas antes de soltarlas (otro nivel, reaparecer, volver al menú).
                misses = cameras_match(cameras, players, 0.3f) ? 0 : misses + 1;
                if (misses < 3) continue;
                std::lock_guard lock(g_mutex);
                g_cameras = 0;
                g_ready = false;
                log_write("Mira directa: las cámaras ya no coinciden, se vuelven a buscar");
                continue;
            }
            if (GetTickCount64() < next_search) continue;
            cameras = find_cameras(players);
            if (!cameras) {
                next_search = GetTickCount64() + 5000;  // la búsqueda recorre la memoria: no en cada segundo
                continue;
            }
            misses = 0;
            std::lock_guard lock(g_mutex);
            g_cameras = cameras;
            g_ready = true;
            log_write("Mira directa: cámaras de los jugadores encontradas");
        }
    }).detach();
}

bool aim_ready() {
    return g_ready;
}

void aim_apply(int player, float yaw, float pitch) {
    if (player < 2 || player > 4 || !std::isfinite(yaw) || !std::isfinite(pitch)) return;
    std::lock_guard lock(g_mutex);
    if (!g_cameras) return;
    uintptr_t at = g_cameras + (player - 1) * k_stride;
    float camera[2];
    if (!safe_read(at, camera, 8) || !std::isfinite(camera[0]) || !std::isfinite(camera[1])) return;
    camera[0] = std::fmod(camera[0] + yaw, k_two_pi);
    if (camera[0] < 0) camera[0] += k_two_pi;
    camera[1] = std::clamp(camera[1] + pitch, -k_max_pitch, k_max_pitch);
    safe_write(at, camera, 8);
}
