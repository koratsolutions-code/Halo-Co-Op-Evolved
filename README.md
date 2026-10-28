# halo1-coop

Campaña de **Halo: Combat Evolved** (Halo: The Master Chief Collection, Steam) para **4 jugadores**,
cada uno en su propio PC y con su propia pantalla completa, conectados por **Radmin VPN** al PC del host.

El juego corre una sola vez en el PC del host con la pantalla dividida en 4 (AlphaRing). El mod del host
envía a cada amigo solo su cuarto de pantalla y recibe sus controles de vuelta.

> **Estado actual:** proyecto reiniciado desde cero. Por ahora solo está documentada la instalación de
> AlphaRing en el host (paso 1). El mod del host y la app del cliente se irán agregando a las carpetas
> `host/` y `cliente/`.

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

### 2. Mod del host

*Pendiente: se agregará en `host/`.*

---

## Cómo jugar (host)

1. Abre MCC desde Steam eligiendo **sin anticheat (EAC desactivado)**.
2. Presiona **F4** para abrir el menú de AlphaRing → **Splitscreen**:
   - **Players = 4**.
   - No cambies las demás opciones. En especial, deja activado *"Use player1's profile"*.
   - AlphaRing **no guarda** la configuración: hay que hacerlo cada vez que se abre el juego.
3. Cierra el menú con **F4**. Mientras está abierto, **ningún jugador se puede mover**.
4. En Halo CE activa los **gráficos clásicos** (tecla **Tab** en partida): con los gráficos nuevos
   solo se ven 2 jugadores.
5. Mantén la ventana del juego **activa** (en primer plano) mientras juegan.

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
