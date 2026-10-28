#include "character_probe.h"
#include "halo1_players.h"
#include "log.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint32_t k_bipd = 0x62697064;          // 'bipd' leído como entero
constexpr uint32_t k_bipd_swapped = 0x64706962;  // por si la clase está guardada al revés
constexpr uint32_t k_matg = 0x6D617467;          // 'matg': el tag globals
constexpr char k_player_path[] = "characters\\cyborg\\cyborg";

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

bool f9_pressed() {
    static bool was_down = false;
    bool down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    bool pressed = down && !was_down;
    was_down = down;
    if (!pressed) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

void describe(uintptr_t address, char* out, size_t size) {
    MEMORY_BASIC_INFORMATION mbi{};
    VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi));
    uintptr_t base = reinterpret_cast<uintptr_t>(mbi.AllocationBase);
    wchar_t module[MAX_PATH] = L"";
    if (mbi.Type == MEM_IMAGE && GetModuleFileNameW(reinterpret_cast<HMODULE>(base), module, MAX_PATH)) {
        const wchar_t* name = wcsrchr(module, L'\\');
        snprintf(out, size, "%ls+0x%llX", name ? name + 1 : module, (unsigned long long)(address - base));
    } else {
        snprintf(out, size, "%s+0x%llX (reserva 0x%llX)", mbi.Type == MEM_MAPPED ? "mapeada" : "reserva",
                 (unsigned long long)(address - base), (unsigned long long)base);
    }
}

std::string hex(uintptr_t address, size_t size) {
    std::vector<uint8_t> bytes(size);
    if (!safe_read(address, bytes.data(), size)) return "(no legible)";
    std::string out;
    char b[4];
    for (size_t i = 0; i < size; ++i) {
        snprintf(b, sizeof(b), "%02X", bytes[i]);
        out += b;
        if (i % 4 == 3) out += ' ';
    }
    return out;
}

std::string fourcc(uint32_t value) {
    char text[5] = {char(value >> 24), char(value >> 16), char(value >> 8), char(value), 0};
    for (int i = 0; i < 4; ++i)
        if (text[i] < 0x20 || text[i] > 0x7E) text[i] = '.';
    return text;
}

// Recorre la memoria legible. Para cada región llama a visit(base, datos, tamaño).
template <typename F>
void scan_memory(F visit) {
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t address = 0;
    std::vector<uint8_t> buffer;
    while (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi))) {
        uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                        (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                                        PAGE_EXECUTE_READWRITE));
        if (readable && mbi.RegionSize < (1ull << 30)) {
            buffer.resize(mbi.RegionSize);
            if (safe_read(base, buffer.data(), buffer.size())) visit(base, buffer.data(), buffer.size());
        }
        address = base + mbi.RegionSize;
        if (address < base) break;
    }
}

// Lista de tags del mapa (como en Halo de PC): una entrada por tag, con el ID del tag en +12 y la clase en +0.
// El ID de cada entrada tiene como índice (16 bits bajos) su propia posición en la lista.
struct TagArray {
    uintptr_t base = 0;
    size_t stride = 0;
    size_t count = 0;
};

bool entry_valid(uintptr_t base, size_t stride, size_t i) {
    uint32_t id = read_as<uint32_t>(base + i * stride + 12, 0xFFFFFFFF);
    return id != 0xFFFFFFFF && (id & 0xFFFF) == i;
}

TagArray try_array(uintptr_t id_address, uint32_t id) {
    static const size_t k_strides[] = {0x20, 0x18, 0x28, 0x30, 0x38, 0x40};
    size_t index = id & 0xFFFF;
    for (size_t stride : k_strides) {
        uintptr_t entry = id_address - 12;
        if (entry < index * stride) continue;
        uintptr_t base = entry - index * stride;
        bool ok = true;
        for (size_t i = 0; i < std::min<size_t>(index + 8, 64) && ok; ++i) ok = entry_valid(base, stride, i);
        if (!ok) continue;
        TagArray array{base, stride, 0};
        while (array.count < 65535 && entry_valid(base, stride, array.count)) ++array.count;
        return array;
    }
    return {};
}

