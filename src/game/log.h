#pragma once
#include <windows.h>

// Registro en halocoop-juego.log, junto al DLL (carpeta del juego). Seguro entre hilos.
void log_init(HMODULE module);
void log_write(const char* format, ...);
