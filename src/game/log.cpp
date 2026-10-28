#include "log.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

static FILE* g_file = nullptr;
static std::mutex g_mutex;

void log_init(HMODULE module) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(module, path, MAX_PATH);
    std::wstring log_path = path;
    log_path = log_path.substr(0, log_path.find_last_of(L"\\/") + 1) + L"halocoop-juego.log";
    g_file = _wfsopen(log_path.c_str(), L"w", _SH_DENYWR);
}

void log_write(const char* format, ...) {
    if (!g_file) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::lock_guard lock(g_mutex);
    fprintf(g_file, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list args;
    va_start(args, format);
    vfprintf(g_file, format, args);
    va_end(args);
    fputc('\n', g_file);
    fflush(g_file);
}