// Para encontrar la lista de tags al instante: ¿qué variables de halo1.dll apuntan a ella? (a la lista, a la
// reserva que la contiene o a la base de los punteros de 32 bits). Si la misma variable aparece en todas las
// misiones, el mod puede leerla directo en vez de buscar en la memoria.
void find_pointers(uintptr_t tags, intptr_t name_base) {
    HMODULE halo1 = GetModuleHandleW(L"halo1.dll");
    if (!halo1) return;
    MEMORY_BASIC_INFORMATION mbi{};
    VirtualQuery(reinterpret_cast<void*>(tags), &mbi, sizeof(mbi));
    const uintptr_t allocation = reinterpret_cast<uintptr_t>(mbi.AllocationBase);
    struct Target {
        uintptr_t value;
        const char* what;
    } targets[] = {{tags, "lista"}, {tags - 0x28, "lista-0x28"}, {allocation, "reserva"},
                   {uintptr_t(name_base), "base de punteros"}};
    const uintptr_t module = reinterpret_cast<uintptr_t>(halo1);
    int hits = 0;
    uintptr_t address = module;
    while (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) &&
           reinterpret_cast<uintptr_t>(mbi.AllocationBase) == module) {
        uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                        (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY));
        if (readable) {
            std::vector<uint8_t> data(mbi.RegionSize);
            if (safe_read(base, data.data(), data.size()))
                for (size_t i = 0; i + 8 <= data.size(); i += 8) {
                    uint64_t value;
                    memcpy(&value, &data[i], 8);
                    for (const Target& t : targets)
                        if (value == t.value && hits < 40) {
                            ++hits;
                            log_write("Personajes: puntero a %s en halo1.dll+0x%llX", t.what,
                                      (unsigned long long)(base + i - module));
                        }
                }
        }
        address = base + mbi.RegionSize;
    }
    log_write("Personajes: %d punteros en halo1.dll (lista 0x%llX, reserva 0x%llX, base 0x%llX)", hits,
              (unsigned long long)tags, (unsigned long long)allocation, (unsigned long long)name_base);
}

// Lo que encontró el último F9, para la prueba F10.
struct Found {
    TagArray tags;
    size_t name_offset = 0;   // puntero de 32 bits al nombre dentro de cada entrada
    intptr_t name_base = 0;   // dirección del nombre = name_base + puntero
    bool ok = false;
};
Found g_found;

