#include "armor_colors.h"
#include "halo1_players.h"
#include "log.h"
#include "skins.h"
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
    uint32_t color = 0;
    uint8_t original[k_change_colors_size] = {};
};

void update(Painted& painted, int player, uint32_t color) {
    uintptr_t biped = halo1_player_biped(player);
    if (biped != painted.biped) {  // personaje nuevo (o ninguno): sus colores son los originales
        painted = Painted{};
        if (!biped || !safe_read(biped + k_change_colors, painted.original, sizeof(painted.original))) return;
        painted.biped = biped;
    }
    if (!biped) return;
    // Las skins conservan sus colores: el de armadura es solo para Master Chief (si alcanzó a pintarse antes de que
    // el cuerpo pasara a ser la skin, se devuelve el original).
    if (skins_is_skin_body(biped)) color = 0;
    float wanted[12];
    if (!armor_change_colors(color, double(GetTickCount64()) / 1000.0, wanted)) {
        if (painted.color) safe_write(biped + k_change_colors, painted.original, sizeof(painted.original));
        painted.color = 0;
        return;
    }
    // Los 4 colores con el elegido (así lo confirmó la prueba en el juego). Se reescribe solo si cambió.
    float current[12];
    if (!safe_read(biped + k_change_colors, current, sizeof(current))) return;
    if (memcmp(current, wanted, sizeof(wanted)) != 0 && safe_write(biped + k_change_colors, wanted, sizeof(wanted))) {
        if (painted.color != color) log_write("Colores: jugador %d de %08X", player + 1, color);
    }
    painted.color = color;
}

}  // namespace

void armor_colors_start() {
    std::thread([] {
        Painted painted[4];
        while (true) {
            Sleep(50);  // el arcoíris animado cambia de tono en cada vuelta
            proto::VideoConfig config;
            bool fresh = video_config(config);  // sin Co Op Evolved Host: se devuelven los colores originales
            for (int p = 0; p < 4; ++p) update(painted[p], p, fresh ? config.armor_colors[p] : 0);
        }
    }).detach();
}
