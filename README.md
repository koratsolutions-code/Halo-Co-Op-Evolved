# halo1-coop

Campaña de **Halo: Combat Evolved** (Halo: The Master Chief Collection, Steam) para **4 jugadores**,
cada uno en su propio PC y con su propia pantalla completa, conectados por **Radmin VPN** al PC del host.

El juego corre una sola vez en el PC del host con la pantalla dividida en 4 (AlphaRing). El mod del host
envía a cada amigo solo su cuarto de pantalla y recibe sus controles de vuelta.

> **Estado actual (fase 1):** el host ya tiene `HaloCoopHost.exe`, que crea los mandos de los
> jugadores 2, 3 y 4 y permite probarlos. Todavía no hay conexión por red ni app del cliente:
> se irán agregando por fases.

## Compilar desde el código

Requiere Visual Studio 2022 o Build Tools con *Desarrollo para el escritorio con C++* (incluye CMake
y Ninja). Ejecuta `build.cmd`: compila y deja `HaloCoopHost.exe` en `host/`.

---

## Estructura del repositorio

| Carpeta    | Para quién        | Contenido |
|------------|-------------------|-----------|
| `host/`    | El que hospeda    | El mod del host y todo lo que va en la carpeta del juego |
| `cliente/` | Los otros 3 jugadores | `HaloCoopClient.exe` y su `LEEME.txt` |

---

## Requisitos

**Host**
- Halo: The Master Chief Collection en **Steam** (versión 1.3528.0.0).
- GPU NVIDIA.
- [Radmin VPN](https://www.radmin-vpn.com/) instalado y conectado a la misma red que los amigos.

**Cada cliente**
- Windows 10/11 con GPU NVIDIA.
- Radmin VPN conectado a la misma red que el host.
- Mando de Xbox o teclado y mouse.

---

## Instalación del host

### 1. AlphaRing (pantalla dividida para 4 jugadores)

AlphaRing no se incluye en este repositorio: se descarga de su página oficial.

1. Descarga `Release1.0.zip` de la release **1.3528.0.0** en
   <https://github.com/wouter51/AlphaRing/releases>.
2. Descomprime y copia `WTSAPI32.dll` en:
   `...\steamapps\common\Halo The Master Chief Collection\mcc\binaries\win64\`
3. (Opcional) Comprueba que es el archivo correcto. En PowerShell:
   ```powershell
   Get-FileHash "...\mcc\binaries\win64\WTSAPI32.dll"
   ```
   Debe empezar por `091AA64DCC040BB6`.

### 2. Driver de mandos virtuales (ViGEmBus)

Instala **ViGEmBus** desde <https://github.com/nefarius/ViGEmBus/releases/latest>
(archivo `ViGEmBus_..._x64_x86_arm64.exe`) y reinicia el PC. Si falta, `HaloCoopHost.exe` lo avisa
y abre esa página.

### 3. HaloCoop Host

Copia la carpeta `host/` a cualquier lugar de tu PC (no hace falta ponerla en la carpeta del juego).
Contiene `HaloCoopHost.exe`, que crea los mandos de los jugadores 2, 3 y 4.

---

## Cómo jugar (host)

> **Importante:** abre siempre **HaloCoop Host primero** y MCC después. Los mandos de los jugadores
> 2, 3 y 4 tienen que existir antes de que arranque el juego; si no, esos jugadores no se pueden mover.
> Si MCC ya está abierto, el launcher te pide cerrarlo.

1. Desconecta cualquier mando físico (cambia el orden de los mandos). Si quieres jugar con mando
   como jugador 1, ver *Problemas frecuentes*.
2. Abre `HaloCoopHost.exe`. Arriba muestra qué Controller le tocó a cada jugador (normalmente
   J2 = Controller 1, J3 = Controller 2, J4 = Controller 3). **No lo cierres mientras jueguen.**
3. Presiona **Abrir MCC** y elige **sin anticheat (EAC desactivado)**.
4. Presiona **F4** para abrir el menú de AlphaRing → **Splitscreen**:
   - **Players = 4**.
   - No cambies las demás opciones. En especial, deja activado *"Use player1's profile"*.
   - AlphaRing **no guarda** la configuración: hay que hacerlo cada vez que se abre el juego.
5. Cierra el menú con **F4**. Mientras está abierto, **ningún jugador se puede mover**.
6. En Halo CE activa los **gráficos clásicos** (tecla **Tab** en partida): con los gráficos nuevos
   solo se ven 2 jugadores.
7. Mantén la ventana del juego **activa** (en primer plano) mientras juegan: si cambias a otra
   ventana, MCC deja de leer los mandos.

### Probar los mandos
En HaloCoop Host, el botón **Mover** de cada jugador hace que su personaje camine hacia adelante,
atrás, a los lados, gire y salte (unos 10 segundos). Después de presionarlo, el juego vuelve a quedar
en primer plano solo.

### Problemas frecuentes
- **Un jugador no se mueve:** cierra MCC, cierra HaloCoop Host, ábrelos de nuevo en ese orden.
- **Tengo un mando físico conectado:** los virtuales quedan en otros Controller. HaloCoop Host lo
  avisa y muestra cuál le tocó a cada jugador; en AlphaRing asigna esos Controller a cada jugador.
- **HaloCoop Host dice que falta ViGEmBus:** instálalo (paso 2) y reinicia el PC.

### Problemas conocidos de AlphaRing con 4 jugadores
- Con gráficos remasterizados solo se ven los jugadores 1 y 2 → usar gráficos clásicos (Tab).
- En algunos niveles (*Assault on the Control Room*, *Keyes*) los jugadores 3 y 4 pueden aparecer fuera del mapa.
- Al terminar un nivel con 4 jugadores el juego se puede congelar: guarda y reinicia la misión si pasa.

---

## Instalación y uso del cliente

*Pendiente: se agregará `HaloCoopClient.exe` en `cliente/`. Las instrucciones estarán en `cliente/LEEME.txt`.*

---

## Desinstalar

Borra de `mcc\binaries\win64\` los archivos `WTSAPI32.dll` y los del mod del host.
Luego, en Steam: clic derecho en MCC → Propiedades → Archivos instalados → **Verificar integridad**.
