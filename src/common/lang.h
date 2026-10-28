#pragma once
// Idioma de la interfaz (inglés por defecto). Cada texto visible se escribe en los dos idiomas: T("...", "...").

enum class Lang { en = 0, es = 1 };

inline Lang g_lang = Lang::en;

inline const char* T(const char* en, const char* es) { return g_lang == Lang::es ? es : en; }
inline const wchar_t* T(const wchar_t* en, const wchar_t* es) { return g_lang == Lang::es ? es : en; }
