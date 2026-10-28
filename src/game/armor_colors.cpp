#include "armor_colors.h"
#include "halo1_players.h"
#include "log.h"
#include "video.h"

#include "../common/armor_colors.h"

#include <windows.h>

#include <cstring>
#include <thread>

namespace {

constexpr uintptr_t k_change_colors = 0x174;
constexpr size_t k_change_colors_size = 4 * 3 * sizeof(float);

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

// Lo que se le pintó a cada jugador, para devolver el color original si vuelve a "por defecto".
struct Painted {
    uintptr_t biped = 0;
    int color = 0;
    uint8_t original[k_change_colors_size] = {};
};

void update(Painted& painted, int player, int color) {
    uintptr_t biped = halo1_player_biped(player);
    if (biped != painted.biped) {  // personaje nuevo (o ninguno): sus colores son los originales
        painted = Painted{};
        if (!biped || !safe_read(biped + k_change_colors, painted.original, sizeof(painted.original))) return;
        painted.biped = biped;
    }
    if (!biped) return;
    if (color <= 0 || color >= k_armor_color_count) {
        if (painted.color) safe_write(biped + k_change_colors, painted.original, sizeof(painted.original));
        painted.color = 0;
        return;
    }
    // Los 4 colores con el elegido (así lo confirmó la prueba en el juego). Se reescribe solo si cambió.
    const ArmorColor& c = k_armor_colors[color];
    float wanted[12];
    for (int i = 0; i < 4; ++i) {
        wanted[i * 3] = c.r;
        wanted[i * 3 + 1] = c.g;
        wanted[i * 3 + 2] = c.b;
    }
    float current[12];
    if (!safe_read(biped + k_change_colors, current, sizeof(current))) return;
    if (memcmp(current, wanted, sizeof(wanted)) != 0 && safe_write(biped + k_change_colors, wanted, sizeof(wanted))) {
        if (painted.color != color) log_write("Colores: jugador %d de %s", player + 1, c.name_es);
    }
    painted.color = color;
}

}  // namespace

void armor_colors_start() {
    std::thread([] {
        Painted painted[4];
        while (true) {
            Sleep(100);
            proto::VideoConfig config;
            bool fresh = video_config(config);  // sin HaloCoop Host: se devuelven los colores originales
            for (int p = 0; p < 4; ++p) update(painted[p], p, fresh ? config.armor_colors[p] : 0);
        }
    }).detach();
}
