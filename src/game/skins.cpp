#include "skins.h"
#include "halo1_players.h"
#include "log.h"
#include "video.h"

#include "../common/skins.h"

#include <windows.h>

#include <MinHook.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <thread>

namespace {

constexpr uintptr_t k_tag_list_rva = 0x1C34FB0;  // puntero a la lista de tags de la misión
constexpr uintptr_t k_tag_data_rva = 0x2B22F50;  // puntero al bloque de datos de los tags (lista = bloque + 0x28)
constexpr uint32_t k_tag_base = 0x50000000;      // los punteros de 32 bits de los tags valen bloque + (p - base)
constexpr size_t k_entry_size = 0x20;
constexpr uint32_t k_bipd = 0x62697064, k_mod2 = 0x6D6F6432, k_antr = 0x616E7472;

// Búsqueda de tags del juego: rax = datos del tag cuyo índice son los 16 bits bajos (con signo) de ecx.
constexpr uintptr_t k_tag_lookup_rva = 0xA9B648;
constexpr uint8_t k_tag_lookup_bytes[] = {0x48, 0x8B, 0x05, 0x61, 0x99, 0x19, 0x01,  // mov rax,[lista]
                                          0x48, 0x0F, 0xBF, 0xD1};                   // movsx rdx,cx
constexpr size_t k_clone_size = 0x1000;  // de sobra para la definición de un biped

uintptr_t g_halo1 = 0;
std::atomic<uint16_t> g_available{0};

// Lo que lee el desvío de la búsqueda: primer índice de las copias (enorme = ninguna) y dirección de la copia de
// cada skin (0 = sin copia: la búsqueda original, que da Master Chief).
constexpr int k_clone_slots = 16;  // una por skin (la 0, Master Chief, no se usa)
static_assert(k_skin_count <= k_clone_slots, "el desvío reserva 16 copias");
volatile int64_t g_first_clone_index = 0x7FFFFFFFFFFFFFFF;
volatile uint64_t g_clone_data[k_clone_slots] = {};
// Mientras el juego crea el cuerpo de un jugador con skin: el índice de Master Chief da la copia de su skin, así el
// cuerpo nace con el modelo y las animaciones de la skin. Tiene que nacer con ella: el juego reserva el espacio de los
// huesos de un cuerpo al crearlo, y uno nacido como Chief (19 huesos) no tiene sitio para los de otro modelo (el
// marine tiene 20): T-pose al aparecer y la mano con el arma descolocada al quedarse quieto.
volatile int64_t g_override_index = 0x7FFFFFFFFFFFFFFF;
volatile uint64_t g_override_data = 0;
// Para el desvío de la aparición (se lee desde el hilo del juego).
std::atomic<int64_t> g_chief_index{-1};  // índice de tag de Master Chief en la misión cargada (-1: ninguna)
std::atomic<uint32_t> g_chief_id{0};
std::atomic<int> g_wanted[4] = {};                 // skin con la que nacerá el próximo cuerpo de cada jugador
std::atomic<uint32_t> g_fake_id[k_clone_slots] = {};  // ID de la copia de cada skin (0: sin copia)

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

bool safe_write(uintptr_t address, const void* data, size_t size) {
    SIZE_T written = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), data, size, &written) &&
           written == size;
}

