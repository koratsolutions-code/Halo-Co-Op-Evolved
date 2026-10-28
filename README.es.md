# Halo: Co Op Evolved

**Español** · [English](README.md)

Campaña de **Halo: Combat Evolved** (Halo: The Master Chief Collection, Steam) para **4 jugadores**,
cada uno en su propio PC y con su propia pantalla completa, conectados por **Radmin VPN** al PC del host.

El juego corre una sola vez en el PC del host con la pantalla dividida en 4 (AlphaRing). El mod del host
envía a cada amigo solo su cuarto de pantalla y su sonido, y recibe sus controles de vuelta.

Las dos apps vienen en **inglés**. Para usarlas en español elige **Español** en el selector de arriba a
la derecha (se guarda). Esta guía usa los textos en español.

## Descarga

**No** hace falta compilar ni instalar herramientas de programación: los archivos listos para usar ya
vienen en este repositorio.

1. Arriba en esta página presiona el botón verde **Code** → **Download ZIP**.
2. Descomprímelo (clic derecho → *Extraer todo...*).
3. El host usa la carpeta `host/`; cada amigo solo necesita la carpeta `cliente/`.

---

## Estructura del repositorio

| Carpeta    | Para quién        | Contenido |
|------------|-------------------|-----------|
| `host/`    | El que hospeda    | `CoOpEvolvedHost.exe` (launcher y panel), `VERSION.dll` (mod del juego, se instala solo) e instrucciones (`LEEME.txt` / `README.txt`) |
| `cliente/` | Los otros 3 jugadores | `CoOpEvolvedClient.exe` e instrucciones (`LEEME.txt` / `README.txt`) |

---

## Requisitos

