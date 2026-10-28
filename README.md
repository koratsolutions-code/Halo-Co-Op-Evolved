# halo1-coop

Campaña de **Halo: Combat Evolved** (Halo: The Master Chief Collection, Steam) para **4 jugadores**,
cada uno en su propio PC y con su propia pantalla completa, conectados por **Radmin VPN** al PC del host.

El juego corre una sola vez en el PC del host con la pantalla dividida en 4 (AlphaRing). El mod del host
envía a cada amigo solo su cuarto de pantalla y recibe sus controles de vuelta.

> **Estado actual (fase 4):** el host (`HaloCoopHost.exe` + `VERSION.dll`) crea los mandos de los
> jugadores 2, 3 y 4, recibe sus controles y les manda su cuadrante del juego como video (NVENC) y el
> sonido del juego (Opus). Cada amigo lo ve y escucha a pantalla completa en `HaloCoopClient.exe`.

## Compilar desde el código

Requiere Visual Studio 2022 o Build Tools con *Desarrollo para el escritorio con C++* (incluye CMake
y Ninja). Ejecuta `build.cmd`: compila y deja `HaloCoopHost.exe` y `VERSION.dll` en `host/` y
`HaloCoopClient.exe` en `cliente/`.

---

## Estructura del repositorio

| Carpeta    | Para quién        | Contenido |
|------------|-------------------|-----------|
| `host/`    | El que hospeda    | `HaloCoopHost.exe` (launcher y panel) y `VERSION.dll` (mod del juego, se instala solo) |
| `cliente/` | Los otros 3 jugadores | `HaloCoopClient.exe` y su `LEEME.txt` |

---

## Requisitos

**Host**
- Halo: The Master Chief Collection en **Steam** (versión 1.3528.0.0).
- GPU NVIDIA (GTX 900 o más nueva) con drivers actualizados: el video se codifica con NVENC.
- Internet con unos **10 Mbps de subida por amigo a 60 FPS** (30 Mbps para 3 amigos).
- [Radmin VPN](https://www.radmin-vpn.com/) instalado y conectado a la misma red que los amigos.

**Cada cliente**
- Windows 10/11 con GPU NVIDIA y drivers actualizados (el video se decodifica con NVDEC).
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
Contiene `HaloCoopHost.exe`, que crea los mandos de los jugadores 2, 3 y 4, y `VERSION.dll`, el mod
que captura el video dentro del juego.

Al abrir `HaloCoopHost.exe` (con MCC cerrado) busca MCC en tus bibliotecas de Steam y **copia solo
`VERSION.dll`** en `mcc\binaries\win64`, o lo actualiza si cambió. El panel muestra si el mod y
AlphaRing están instalados; si no pudo copiarlo (permisos), cópialo tú a esa carpeta.

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

### Video
El panel muestra la línea **Video**: unos segundos después de abrir MCC dice *listo* con la resolución
del juego y los FPS y Mbps que recibe cada amigo. Con el selector **Video a 30/60/120 FPS** eliges cuánto
mandar: más FPS = menos retraso, pero más subida de internet. Cada amigo recibe un cuarto de la
resolución del juego (con el juego a 2560x1440, cada uno ve 1280x720).

### Audio
HaloCoop Host captura **solo el sonido de MCC** (no Discord, música ni otras apps) y lo manda igual a
todos los amigos (~0,1 Mbps cada uno). El panel muestra *Audio: activo* cuando MCC está abierto.
Requiere Windows 10 versión 2004 o más nuevo.

### Probar los mandos
En HaloCoop Host, el botón **Mover** de cada jugador hace que su personaje camine hacia adelante,
atrás, a los lados, gire y salte (unos 10 segundos). Después de presionarlo, el juego vuelve a quedar
en primer plano solo.

### Problemas frecuentes
- **Un jugador no se mueve:** cierra MCC, cierra HaloCoop Host, ábrelos de nuevo en ese orden.
- **Tengo un mando físico conectado:** los virtuales quedan en otros Controller. HaloCoop Host lo
  avisa y muestra cuál le tocó a cada jugador; en AlphaRing asigna esos Controller a cada jugador.
- **HaloCoop Host dice que falta ViGEmBus:** instálalo (paso 2) y reinicia el PC.
- **Video: el juego no responde:** espera unos 10 segundos tras abrir MCC. Si sigue, revisa que el panel
  diga *Mod del juego: instalado* y mira `halocoop-juego.log` en `mcc\binaries\win64`.

### Problemas conocidos de AlphaRing con 4 jugadores
- Con gráficos remasterizados solo se ven los jugadores 1 y 2 → usar gráficos clásicos (Tab).
- En algunos niveles (*Assault on the Control Room*, *Keyes*) los jugadores 3 y 4 pueden aparecer fuera del mapa.
- Al terminar un nivel con 4 jugadores el juego se puede congelar: guarda y reinicia la misión si pasa.

---

## Instalación y uso del cliente

Cada amigo descarga la carpeta `cliente/` y abre `HaloCoopClient.exe`
(si Windows muestra "Windows protegió su PC": *Más información → Ejecutar de todas formas*).

1. Escribe la **IP de Radmin del host** (la ve el host arriba en su HaloCoop Host) y tu nombre.
2. Elige tu número de jugador: **2** (arriba a la derecha), **3** (abajo a la izquierda) o
   **4** (abajo a la derecha). Cada amigo uno distinto.
3. Elige **Mando** o **Teclado y mouse** y presiona **Conectar**.

Cuando llega el video, el cliente pasa a **pantalla completa** con tu cuadrante del juego.

| Tecla | Qué hace |
|-------|----------|
| **F2** | Menú: volumen, sensibilidad, controles, desconectar |
| **F1** | Estadísticas: ping, FPS del video, Mbps, fotogramas perdidos |
| **F3** | Silenciar / activar el sonido del juego |
| **F11** o **Alt+Enter** | Pantalla completa / ventana |
| **Esc** | Suelta el mouse (teclado y mouse) |
| **+ / -** | Sensibilidad del mouse (0.1x a 99x) |

- Con **mando** no hace falta nada más.
- Con **teclado y mouse** haz clic en el video para capturar el mouse. La lista de teclas está en
  `cliente/LEEME.txt`.
- Si el video se congela un momento al perder paquetes, se recupera solo en menos de medio segundo.
- El **volumen del juego** se ajusta con la barra del menú principal o de F2 (se guarda); **F3** silencia.

### Primera vez en el host: firewall
La primera vez que abres HaloCoop Host, Windows pregunta si permite el acceso a la red. Marca
**redes privadas y públicas** (Radmin VPN suele quedar como red pública) y acepta. Si no aparece y
tus amigos no pueden conectarse, permite `HaloCoopHost.exe` en *Firewall de Windows → Permitir una
aplicación*.

### Panel del host
Cada jugador conectado aparece con su nombre, su IP y cuántos controles por segundo llegan
(unos 250 es lo normal). **Expulsar** lo desconecta (puede volver a conectarse).

---

## Desinstalar

Borra de `mcc\binaries\win64\` los archivos `WTSAPI32.dll` (AlphaRing), `VERSION.dll` (este mod) y
`halocoop-juego.log`.
Luego, en Steam: clic derecho en MCC → Propiedades → Archivos instalados → **Verificar integridad**.