// Desvío en código máquina al principio de la búsqueda de tags. Toca solo rax, rdx y las banderas, igual que la
// función original (así no importa qué registros den por intactos los que la llaman).
bool hook_tag_lookup() {
    uint8_t bytes[sizeof(k_tag_lookup_bytes)];
    if (!safe_read(g_halo1 + k_tag_lookup_rva, bytes, sizeof(bytes)) ||
        memcmp(bytes, k_tag_lookup_bytes, sizeof(bytes)) != 0) {
        log_write("Skins: esta versión de MCC no coincide con la esperada, skins desactivadas");
        return false;
    }
    uint8_t* stub = static_cast<uint8_t*>(VirtualAlloc(nullptr, 128, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!stub) return false;
    uint8_t* p = stub;
    auto put = [&](std::initializer_list<uint8_t> list) { for (uint8_t b : list) *p++ = b; };
    auto put64 = [&](uint64_t value) { memcpy(p, &value, 8); p += 8; };
    put({0x48, 0x0F, 0xBF, 0xD1});                             // movsx rdx, cx
    put({0x48, 0xB8}); put64(uint64_t(&g_override_index));     // mov rax, &índice sustituido
    put({0x48, 0x3B, 0x10});                                   // cmp rdx, [rax]
    put({0x75, 0x0E});                                         // jne copias
    put({0x48, 0xB8}); put64(uint64_t(&g_override_data));      // mov rax, &copia que lo sustituye
    put({0x48, 0x8B, 0x00});                                   // mov rax, [rax]
    put({0xC3});                                               // ret
    put({0x48, 0xB8}); put64(uint64_t(&g_first_clone_index));  // copias: mov rax, &primer índice
    put({0x48, 0x2B, 0x10});                                   // sub rdx, [rax]
    put({0x48, 0x83, 0xFA, 0x10});                             // cmp rdx, 16
    put({0x73, 0x14});                                         // jae original
    put({0x48, 0xB8}); put64(uint64_t(&g_clone_data[0]));      // mov rax, &copias
    put({0x48, 0x8B, 0x04, 0xD0});                             // mov rax, [rax + rdx*8]
    put({0x48, 0x85, 0xC0});                                   // test rax, rax
    put({0x74, 0x01});                                         // jz original
    put({0xC3});                                               // ret
    put({0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});                 // original: jmp [rip]
    uint8_t* trampoline_slot = p;
    put64(0);
    void* target = reinterpret_cast<void*>(g_halo1 + k_tag_lookup_rva);
    void* trampoline = nullptr;
    MH_STATUS status = MH_CreateHook(target, stub, &trampoline);
    if (status == MH_OK) {
        uint64_t address = uint64_t(trampoline);
        memcpy(trampoline_slot, &address, 8);
        FlushInstructionCache(GetCurrentProcess(), stub, 128);
        status = MH_EnableHook(target);
    }
    if (status != MH_OK) log_write("Skins: no se pudo desviar la búsqueda de tags (%s)", MH_StatusToString(status));
    return status == MH_OK;
}

// Lista de tags de la misión cargada.
struct Tags {
    uintptr_t list = 0;
    size_t count = 0;
    uintptr_t base = 0;  // dirección = base + puntero de 32 bits
    uintptr_t at(uint32_t pointer) const { return base + (pointer - k_tag_base); }
    uintptr_t entry(size_t index) const { return list + index * k_entry_size; }
};

bool read_tags(uintptr_t list, Tags& tags) {
    uintptr_t data = read_as<uintptr_t>(g_halo1 + k_tag_data_rva);
    if (!list || !data || list != data + 0x28) return false;
    tags.list = list;
    tags.base = data;
    tags.count = 0;
    while (tags.count < 0x7FF0 && (read_as<uint32_t>(tags.entry(tags.count) + 12, 0xFFFFFFFF) & 0xFFFF) == tags.count)
        ++tags.count;
    return tags.count > 0;
}

uintptr_t find_tag(const Tags& tags, uint32_t cls, const char* name) {
    char text[128];
    size_t length = strlen(name) + 1;
    if (length > sizeof(text)) return 0;
    for (size_t i = 0; i < tags.count; ++i) {
        uintptr_t entry = tags.entry(i);
        if (read_as<uint32_t>(entry) != cls) continue;
        if (safe_read(tags.at(read_as<uint32_t>(entry + 0x10)), text, length) && memcmp(text, name, length) == 0)
            return entry;
    }
    return 0;
}

// Referencia (clase, nombre, 0, ID) a un tag de esa clase dentro de los datos de un personaje.
uintptr_t find_reference(const Tags& tags, uintptr_t data, uint32_t cls) {
    for (size_t offset = 0; offset < 0x200; offset += 4) {
        if (read_as<uint32_t>(data + offset) != cls) continue;
        uint32_t id = read_as<uint32_t>(data + offset + 12);
        if ((id & 0xFFFF) >= tags.count) continue;
        uintptr_t entry = tags.entry(id & 0xFFFF);
        if (read_as<uint32_t>(entry) == cls && read_as<uint32_t>(entry + 12) == id) return data + offset;
    }
    return 0;
}

// Modelo y animaciones de un personaje (las dos referencias completas de 16 bytes).
struct Look {
    uint8_t model[16];
    uint8_t animations[16];
};

bool read_look(const Tags& tags, uintptr_t entry, uintptr_t* model_at, uintptr_t* animations_at, Look& look) {
    uintptr_t data = tags.at(read_as<uint32_t>(entry + 0x14));
    uintptr_t model = find_reference(tags, data, k_mod2), animations = find_reference(tags, data, k_antr);
    if (model_at) *model_at = model;
    if (animations_at) *animations_at = animations;
    return model && animations && safe_read(model, look.model, 16) && safe_read(animations, look.animations, 16);
}

// Todo lo de la misión cargada. Cuando el juego pone otra lista de tags (otra misión, reinicio) se rehace.
// Por skin hay una copia fija de la definición de Master Chief (índice de tag count + 1 + skin) con el modelo de la
// skin y unas animaciones que calzan con ese modelo (ver common/skins.h). Un cuerpo nace con la copia de la skin de
// su jugador y la conserva hasta morir: elegir otra skin cambia el próximo cuerpo, no el actual (no cabe, ver arriba).
// La lista ampliada (esas entradas apuntan a Master Chief, por si algo del juego lee la lista sin pasar por la
// búsqueda) solo se pone cuando alguien usa una skin. Nada se libera: el juego puede estar leyendo en otro hilo.
struct Mission {
    uintptr_t list = 0;  // la del juego
    size_t count = 0;
    bool ok = false;
    uint32_t chief_id = 0;
    uintptr_t chief_entry = 0, chief_data = 0;
    size_t model_offset = 0, animations_offset = 0;  // dentro de la definición
    uint16_t available = 0;
    Look looks[k_skin_count]{};
    uintptr_t our_list = 0;  // 0 = todavía no puesta
    int wanted[4] = {};      // última skin pedida por cada jugador (para el log)
} g_mission;

uint32_t model_checksum(const Tags& tags, const Look& look) {
    uint32_t id;
    memcpy(&id, look.model + 12, 4);
    uintptr_t entry = tags.entry(id & 0xFFFF);
    return read_as<uint32_t>(tags.at(read_as<uint32_t>(entry + 0x14)) + 4);  // suma de nodos del modelo
}

void load_mission(uintptr_t list) {
    g_first_clone_index = 0x7FFFFFFFFFFFFFFF;
    g_chief_index = -1;
    for (auto& data : g_clone_data) data = 0;
    for (auto& id : g_fake_id) id = 0;
    for (auto& skin : g_wanted) skin = 0;
    g_mission = Mission{};
    g_mission.list = list;
    g_available = 0;
    Tags tags;
    if (!read_tags(list, tags)) return;
    Mission& m = g_mission;
    m.count = tags.count;
    uintptr_t chief = find_tag(tags, k_bipd, k_skins[0].tag);
    uintptr_t model = 0, animations = 0;
    if (!chief || !read_look(tags, chief, &model, &animations, m.looks[0])) {
        log_write("Skins: no se encontró Master Chief en esta misión");
        return;
    }
    m.chief_entry = chief;
    m.chief_id = read_as<uint32_t>(chief + 12);
    m.chief_data = tags.at(read_as<uint32_t>(chief + 0x14));
    m.model_offset = model - m.chief_data;
    m.animations_offset = animations - m.chief_data;
    // Animaciones del marine, para las skins con su mismo esqueleto.
    Look marine{};
    uintptr_t marine_entry = find_tag(tags, k_bipd, "characters\\marine\\marine");
    bool has_marine = marine_entry && read_look(tags, marine_entry, nullptr, nullptr, marine);
    uint32_t marine_checksum = has_marine ? model_checksum(tags, marine) : 0;
    m.available = 1;
    char names[512] = "";
    for (int s = 1; s < k_skin_count; ++s) {
        uintptr_t entry = find_tag(tags, k_bipd, k_skins[s].tag);
        if (!entry || !read_look(tags, entry, nullptr, nullptr, m.looks[s])) continue;
        if (k_skins[s].animations == SkinAnimations::marine) {
            if (!has_marine || model_checksum(tags, m.looks[s]) != marine_checksum) {
                log_write("Skins: %s está en la misión pero su esqueleto no es el del marine: no disponible",
                          k_skins[s].name_es);
                continue;
            }
            memcpy(m.looks[s].animations, marine.animations, 16);
        }
        m.available |= uint16_t(1u << s);
        strncat_s(names, " ", _TRUNCATE);
        strncat_s(names, k_skins[s].name_es, _TRUNCATE);
        strncat_s(names, ",", _TRUNCATE);
    }
    m.ok = true;
    g_chief_id = m.chief_id;
    g_chief_index = int64_t(m.chief_id & 0xFFFF);
    g_available = m.available;
    log_write("Skins: misión con %zu tags; skins disponibles: Master Chief,%s", tags.count, names);
}

// Pone la lista ampliada con una copia por cada skin de la misión (la primera vez que alguien usa una skin).
bool install_clones() {
    Mission& m = g_mission;
    if (m.our_list) return true;
    size_t total = m.count + 1 + k_clone_slots;
    if (total >= 0x7FFF) return false;
    uintptr_t clones = reinterpret_cast<uintptr_t>(
        VirtualAlloc(nullptr, k_clone_slots * k_clone_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    uintptr_t list = reinterpret_cast<uintptr_t>(
        VirtualAlloc(nullptr, total * k_entry_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!clones || !list) {
        log_write("Skins: sin memoria para las copias");
        return false;
    }
    if (!safe_read(m.list, reinterpret_cast<void*>(list), m.count * k_entry_size)) return false;
    uint32_t fake_id[k_clone_slots] = {};
    for (int s = 0; s < k_clone_slots; ++s) {
        size_t index = m.count + 1 + s;
        uint32_t salt = ((m.chief_id >> 16) + uint32_t(index - (m.chief_id & 0xFFFF))) & 0xFFFF;
        fake_id[s] = (salt << 16) | uint32_t(index);
        uintptr_t entry = list + index * k_entry_size;
        if (!safe_read(m.chief_entry, reinterpret_cast<void*>(entry), k_entry_size)) return false;
        memcpy(reinterpret_cast<void*>(entry + 12), &fake_id[s], 4);  // sus datos siguen siendo los de Chief
        if (s == 0 || s >= k_skin_count || !(m.available & (1u << s))) continue;
        uintptr_t clone = clones + s * k_clone_size;
        if (!safe_read(m.chief_data, reinterpret_cast<void*>(clone), k_clone_size)) return false;
        memcpy(reinterpret_cast<void*>(clone + m.model_offset), m.looks[s].model, 16);
        memcpy(reinterpret_cast<void*>(clone + m.animations_offset), m.looks[s].animations, 16);
    }
    for (int s = 1; s < k_skin_count; ++s) {
        if (!(m.available & (1u << s))) continue;
        g_clone_data[s] = clones + s * k_clone_size;
        g_fake_id[s] = fake_id[s];
    }
    g_first_clone_index = int64_t(m.count + 1);  // desde ya el desvío entrega las copias que tengan dirección
    if (!safe_write(g_halo1 + k_tag_list_rva, &list, sizeof(list))) {
        g_first_clone_index = 0x7FFFFFFFFFFFFFFF;
        log_write("Skins: no se pudo poner la lista de tags con las copias");
        return false;
    }
    m.our_list = list;
    log_write("Skins: copias listas");
    return true;
}

// Solo anota con qué skin nacerá el próximo cuerpo del jugador; el actual se queda como está.
void update_player(int player, int wanted) {
    Mission& m = g_mission;
    if (wanted <= 0 || wanted >= k_skin_count || !(m.available & (1u << wanted))) wanted = 0;  // → Master Chief
    if (wanted == m.wanted[player]) return;
    if (wanted && !install_clones()) return;
    g_wanted[player] = wanted;
    m.wanted[player] = wanted;
    log_write("Skins: J%d será %s al reaparecer", player + 1, k_skins[wanted].name_es);
}

// ---- Cuerpos que nacen con su skin ----
// halo1.dll+0xAD4184 crea el cuerpo de un jugador al aparecer o reaparecer (se encontró vigilando quién escribe el
// personaje del jugador). Uno de sus argumentos identifica al jugador: 16 bits bajos = índice 0-3 y 16 altos = la
// "sal" que está al principio de su registro.
constexpr uintptr_t k_spawn_rva = 0xAD4184;
constexpr uint8_t k_spawn_prologue[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48,
                                        0x89, 0x78, 0x20, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57};

int player_of(const uint64_t* args, int count) {
    for (int i = 0; i < count; ++i) {
        uint32_t value = uint32_t(args[i]);
        int index = value & 0xFFFF;
        if (index > 3) continue;
        uintptr_t record = halo1_player_record(index);
        if (record && read_as<uint16_t>(record) == uint16_t(value >> 16)) return index;
    }
    return -1;
}

using SpawnFn = uint64_t(__fastcall*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
SpawnFn g_original_spawn = nullptr;

// Mismos 8 argumentos enteros que recibe (4 en registros, 4 en la pila) y se los pasa tal cual al original.
uint64_t __fastcall spawn_hook(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6,
                               uint64_t a7, uint64_t a8) {
    uint64_t args[4] = {a1, a2, a3, a4};
    int player = player_of(args, 4);
    int skin = player >= 0 ? g_wanted[player].load() : 0;
    uint64_t clone = skin > 0 && skin < k_clone_slots ? g_clone_data[skin] : 0;
    uint32_t fake = clone ? g_fake_id[skin].load() : 0;
    int64_t chief_index = g_chief_index;
    bool with_skin = clone && fake && chief_index >= 0;
    if (with_skin) {
        g_override_data = clone;
        g_override_index = chief_index;
    }
    uint64_t result = g_original_spawn(a1, a2, a3, a4, a5, a6, a7, a8);
    g_override_index = 0x7FFFFFFFFFFFFFFF;
    if (with_skin) {
        // El cuerpo nuevo dice "soy Master Chief": que diga la copia de su skin, así la conserva toda su vida
        // (aunque el jugador elija otra skin, que le llega al reaparecer).
        uint32_t chief = g_chief_id;
        if (uintptr_t biped = halo1_player_biped(player); biped && read_as<uint32_t>(biped) == chief)
            safe_write(biped, &fake, 4);
    }
    return result;
}

bool hook_spawn() {
    void* spawn = reinterpret_cast<void*>(g_halo1 + k_spawn_rva);
    uint8_t prologue[sizeof(k_spawn_prologue)];
    if (!safe_read(uintptr_t(spawn), prologue, sizeof(prologue)) ||
        memcmp(prologue, k_spawn_prologue, sizeof(prologue)) != 0) {
        log_write("Skins: la función de aparición no es la esperada (las skins se ponen después de aparecer)");
        return false;
    }
    MH_STATUS status = MH_CreateHook(spawn, reinterpret_cast<void*>(&spawn_hook),
                                     reinterpret_cast<void**>(&g_original_spawn));
    if (status == MH_OK) status = MH_EnableHook(spawn);
    if (status != MH_OK) log_write("Skins: no se pudo enganchar la aparición (%s)", MH_StatusToString(status));
    return status == MH_OK;
}

}  // namespace

uint16_t skins_available() { return g_available; }

void skins_start() {
    std::thread([] {
        // halo1.dll se carga al abrir Halo CE desde el menú de MCC: esperarlo.
        while (!(g_halo1 = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"halo1.dll")))) Sleep(1000);
        if (!hook_tag_lookup()) return;
        hook_spawn();
        log_write("Skins: listas");
        ULONGLONG retry_at = 0;
        while (true) {
            Sleep(50);
            uintptr_t list = read_as<uintptr_t>(g_halo1 + k_tag_list_rva);
            if (!list) {
                if (g_mission.list) load_mission(0);
                continue;
            }
            // El juego cargó (o recargó) una misión: otra lista, o volvió a poner la suya en vez de la nuestra.
            // Mientras carga puede faltar algo: se reintenta cada segundo.
            bool reload = list != g_mission.list && list != g_mission.our_list;
            if (g_mission.our_list && list == g_mission.list) reload = true;
            if (!g_mission.ok && GetTickCount64() >= retry_at) reload = true;
            if (reload) {
                load_mission(list);
                retry_at = GetTickCount64() + 1000;
            }
            if (!g_mission.ok) continue;
            proto::VideoConfig config;
            bool fresh = video_config(config);  // sin Co Op Evolved Host: todos como Master Chief
            for (int p = 0; p < 4; ++p) update_player(p, fresh ? config.skins[p] : 0);
        }
    }).detach();
}