**Host**
- Halo: The Master Chief Collection en **Steam** (versión 1.3528.0.0).
- GPU NVIDIA (GTX 900 o más nueva) con drivers actualizados: el video se codifica con NVENC.
- Internet con unos **10 Mbps de subida por amigo a 60 FPS** (30 Mbps para 3 amigos).
- [Radmin VPN](https://www.radmin-vpn.com/) instalado y conectado a la misma red que los amigos.
- Windows 10 versión 2004 o más nuevo (para capturar solo el sonido del juego).

**Cada cliente**
- Windows 10/11 con GPU NVIDIA y drivers actualizados (el video se decodifica con NVDEC).
- Radmin VPN conectado a la misma red que el host.
- Mando de Xbox o teclado y mouse.

---

## Instalación del host

### Carpeta del juego
Varios pasos usan la carpeta `mcc\binaries\win64` del juego. Para encontrarla: en Steam, clic derecho
en **Halo: The Master Chief Collection → Administrar → Ver archivos locales**, y dentro abre
`mcc` → `binaries` → `win64`.

### 1. AlphaRing (pantalla dividida para 4 jugadores)

AlphaRing no se incluye en este repositorio: se descarga de su página oficial.

1. Descarga `Release1.0.zip` de la release **1.3528.0.0** en
   <https://github.com/wouter51/AlphaRing/releases>.
2. Descomprime y copia `WTSAPI32.dll` en la carpeta `mcc\binaries\win64` del juego.
3. (Opcional) Comprueba que es el archivo correcto: en esa carpeta, Shift + clic derecho →
   *Abrir en Terminal* y ejecuta:
   ```powershell
   Get-FileHash .\WTSAPI32.dll
   ```
   Debe empezar por `091AA64DCC040BB6`.

### 2. Driver de mandos virtuales (ViGEmBus)

Instala **ViGEmBus** desde <https://github.com/nefarius/ViGEmBus/releases/latest>
(archivo `ViGEmBus_..._x64_x86_arm64.exe`) y reinicia el PC. Si falta, `CoOpEvolvedHost.exe` lo avisa
y abre esa página.

### 3. Co Op Evolved Host

Copia la carpeta `host/` a donde quieras (no hace falta ponerla en la carpeta del juego).
Contiene `CoOpEvolvedHost.exe`, que crea los mandos de los jugadores 2, 3 y 4, y `VERSION.dll`, el mod
que captura el video dentro del juego.

Al abrir `CoOpEvolvedHost.exe` (con MCC cerrado) busca MCC en tus bibliotecas de Steam y **copia solo
`VERSION.dll`** en `mcc\binaries\win64`, o lo actualiza si cambió. El panel muestra si el mod y
AlphaRing están instalados; si no pudo copiarlo (permisos), cópialo tú a esa carpeta.

---

## Cómo jugar (host)

> **Importante:** abre siempre **Co Op Evolved Host primero** y MCC después. Los mandos de los jugadores
> 2, 3 y 4 tienen que existir antes de que arranque el juego; si no, esos jugadores no se pueden mover.
> Si MCC ya está abierto, el launcher te pide cerrarlo.

1. Desconecta cualquier mando físico (cambia el orden de los mandos). Si quieres jugar con mando
   como jugador 1, ver *Problemas frecuentes*.
2. Abre `CoOpEvolvedHost.exe`. Arriba muestra tu IP de Radmin (pásasela a tus amigos) y qué Controller
   le tocó a cada jugador (normalmente J2 = Controller 1, J3 = Controller 2, J4 = Controller 3).
   **No lo cierres mientras jueguen.**
3. Escribe tu nombre en **Nickname** y elige tu color en **Armadura** (tarjeta del jugador 1).
4. Presiona **Abrir MCC** y elige **sin anticheat (EAC desactivado)**.
5. Presiona **F4** para abrir el menú de AlphaRing → **Splitscreen**:
   - **Players = 4**.
   - No cambies las demás opciones. En especial, deja activado *"Use player1's profile"*.
   - AlphaRing **no guarda** la configuración: hay que hacerlo cada vez que se abre el juego.
6. Cierra el menú con **F4**. Mientras está abierto, **ningún jugador se puede mover**.
7. En Halo CE activa los **gráficos clásicos** (tecla **Tab** en partida): con los gráficos nuevos
   solo se ven 2 jugadores.
8. Mantén la ventana del juego **activa** (en primer plano) mientras juegan: si cambias a otra
   ventana, MCC deja de leer los mandos.

### Video
El panel muestra la línea **Video**: unos segundos después de abrir MCC dice *Video listo* con la
resolución del juego, y la tarjeta de cada jugador muestra los FPS y Mbps que recibe ese amigo. Con el selector **Video a 30/60/120 FPS** eliges cuánto
mandar: más FPS = menos retraso, pero más subida de internet. Cada amigo recibe un cuarto de la
resolución del juego (con el juego a 2560x1440, cada uno ve 1280x720).

### Audio
Co Op Evolved Host captura **solo el sonido de MCC** (no Discord, música ni otras apps) y lo manda igual a
todos los amigos (~0,1 Mbps cada uno). El panel muestra *Audio activo* cuando MCC está abierto.

### Nombres sobre los jugadores
El mod del juego dibuja el nombre de cada jugador sobre su personaje, en tu pantalla y en el video de cada
amigo. Se ven también a través de paredes y se achican y aclaran con la distancia. El nombre del jugador 1
es el **Nickname** del panel; los jugadores 2-4 usan el nombre que escribieron en su cliente.
- **Mostrar nombres sobre los jugadores** los activa o desactiva.
- Si los nombres no quedan justo sobre los personajes, ajusta **FOV del juego** al campo de visión del juego.
- Los nombres aparecen unos segundos después de que carga cada nivel.

### Jugador 1 a pantalla completa
Por defecto ves toda la pantalla dividida. Con **Jugador 1 a pantalla completa (F6 en el juego)** marcado, ves
solo tu parte (arriba a la izquierda) estirada a todo tu monitor; tus amigos siguen recibiendo la suya.
- **F6** cambia entre pantalla completa y dividida mientras juegas (el juego tiene que ser la ventana activa).
  La casilla del panel solo elige cómo empieza.
- Los menús de pausa de MCC se dibujan para toda la pantalla, así que en pantalla completa se ven cortados:
  presiona **F6** para usarlos en pantalla dividida.
- Tu imagen es un cuarto de la resolución del juego estirado, así que con más resolución se ve más nítido
  (a 2560x1440 tu parte es de 1280x720).
- Solo cambia lo que muestra tu monitor: no toca el juego, los mandos ni AlphaRing.

### Colores de armadura
La personalización de armadura de MCC solo vale en multijugador, así que el mod del juego pinta la armadura
de cada jugador en la campaña. El jugador 1 la elige en **Armadura** (tarjeta del jugador 1); los jugadores
2-4 en **Color de armadura** de su cliente (también en F2 durante la partida). Están los 18 colores de Halo CE
más **Por defecto** (el verde oliva de siempre). El color se vuelve a poner solo al reaparecer o al cargar
otro nivel, y cada tarjeta de jugador lo muestra.

### Probar los mandos
En Co Op Evolved Host, el botón **Mover** de cada jugador hace que su personaje camine hacia adelante,
atrás, a los lados, gire y salte (unos 10 segundos). Después de presionarlo, el juego vuelve a quedar
en primer plano solo.

### Primera vez: firewall
La primera vez que abres Co Op Evolved Host, Windows pregunta si permite el acceso a la red. Marca
**redes privadas y públicas** (Radmin VPN suele quedar como red pública) y acepta. Si no aparece y
tus amigos no pueden conectarse, permite `CoOpEvolvedHost.exe` en *Firewall de Windows → Permitir una
aplicación*.

### Seguridad
- Co Op Evolved Host solo responde a **Radmin VPN** (26.x.x.x), a tu **red local** y a tu propio PC. Los paquetes
  de internet se ignoran, aunque el puerto esté abierto en el firewall o reenviado en tu router. No hace falta
  (ni conviene) reenviar ningún puerto.
- Cualquiera de tu red de Radmin puede tomar un jugador libre: protege la red de Radmin con contraseña y
  compártela solo con tus amigos. **Expulsar** saca a alguien en cualquier momento.
- El mod del juego solo lee la memoria de Halo 1, salvo los colores de armadura de los 4 jugadores. No toca
  tus archivos, tu cuenta de Steam ni tu perfil de MCC.
- `coopevolved-host.log` y `coopevolved-cliente.log` guardan los nombres e IPs de Radmin de tus amigos. Se quedan
  en tu PC: no los publiques.

### Panel del host
Está ordenado como la pantalla dividida: una tarjeta por jugador. Cada amigo conectado muestra su nombre,
su IP, cuántos controles por segundo llegan (unos 250 es lo normal) y el video que recibe. **Expulsar** lo
desconecta (puede volver a conectarse). El panel guarda tu nickname, el idioma, los FPS y lo de los
nombres en `coopevolved-host.ini`, junto a `CoOpEvolvedHost.exe`.

### Problemas frecuentes
- **Un jugador no se mueve:** cierra MCC, cierra Co Op Evolved Host, ábrelos de nuevo en ese orden.
- **Tengo un mando físico conectado:** los virtuales quedan en otros Controller. Co Op Evolved Host lo
  avisa y muestra cuál le tocó a cada jugador; en AlphaRing asigna esos Controller a cada jugador.
- **Co Op Evolved Host dice que falta ViGEmBus:** instálalo (paso 2) y reinicia el PC.
- **Video: el juego no responde:** espera unos 10 segundos tras abrir MCC. Si sigue, revisa que el panel
  muestre *Mod del juego* con punto verde y mira `coopevolved-juego.log` en la carpeta `mcc\binaries\win64`.
- **No salen los nombres:** revisa que *Mostrar nombres sobre los jugadores* esté activado y espera unos
  segundos después de que cargue el nivel. Los nombres solo funcionan con MCC versión 1.3528.

### Problemas conocidos de AlphaRing con 4 jugadores
- Con gráficos remasterizados solo se ven los jugadores 1 y 2 → usar gráficos clásicos (Tab).
- En algunos niveles (*Assault on the Control Room*, *Keyes*) los jugadores 3 y 4 pueden aparecer fuera del mapa.
- Al terminar un nivel con 4 jugadores el juego se puede congelar: guarda y reinicia la misión si pasa.

---

## Cómo jugar (cliente)

Para los jugadores 2, 3 y 4. **No** necesitas Halo, Steam ni MCC: el juego corre en el PC del host.

### Antes de la primera vez
1. **Radmin VPN:** instala [Radmin VPN](https://www.radmin-vpn.com/) y únete a la red del host
   (*Red → Unirse a una red existente*, con el nombre y la contraseña de red que te da el host).
2. **Drivers de NVIDIA:** actualiza los drivers de tu GPU NVIDIA (GeForce Experience / NVIDIA App, o nvidia.com).
3. **El cliente:** descarga el repositorio (*Descarga*) y copia la carpeta `cliente/` donde quieras.
   No hay que instalar nada más.

### Cada vez que jueguen
1. Revisa que Radmin VPN esté **encendido** y que el host aparezca conectado en tu red de Radmin.
2. Abre `CoOpEvolvedClient.exe`. La primera vez Windows puede mostrar "Windows protegió su PC":
   *Más información → Ejecutar de todas formas*.
3. Llena el menú (se guarda todo para la próxima vez):
   - **IP de Radmin del host:** el host la ve arriba en su Co Op Evolved Host (botón *Copiar*).
   - **Tu nombre:** sale sobre tu personaje para todos.
   - **Color de armadura:** tu color en la campaña.
   - **Jugador:** **2** (arriba a la derecha), **3** (abajo a la izquierda) o **4** (abajo a la derecha).
     Cada amigo uno distinto; usa siempre el mismo.
   - **Mando** o **Teclado y mouse**.
4. Presiona **Conectar**. Cuando el host abre el juego, el cliente pasa a **pantalla completa** con tu parte
   de la pantalla y el sonido del juego.
5. Para salir: **F2 → Desconectar**, o cierra la ventana.

### Teclas
| Tecla | Qué hace |
|-------|----------|
| **F2** | Menú: color de armadura, volumen, sensibilidad, controles, desconectar |
| **F1** | Estadísticas: ping, FPS del video, Mbps, fotogramas perdidos |
| **F3** | Silenciar / activar el sonido del juego |
| **F11** o **Alt+Enter** | Pantalla completa / ventana |
| **Esc** | Suelta el mouse (teclado y mouse) |
| **+ / -** | Sensibilidad del mouse (0.1x a 99x) |

- Con **mando** no hace falta nada más (se recomienda mando de Xbox).
- Con **teclado y mouse** haz clic en el video para capturar el mouse. La lista completa de teclas está en
  `cliente/LEEME.txt` y en F2 → *Controles del teclado*.
- **Mira directa con mouse (beta)** (teclado y mouse, en el menú o en F2): el mouse gira tu cámara directo, como
  Halo en PC, en vez de mover el stick derecho del mando (sin zona muerta ni aceleración del stick). Al lado,
  *activa* indica que funciona; si el juego del host todavía no puede usarla, se usa el stick como antes. El panel
  del host muestra *mira con mouse* en tu tarjeta. Se ajusta con **+ / -**.
- Si el video se congela un momento al perder paquetes, se recupera solo en menos de medio segundo.
- El **volumen del juego** se ajusta con la barra del menú principal o de F2 (se guarda); **F3** silencia.

### Problemas frecuentes (cliente)
- **"El host no responde":** revisa la IP, que Radmin VPN esté conectado (ping al host desde Radmin) y que el
  host tenga Co Op Evolved Host abierto.
- **"Esperando el video...":** el host todavía no abrió MCC, o su panel muestra que falta el mod del juego.
- **"Ese jugador ya está ocupado":** otro amigo eligió tu número; elige otro.
- **Pantalla negra o error de NVIDIA en laptop:** *Configuración de Windows → Pantalla → Gráficos* → pon
  `CoOpEvolvedClient.exe` en **Alto rendimiento**.
- **Tu personaje no se mueve:** el juego tiene que estar en primer plano en el PC del host; avísale.
- Si algo falla, mándale al host tu `coopevolved-cliente.log` (en la carpeta del cliente) por privado.

---

## Compilar desde el código (opcional)

**Solo si quieres cambiar el código.** Para jugar usa los archivos que ya vienen en `host/` y `cliente/`
(ver *Descarga*).

1. **Instala Visual Studio** (gratis):
   1. Entra a <https://visualstudio.microsoft.com/downloads/> y descarga **Visual Studio Community**.
   2. Abre el instalador. En la pestaña **Cargas de trabajo** marca **Desarrollo para el escritorio con
      C++** y presiona **Instalar**. Descarga varios GB y tarda un rato; no hace falta cambiar nada más.
   3. Cuando termine puedes cerrar Visual Studio: no tiene que quedar abierto.
2. **Descarga el código**: botón verde **Code** → **Download ZIP**, y descomprímelo.
3. **Compila**: en la carpeta descomprimida, haz doble clic en **`build.cmd`**.
   - Si Windows muestra "Windows protegió su PC": *Más información → Ejecutar de todas formas*.
   - Se abre una ventana negra. La primera vez tarda unos minutos; después es mucho más rápido.
   - Termina con **`DONE: ...`** y *Presione una tecla*: los archivos nuevos ya están en `host/` y
     `cliente/`.
   - Si termina con **`BUILD FAILED`**, lee los mensajes de arriba. Lo más común: Visual Studio se instaló
     sin *Desarrollo para el escritorio con C++* (abre *Visual Studio Installer* → **Modificar** y márcalo),
     o `CoOpEvolvedHost.exe` / `CoOpEvolvedClient.exe` estaban abiertos al compilar.

Compilar también crea una carpeta `build/` con archivos temporales; puedes borrarla.

---

## Desinstalar

Borra de la carpeta `mcc\binaries\win64` del juego los archivos `WTSAPI32.dll` (AlphaRing),
`VERSION.dll` (este mod) y `coopevolved-juego.log`.
Luego, en Steam: clic derecho en MCC → Propiedades → Archivos instalados → **Verificar integridad**.
