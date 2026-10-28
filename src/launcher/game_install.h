#pragma once
#include <string>

// Instalación del DLL del juego (VERSION.dll) en la carpeta de MCC, buscada en las bibliotecas de Steam.

enum class ModState { game_not_found, not_installed, outdated, installed };

std::wstring game_binaries_dir();             // ...\mcc\binaries\win64, o vacío si no se encontró MCC
ModState game_mod_state(const std::wstring& binaries_dir);
bool game_alpharing_present(const std::wstring& binaries_dir);
bool game_install_mod(const std::wstring& binaries_dir, std::wstring& error);  // MCC tiene que estar cerrado
