#pragma once

// Registro en coopevolved-host.log, junto al .exe. Seguro entre hilos.
void log_init();
void log_write(const char* format, ...);
