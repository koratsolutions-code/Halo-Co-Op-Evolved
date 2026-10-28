#pragma once

// Registro en coopevolved-cliente.log, junto al .exe (para revisar problemas de conexión o video).
void log_init();
void log_write(const char* format, ...);
