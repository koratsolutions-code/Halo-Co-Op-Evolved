#pragma once

// Registro en halocoop-cliente.log, junto al .exe (para revisar problemas de conexión o video).
void log_init();
void log_write(const char* format, ...);
