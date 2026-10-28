#include "spawn_probe.h"
#include "halo1_players.h"
#include "log.h"

#include <windows.h>
#include <tlhelp32.h>

#include <atomic>
#include <cstdio>
#include <thread>

namespace {

constexpr int k_max_hits = 20;
constexpr int k_frames = 10;

struct Hit {
    uintptr_t frames[k_frames];  // [0] = instrucción que escribió (la siguiente a ella); luego quien la llamó...
    int frame_count;
    uint32_t value;
    DWORD thread;
};

Hit g_hits[k_max_hits];
std::atomic<int> g_hit_count{0};
uintptr_t g_watched = 0;  // dirección vigilada (0: nada)
PVOID g_handler = nullptr;

// Se ejecuta en el hilo del juego que escribió: nada de memoria nueva ni de bloqueos, solo copiar datos.
LONG CALLBACK on_exception(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* context = info->ContextRecord;
    if (!(context->Dr6 & 1)) return EXCEPTION_CONTINUE_SEARCH;  // no es nuestro punto de vigilancia
    context->Dr6 = 0;
    int index = g_hit_count.load();
    if (index >= k_max_hits) return EXCEPTION_CONTINUE_EXECUTION;
    Hit& hit = g_hits[index];
    hit.thread = GetCurrentThreadId();
    hit.value = g_watched ? *reinterpret_cast<volatile uint32_t*>(g_watched) : 0;
    // Cadena de llamadas con la información de desenrollado de x64 (la misma que usan los depuradores).
    CONTEXT walk = *context;
    hit.frame_count = 0;
    for (int i = 0; i < k_frames && walk.Rip; ++i) {
        hit.frames[hit.frame_count++] = walk.Rip;
        DWORD64 image = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(walk.Rip, &image, nullptr);
        if (function) {
            PVOID handler_data = nullptr;
            DWORD64 establisher = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, walk.Rip, function, &walk, &handler_data, &establisher, nullptr);
        } else {
            walk.Rip = *reinterpret_cast<DWORD64*>(walk.Rsp);  // función hoja: la dirección de vuelta está arriba
            walk.Rsp += 8;
        }
    }
    g_hit_count = index + 1;
    return EXCEPTION_CONTINUE_EXECUTION;
}

// Pone (address != 0) o quita el punto de vigilancia en todos los hilos del juego menos este.
int set_watch(uintptr_t address) {
    int threads = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    THREADENTRY32 entry{sizeof(entry)};
    for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry)) {
        if (entry.th32OwnerProcessID != GetCurrentProcessId() || entry.th32ThreadID == GetCurrentThreadId()) continue;
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE,
                                   entry.th32ThreadID);
        if (!thread) continue;
        if (SuspendThread(thread) != DWORD(-1)) {
            CONTEXT context{};
            context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(thread, &context)) {
                // DR0: la dirección. DR7: L0 activo, condición "escritura" (01), largo 4 bytes (11).
                context.Dr7 &= ~((uint64_t(0xF) << 16) | 3);
                if (address) {
                    context.Dr0 = address;
                    context.Dr7 |= 1 | (uint64_t(1) << 16) | (uint64_t(3) << 18);
                } else {
                    context.Dr0 = 0;
                }
                if (SetThreadContext(thread, &context)) ++threads;
            }
            ResumeThread(thread);
        }
        CloseHandle(thread);
    }
    CloseHandle(snapshot);
    return threads;
}

void describe(uintptr_t address, char* out, size_t size) {
    HMODULE module = nullptr;
    wchar_t path[MAX_PATH] = L"";
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(address), &module) &&
        GetModuleFileNameW(module, path, MAX_PATH)) {
        const wchar_t* name = wcsrchr(path, L'\\');
        snprintf(out, size, "%ls+0x%llX", name ? name + 1 : path,
                 (unsigned long long)(address - reinterpret_cast<uintptr_t>(module)));
    } else {
        snprintf(out, size, "0x%llX", (unsigned long long)address);
    }
}

bool f11_pressed() {
    static bool was_down = false;
    bool down = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
    bool pressed = down && !was_down;
    was_down = down;
    if (!pressed) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

void disarm(const char* why) {
    int threads = set_watch(0);
    g_watched = 0;
    log_write("Aparición: vigilante quitado (%s, %d hilos)", why, threads);
}

}  // namespace

void spawn_probe_start() {
    std::thread([] {
        int logged = 0;
        while (true) {
            Sleep(50);
            // Anotar los avisos nuevos (el manejador solo los guarda).
            int count = g_hit_count.load();
            for (; logged < count; ++logged) {
                const Hit& hit = g_hits[logged];
                char line[1024];
                int length = snprintf(line, sizeof(line), "Aparición: escritura %d · valor 0x%08X · hilo %lu ·", logged + 1,
                                      hit.value, hit.thread);
                for (int i = 0; i < hit.frame_count && length < int(sizeof(line)) - 64; ++i) {
                    char where[96];
                    describe(hit.frames[i], where, sizeof(where));
                    length += snprintf(line + length, sizeof(line) - length, " %s", where);
                }
                log_write("%s", line);
            }
            if (g_watched && count >= k_max_hits) disarm("20 avisos");
            if (!f11_pressed()) continue;
            if (g_watched) {
                disarm("F11");
                continue;
            }
            uintptr_t record = halo1_player_record(1);  // jugador 2
            if (!record) {
                log_write("Aparición: todavía no se encontraron los jugadores (entra a una misión y espera)");
                continue;
            }
            if (!g_handler) g_handler = AddVectoredExceptionHandler(1, on_exception);
            g_hit_count = 0;
            logged = 0;
            g_watched = record + 0x64;
            uint32_t now = *reinterpret_cast<volatile uint32_t*>(g_watched);
            int threads = set_watch(g_watched);
            log_write("Aparición: vigilando el personaje del J2 (ahora 0x%08X) en %d hilos. Maten al J2 y esperen a "
                      "que reaparezca", now, threads);
        }
    }).detach();
}
