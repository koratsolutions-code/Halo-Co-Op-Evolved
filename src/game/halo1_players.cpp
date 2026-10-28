#include "halo1_players.h"
#include "log.h"

#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr uint32_t k_signature = 0x64407440;  // 'd@t@' leído como entero: firma de las data arrays de Halo
constexpr size_t k_signature_offset = 0x28;
constexpr uintptr_t k_player_count_rva = 0x1B7B910;  // cantidad de jugadores locales en halo1.dll

// Lectura segura (no se cae si la página no es legible).
bool safe_read(uintptr_t address, void* out, size_t size) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), out, size, &read) &&
           read == size;
}

template <typename T>
T read_as(uintptr_t address, T fallback = T{}) {
    T value = fallback;
    safe_read(address, &value, sizeof(T));
    return value;
}

struct Tables {
    uintptr_t players_header = 0, players_data = 0, objects_header = 0, objects_data = 0, pool = 0;
    int16_t player_size = 0;
};

std::mutex g_mutex;
Tables g_tables;  // copia viva (players_header == 0: no encontrada)

bool header_valid(uintptr_t header, const char* name) {
    char text[32] = {};
    return read_as<uint32_t>(header + k_signature_offset) == k_signature && safe_read(header, text, sizeof(text)) &&
           strncmp(text, name, 32) == 0;
}

// Recorre la memoria buscando data arrays 'players' y 'object' (misma reserva de memoria = misma copia).
std::vector<Tables> find_copies() {
    struct Found {
        uintptr_t header;
        bool players;
    };
    std::vector<Found> found;
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t address = 0;
    std::vector<uint8_t> buffer;
    while (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi))) {
        uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                        (mbi.Protect & PAGE_READWRITE);  // el estado del juego siempre es escribible
        if (readable && mbi.RegionSize < (1ull << 30)) {
            buffer.resize(mbi.RegionSize);
            if (safe_read(base, buffer.data(), buffer.size())) {
                for (size_t i = k_signature_offset; i + 4 <= buffer.size(); i += 4) {
                    uint32_t value;
                    memcpy(&value, &buffer[i], 4);
                    if (value != k_signature) continue;
                    const char* name = reinterpret_cast<const char*>(&buffer[i - k_signature_offset]);
                    bool players = strncmp(name, "players", 32) == 0;
                    if (players || strncmp(name, "object", 32) == 0)
                        found.push_back({base + i - k_signature_offset, players});
                }
            }
        }
        address = base + mbi.RegionSize;
        if (address < base) break;
    }

    auto allocation_of = [](uintptr_t a) {
        MEMORY_BASIC_INFORMATION m{};
        VirtualQuery(reinterpret_cast<void*>(a), &m, sizeof(m));
        return reinterpret_cast<uintptr_t>(m.AllocationBase);
    };
    std::vector<Tables> copies;
    for (const auto& p : found) {
        if (!p.players) continue;
        for (const auto& o : found) {
            if (o.players || allocation_of(o.header) != allocation_of(p.header)) continue;
            Tables t;
            t.players_header = p.header;
            t.players_data = p.header + read_as<uint32_t>(p.header + 0x34);
            t.player_size = read_as<int16_t>(p.header + 0x22);
            t.objects_header = o.header;
            t.objects_data = o.header + read_as<uint32_t>(o.header + 0x34);
            t.pool = o.header + 0x38 + size_t(read_as<int16_t>(o.header + 0x20)) * 12 + 0x34;
            copies.push_back(t);
        }
    }
    return copies;
}

// La copia viva es la que el juego actualiza: el contador en +0x74 del registro del jugador 1 avanza.
bool pick_live(const std::vector<Tables>& copies, Tables* out) {
    std::vector<uint32_t> before;
    for (const auto& c : copies) before.push_back(read_as<uint32_t>(c.players_data + 0x74));
    Sleep(500);
    for (size_t i = 0; i < copies.size(); ++i) {
        if (read_as<uint32_t>(copies[i].players_data + 0x74) != before[i]) {
            *out = copies[i];
            return true;
        }
    }
    return false;
}

uintptr_t biped_address(const Tables& t, int p) {
    if (!t.players_header) return 0;
    uint32_t handle = read_as<uint32_t>(t.players_data + size_t(p) * size_t(t.player_size) + 0x64, 0xFFFFFFFF);
    if (handle == 0xFFFFFFFF) return 0;
    uint8_t entry[12];
    if (!safe_read(t.objects_data + size_t(handle & 0xFFFF) * 12, entry, 12)) return 0;
    uint16_t salt;
    uint32_t offset;
    memcpy(&salt, entry, 2);
    memcpy(&offset, entry + 8, 4);
    return salt == (handle >> 16) ? t.pool + offset : 0;
}

}  // namespace

void halo1_players_start() {
    std::thread([] {
        int16_t last_count = -1;
        bool rescan = true;
        while (true) {
            Sleep(1000);
            HMODULE halo1 = GetModuleHandleW(L"halo1.dll");
            int16_t count = halo1 ? read_as<int16_t>(reinterpret_cast<uintptr_t>(halo1) + k_player_count_rva, -1) : -1;
            if (count != last_count) {  // cambió la partida (mapa nuevo, otra cantidad de jugadores)
                last_count = count;
                rescan = true;
            }
            {
                std::lock_guard lock(g_mutex);
                if (g_tables.players_header && (!header_valid(g_tables.players_header, "players") ||
                                                !header_valid(g_tables.objects_header, "object"))) {
                    g_tables = {};
                    rescan = true;
                }
            }
            if (!rescan || count < 1) continue;
            Sleep(2000);  // dejar que el mapa termine de cargar
            std::vector<Tables> copies = find_copies();
            Tables live;
            if (!copies.empty() && pick_live(copies, &live)) {
                std::lock_guard lock(g_mutex);
                g_tables = live;
                rescan = false;
                log_write("Nombres: jugadores de Halo 1 encontrados (%zu copias de las tablas)", copies.size());
            }
        }
    }).detach();
}

bool halo1_players_read(Halo1Player out[4]) {
    Tables t;
    {
        std::lock_guard lock(g_mutex);
        t = g_tables;
    }
    if (!t.players_header) return false;
    for (int p = 0; p < 4; ++p) {
        Halo1Player& player = out[p];
        player = {};
        uintptr_t biped = biped_address(t, p);
        if (!biped) continue;
        if (!safe_read(biped + 0x18, player.position, 12) || !safe_read(biped + 0x5C, player.center, 12) ||
            !safe_read(biped + 0x210, player.look, 12))
            continue;
        float length = std::sqrt(player.look[0] * player.look[0] + player.look[1] * player.look[1] +
                                 player.look[2] * player.look[2]);
        player.alive = std::isfinite(player.position[0]) && length > 0.9f && length < 1.1f;
    }
    return true;
}
