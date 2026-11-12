#pragma once

// Registro en un archivo junto al .exe (coopevolved-host.log o coopevolved-cliente.log). Seguro entre hilos.
// Lo comparten el host y el cliente (src/client/log.h incluye este).
void log_init(const wchar_t* file_name);
void log_write(const char* format, ...);