void run() {
    Halo1Player players[4];
    uintptr_t biped = halo1_players_read(players) ? halo1_player_biped(0) : 0;
    if (!biped) {
        log_write("Personajes: el jugador 1 no tiene personaje ahora (entra a una misión y espera unos segundos)");
        return;
    }
    uint32_t player_tag = read_as<uint32_t>(biped);
    log_write("Personajes: ID del tag del J1: 0x%08X (índice %u)", player_tag, player_tag & 0xFFFF);

    // 1) Dónde aparece ese ID, y el texto "characters\cyborg\cyborg".
    ULONGLONG started = GetTickCount64();
    std::vector<uintptr_t> id_hits, player_paths;
    const size_t path_length = strlen(k_player_path);
    scan_memory([&](uintptr_t base, const uint8_t* data, size_t size) {
        for (size_t i = 12; i + 4 <= size && id_hits.size() < 4000; i += 4) {
            uint32_t word;
            memcpy(&word, data + i, 4);
            if (word == player_tag) id_hits.push_back(base + i);
        }
        for (size_t i = 0; i + path_length <= size && player_paths.size() < 16; ++i)
            if (data[i] == 'c' && memcmp(data + i, k_player_path, path_length) == 0 &&
                (i + path_length == size || data[i + path_length] == 0))
                player_paths.push_back(base + i);
    });
    log_write("Personajes: búsqueda en %.1f s · el ID aparece %zu veces · el nombre %zu veces",
              (GetTickCount64() - started) / 1000.0, id_hits.size(), player_paths.size());

    // 2) La lista de tags: una aparición del ID con 'bipd' 12 bytes antes y entradas vecinas coherentes.
    TagArray tags;
    std::vector<uintptr_t> references;  // otras apariciones con 'bipd' antes: referencias al personaje
    for (uintptr_t hit : id_hits) {
        uint32_t cls = read_as<uint32_t>(hit - 12);
        if (cls != k_bipd && cls != k_bipd_swapped) continue;
        if (!tags.base) {
            TagArray found = try_array(hit, player_tag);
            if (found.base) {
                tags = found;
                continue;
            }
        }
        if (!(tags.base && hit >= tags.base && hit < tags.base + tags.count * tags.stride)) references.push_back(hit);
    }
    if (!tags.base) {
        log_write("Personajes: no se encontró la lista de tags. Apariciones del ID con lo que hay antes:");
        int shown = 0;
        for (uintptr_t hit : id_hits) {
            if (++shown > 30) break;
            char where[160];
            describe(hit, where, sizeof(where));
            log_write("Personajes:   %s · %s", where, hex(hit - 16, 32).c_str());
        }
        log_write("Personajes: fin");
        return;
    }
    char where[160];
    describe(tags.base, where, sizeof(where));
    log_write("Personajes: lista de tags en %s · %zu tags · entradas de 0x%zX bytes", where, tags.count, tags.stride);
    uintptr_t player_entry = tags.base + (player_tag & 0xFFFF) * tags.stride;
    log_write("Personajes: entrada del jugador: %s", hex(player_entry, tags.stride).c_str());

    // 3) Nombres: en la entrada hay un puntero (32 o 64 bits) al nombre. Se busca el campo que, con alguna base,
    //    apunta a uno de los textos "characters\cyborg\cyborg" encontrados.
    size_t name_offset = 0;
    bool name_64 = false;
    intptr_t name_base = 0;
    for (size_t offset = 16; offset + 4 <= tags.stride && !name_offset; offset += 4) {
        uint64_t value64 = offset + 8 <= tags.stride ? read_as<uint64_t>(player_entry + offset) : 0;
        uint32_t value32 = read_as<uint32_t>(player_entry + offset);
        for (uintptr_t p : player_paths) {
            if (value64 == p) {
                name_offset = offset, name_64 = true, name_base = 0;
                break;
            }
            // 32 bits relativo: comprobar con otra entrada que la misma base da un texto con '\'.
            intptr_t base = intptr_t(p) - intptr_t(value32);
            uint32_t other = read_as<uint32_t>(tags.base + offset);
            char text[8] = {};
            if (value32 && safe_read(uintptr_t(base + intptr_t(other)), text, 7) &&
                std::all_of(text, text + 7, [](char c) { return c >= 0x20 && c < 0x7F; })) {
                name_offset = offset, name_64 = false, name_base = base;
                break;
            }
        }
    }
    auto name_of = [&](size_t i) -> std::string {
        if (!name_offset) return "?";
        uintptr_t entry = tags.base + i * tags.stride;
        uintptr_t at = name_64 ? uintptr_t(read_as<uint64_t>(entry + name_offset))
                               : uintptr_t(name_base + intptr_t(read_as<uint32_t>(entry + name_offset)));
        char text[128] = {};
        if (!safe_read(at, text, sizeof(text) - 1)) return "?";
        return text;
    };
    log_write("Personajes: nombre en +0x%zX (%s)", name_offset, name_offset ? (name_64 ? "64 bits" : "32 bits") : "no encontrado");

    // 4) Personajes (clase bipd) y el tag globals.
    int characters = 0;
    for (size_t i = 0; i < tags.count; ++i) {
        uintptr_t entry = tags.base + i * tags.stride;
        uint32_t cls = read_as<uint32_t>(entry);
        if (cls == k_bipd || cls == k_bipd_swapped) {
            ++characters;
            log_write("Personajes:   0x%08X  %s%s", read_as<uint32_t>(entry + 12), name_of(i).c_str(),
                      i == (player_tag & 0xFFFF) ? "  <- jugador" : "");
        } else if (cls == k_matg) {
            log_write("Personajes: globals: 0x%08X %s · entrada %s", read_as<uint32_t>(entry + 12), name_of(i).c_str(),
                      hex(entry, tags.stride).c_str());
        }
    }
    log_write("Personajes: %d personajes en la misión", characters);

    // 5) Referencias al personaje del jugador fuera de la lista (una es "los jugadores son Master Chief").
    log_write("Personajes: %zu referencias al personaje del jugador", references.size());
    int shown = 0;
    for (uintptr_t r : references) {
        if (++shown > 25) break;
        describe(r - 12, where, sizeof(where));
        log_write("Personajes:   %s · %s", where, hex(r - 16, 32).c_str());
    }
    // Para la prueba F10: la lista de tags y cómo leer sus nombres.
    g_found = {};
    if (name_offset && !name_64) {
        g_found.tags = tags;
        g_found.name_offset = name_offset;
        g_found.name_base = name_base;
        g_found.ok = true;
    }
    find_pointers(tags.base, name_base);
    log_write("Personajes: fin");
}

}  // namespace

void character_probe_start() {
    std::thread([] {
        while (true) {
            Sleep(50);
            if (!f9_pressed()) continue;
            log_write("Personajes: F9, buscando");
            run();
        }
    }).detach();
}
