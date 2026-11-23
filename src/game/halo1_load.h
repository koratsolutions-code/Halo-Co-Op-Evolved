#pragma once
// Aviso de "halo1.dll recién cargado". MCC carga halo1.dll al elegir Halo CE y lo vuelve a cargar al empezar cada
// misión y al volver al menú. Cada carga trae el código original, así que los desvíos se ponen otra vez. Se ponen solo
// aquí, justo cuando vuelve LoadLibraryExW (antes de que MCC lo use): si un hilo los pone mientras halo1.dll se
// descarga, MinHook falla (MH_ERROR_NOT_EXECUTABLE) o el juego se congela.
#include <cstdint>

using Halo1LoadedFn = void (*)(uintptr_t halo1);

void halo1_load_start();                       // desvía LoadLibraryExW; después de MH_Initialize
void halo1_load_subscribe(Halo1LoadedFn fn);   // fn(base) en cada carga (y ya mismo si halo1.dll está cargado)
