# Halo: Co Op Evolved

**Español** · [English](README.md)

Campaña de **Halo: Combat Evolved** (Halo: The Master Chief Collection, Steam) para **4 jugadores**,
cada uno en su PC y con su propia pantalla completa, conectados por **Radmin VPN** al PC del host.

El juego corre una sola vez, en el PC del host, con la pantalla dividida entre los jugadores (2, 3 o 4). Co Op
Evolved le manda a cada amigo solo su parte de la pantalla más el sonido del juego, y recibe sus controles. Durante
las cinemáticas todos ven la pantalla entera.

Es un solo programa, `CoOpEvolved.exe`, para todos: el host elige **Crear partida** y los amigos **Unirse a una
partida**. Viene en **inglés** por defecto; cámbialo a **Español** con el selector de arriba a la derecha.

## Descarga

**No** necesitas compilar ni instalar herramientas de programación: los archivos listos para usar ya
vienen en este repositorio.

1. Arriba en esta página presiona el botón verde **Code** → **Download ZIP**.
2. Descomprímelo (clic derecho → *Extraer todo...*).
3. Todos (host y amigos) usan la carpeta `CoOpEvolved/`. Cópiala donde quieras.

| Archivo en `CoOpEvolved/` | Qué es |
|---------------------------|--------|
| `CoOpEvolved.exe` | El programa: panel del host o cliente, se elige arriba a la derecha |
| `VERSION.dll` | El mod dentro del juego. Solo lo necesita el host: el programa lo instala solo |
| `README.txt` / `LEEME.txt` | Instrucciones cortas |

---

## Requisitos

**Host**
- Halo: The Master Chief Collection en **Steam** (versión 1.3528.0.0).
- GPU NVIDIA (GTX 900 o más nueva) con drivers al día: el video se codifica con NVENC.
- Unos **10 Mbps de subida por amigo a 60 FPS** (30 Mbps para 3 amigos).
- [Radmin VPN](https://www.radmin-vpn.com/) instalado y en la misma red que tus amigos.
- Windows 10 versión 2004 o más nuevo (para capturar solo el sonido del juego).

**Cada amigo**
- Windows 10/11 con GPU NVIDIA y drivers al día (el video se decodifica con NVDEC).
- Radmin VPN en la misma red que el host.
- Mando de Xbox, o teclado y mouse.
- **No** necesitan Halo, Steam ni MCC.

No hay que instalar nada más: ni AlphaRing ni drivers de mandos virtuales.

---

## Cómo jugar (host)

1. Abre `CoOpEvolved.exe` y elige **Crear partida** arriba a la derecha (desde ahí abre siempre en ese modo).
   Con MCC cerrado, busca el juego en tus bibliotecas de Steam e **instala o actualiza `VERSION.dll` solo**
   en la carpeta `mcc\binaries\win64` del juego. **No lo cierres mientras jueguen.**
2. Arriba sale tu **IP de Radmin**: pásasela a tus amigos (botón *Copiar*).
3. Escribe tu nombre en **Nickname** y elige tu color en **Armadura** y tu **Skin** (tarjeta del jugador 1).
4. Presiona **Abrir MCC** y en la ventana de Steam elige **"Play Halo: MCC (Anti-Cheat Disabled)"**.
5. Empieza una misión de la campaña de Halo CE: tiene **tantos jugadores como amigos conectados**, sin ningún
   menú extra (ver *Jugadores y quien llega tarde*).
6. Activa los **gráficos clásicos** (tecla **Tab** en el juego): con los gráficos nuevos solo se ven 2 jugadores.
7. Deja la ventana del juego **activa** (al frente) mientras juegan: si cambias a otra ventana, MCC deja de
   leer los mandos.

El jugador 1 (tú) juega con **teclado y mouse**.

### Jugadores y quien llega tarde
- Cuando una misión empieza **desde el menú de MCC**, tiene al jugador 1 más todos los amigos hasta el número de
  jugador conectado más alto: con el jugador 2 conectado son 2 jugadores, con el 2 y el 3 son 3. Cada amigo
  conserva su número, así que si están el 2 y el 4 son 4 y el 3 queda quieto. Pídeles a tus amigos que usen el 2,
  el 3 y el 4 en ese orden.
- La pantalla se reparte según eso: **2 jugadores** = mitades arriba y abajo; **3 jugadores** = el jugador 1 en la
  mitad de arriba, el 2 abajo a la izquierda y el 3 abajo a la derecha; **4 jugadores** = cuartos.
- Un amigo que se conecta **con la misión ya empezada** mira la pantalla entera con un aviso. Para que entre,
  vuelve al menú de MCC y empieza la misión otra vez: reiniciar desde el menú de pausa deja los mismos jugadores.

### Carpeta del juego
Para encontrar la carpeta `mcc\binaries\win64` del juego: en Steam, clic derecho en
**Halo: The Master Chief Collection → Administrar → Ver archivos locales**, y entra a `mcc` → `binaries` →
`win64`.

### Video
El panel muestra la línea **Video**: unos segundos después de abrir MCC dice *Video listo* con la resolución
del juego, y cada tarjeta de jugador muestra los FPS y Mbps que recibe ese amigo. El selector **Video a
30/60/120 FPS** elige cuánto mandar: más FPS = menos retraso, pero más subida. Cada amigo recibe su parte de la
resolución del juego (con el juego a 2560x1440 y 4 jugadores, cada uno ve 1280x720; con 2 jugadores, el jugador 2
recibe una franja ancha de 2560x720).

### Cinemáticas
Halo dibuja las cinemáticas una sola vez sobre toda la pantalla. Mientras dura una, cada amigo recibe **la
pantalla entera** (reducida a su tamaño) en vez de su cuarto, y al terminar vuelve a su cuarto. **Cinemáticas
completas para los amigos** lo desactiva. Los subtítulos durante el juego los sigue dibujando MCC sobre toda la
pantalla dividida, así que los amigos pueden verlos cortados.

### Audio
Co Op Evolved captura **solo el sonido de MCC** (no Discord, música ni otras apps) y manda la misma mezcla a
cada amigo (~0,1 Mbps cada uno). El panel muestra *Audio activo* mientras MCC está abierto.

### Nombres sobre los jugadores
El mod del juego dibuja el nombre de cada jugador sobre su personaje, en tu pantalla y en el video de cada
amigo. Los nombres se ven a través de las paredes y se achican y aclaran con la distancia. El nombre del
jugador 1 es el **Nickname** del panel; los jugadores 2-4 usan el que escribieron en su cliente.
- **Mostrar nombres sobre los jugadores** los activa o desactiva.
- Si los nombres no quedan justo sobre los personajes, ajusta **FOV del juego** al campo de visión del juego.
- Los nombres aparecen unos segundos después de cargar cada nivel.

### Jugador 1 a pantalla completa
Por defecto ves toda la pantalla dividida. Con **Jugador 1 a pantalla completa (F6 en el juego)** marcado, ves
solo tu parte (arriba a la izquierda) estirada a todo tu monitor; tus amigos siguen recibiendo la suya.
- **F6** cambia entre pantalla completa y dividida mientras juegas (el juego tiene que ser la ventana activa).
  La casilla del panel solo elige cómo empieza.
- Los menús de pausa de MCC se dibujan para toda la pantalla, así que a pantalla completa se ven cortados:
  presiona **F6** para usarlos en pantalla dividida. Las cinemáticas se ven enteras.
- Tu imagen es tu parte de la resolución del juego estirada: con más resolución en el juego se ve más nítido
  (a 2560x1440 con 4 jugadores tu parte es 1280x720). Con 2 o 3 jugadores tu parte es una franja ancha: se
  muestra sin estirar, con franjas negras arriba y abajo.

### Colores de armadura
La personalización de armadura de MCC solo vale en multijugador, así que el mod del juego pinta la armadura
de cada jugador en la campaña. El jugador 1 la elige en **Armadura** (tarjeta del jugador 1); los jugadores
2-4 en **Color de armadura** de su cliente (también en F2 durante la partida). Están los 18 colores de Halo CE
más **Por defecto** (el verde oliva de siempre). El color se vuelve a poner solo al reaparecer o al cargar
otro nivel, y cada tarjeta de jugador lo muestra.

### Skins
Cada jugador puede verse como otro personaje de Halo CE y seguir siendo Master Chief en todo lo demás (hitbox,
velocidad, salto). El jugador 1 la elige en **Skin** (tarjeta del jugador 1); los jugadores 2-4 en **Skin** de
su cliente (también en F2 durante la partida). Cada tarjeta de jugador muestra la skin de ese amigo.

Skins: Master Chief, Marine, Marine con armadura, Tripulante, Capitán Keyes (campaña), Capitán Keyes (gala),
Jenkins, Marine Flood y Elite.
- Una skin solo existe en las misiones que tienen a ese personaje. Con una misión cargada, el selector muestra
  en gris las que no tiene; si la tuya no está, en esa misión eres Master Chief.
- Una skin nueva se ve **la próxima vez que reapareces**: el cuerpo que tienes conserva su aspecto hasta morir.
- Las misiones que empiezan con una cinemática que coloca a los jugadores (por ejemplo *Halo*, con la cápsula)
  empiezan con todos como Master Chief; cada jugador recibe su skin al reaparecer por primera vez.
- Solo cambia el aspecto: los disparos siguen dando donde estaría el cuerpo de Master Chief.

### Probar los mandos
El botón **Mover** de cada jugador hace que su personaje camine adelante, atrás, a los lados, gire y salte
(unos 10 segundos). Después el juego vuelve al frente.

### Jugar como cliente en el mismo PC
Para probar, puedes abrir `CoOpEvolved.exe` otra vez con el host abierto: esa ventana se abre sola en **Unirse
a una partida** (conéctate a `127.0.0.1`). Solo puede haber un host por PC: una segunda ventana de host se abre
en gris y no hace nada.

### Primera vez: firewall
La primera vez que creas una partida, Windows pregunta si permitir el acceso a la red. Marca **redes privadas y
públicas** (Radmin VPN suele ser una red pública) y acepta. Si no aparece y tus amigos no pueden conectarse,
permite `CoOpEvolved.exe` en *Firewall de Windows → Permitir una aplicación*.

### Seguridad
- El host solo atiende a **Radmin VPN** (26.x.x.x), tu **red local** y tu propio PC. Los paquetes que llegan
  de internet se ignoran, aunque el puerto esté abierto en el firewall o redirigido en tu router. No necesitas
  (ni debes) abrir ningún puerto.
- Cualquiera de tu red de Radmin puede tomar un jugador libre: protege la red de Radmin con contraseña y
  compártela solo con tus amigos. **Expulsar** saca a alguien en cualquier momento.
- El mod del juego cambia el juego solo mientras corre: los 4 jugadores locales, los controles de los
  jugadores 2-4, los colores de armadura, las skins y las cinemáticas. No toca tus archivos, tu cuenta de
  Steam ni tu perfil de MCC.
- `coopevolved-host.log` y `coopevolved-cliente.log` guardan los nombres e IPs de Radmin de tus amigos. Se quedan
  en tu PC: no los publiques.

### Panel del host
Está ordenado como la pantalla dividida: una tarjeta por jugador. Cada amigo conectado muestra su nombre,
su IP, cuántos controles por segundo llegan (unos 250 es lo normal) y el video que recibe. **Expulsar** lo
desconecta (puede volver a conectarse). El panel guarda tu nickname, tu color de armadura, tu skin, el idioma,
los FPS y lo de los nombres en `coopevolved-host.ini`, junto a `CoOpEvolved.exe`.

### Problemas comunes
- **El juego no tiene los jugadores que debería:** la cantidad se fija al empezar la misión desde el menú de
  MCC (ver *Jugadores y quien llega tarde*). Revisa que el panel muestre en verde *4 jugadores: integrado* y que
  `VERSION.dll` esté instalado (punto *Mod del juego*). Solo funciona con MCC 1.3528 de Steam.
- **AlphaRing sigue instalado** (de una versión anterior de Co Op Evolved): funciona igual, pero entonces
  AlphaRing pone los jugadores (F4 → Players = 4 cada vez). Para usar los 4 jugadores integrados, borra
  `WTSAPI32.dll` de la carpeta `mcc\binaries\win64` del juego.
- **Video: el juego no responde:** espera unos 10 segundos después de abrir MCC. Si sigue, revisa que el panel
  muestre en verde *Mod del juego* y mira `coopevolved-juego.log` en la carpeta `mcc\binaries\win64`.
- **No salen nombres sobre los jugadores:** revisa que *Mostrar nombres sobre los jugadores* esté activado y
  espera unos segundos después de cargar el nivel. Los nombres solo funcionan con MCC versión 1.3528.

### Problemas conocidos con 4 jugadores
- Con los gráficos remasterizados solo se ven los jugadores 1 y 2 → usa los gráficos clásicos (Tab).
- En algunos niveles (*Assault on the Control Room*, *Keyes*) los jugadores 3 y 4 pueden aparecer fuera del
  mapa.
- Terminar un nivel con 4 jugadores congelaba el juego; el mod trae un arreglo, pero si igual pasa, guarda y
  reinicia la misión.

---

## Cómo jugar (amigos)

Para los jugadores 2, 3 y 4. **No** necesitas Halo, Steam ni MCC: el juego corre en el PC del host.

### Antes de la primera vez
1. **Radmin VPN:** instala [Radmin VPN](https://www.radmin-vpn.com/) y únete a la red del host
   (*Red → Unirse a una red existente*, con el nombre y la contraseña que te da el host).
2. **Drivers de NVIDIA:** actualiza los drivers de tu GPU NVIDIA (GeForce Experience / NVIDIA App, o
   nvidia.com).
3. **El programa:** descarga el repositorio (*Descarga*) y copia la carpeta `CoOpEvolved/` donde quieras. No
   hay que instalar nada más.

### Cada vez que juegues
1. Revisa que Radmin VPN esté **encendido** y que el host aparezca conectado en tu red de Radmin.
2. Abre `CoOpEvolved.exe`. La primera vez Windows puede mostrar "Windows protegió su PC":
   *Más información → Ejecutar de todas formas*. Si abre el panel del host, elige **Unirse a una partida**
   arriba a la derecha.
3. Llena el menú (se guarda todo para la próxima vez):
   - **IP de Radmin del host:** el host la ve arriba en su Co Op Evolved (botón *Copiar*).
   - **Tu nombre:** sale sobre tu personaje para todos.
   - **Color de armadura:** tu color en la campaña.
   - **Skin:** el personaje que pareces (ver *Skins* en la parte del host).
   - **Jugador:** **2** (arriba a la derecha), **3** (abajo a la izquierda) o **4** (abajo a la derecha).
     Cada amigo uno distinto; usa siempre el mismo.
   - **Mando** o **Teclado y mouse**.
4. Presiona **Conectar**. Cuando el host abre el juego, el cliente pasa a **pantalla completa** con tu parte
   de la pantalla y el sonido del juego. Conéctate **antes de que el host empiece la misión**: si entras después,
   miras la pantalla entera hasta que el host vuelva a empezar la misión desde el menú de MCC.
5. Para salir: **F2 → Desconectar**, o cierra la ventana.

### Teclas
| Tecla | Qué hace |
|-------|----------|
| **F2** | Menú: color de armadura, skin, volumen, sensibilidad, controles, desconectar |
| **F1** | Estadísticas: ping, FPS del video, Mbps, fotogramas perdidos |
| **F3** | Silenciar / activar el sonido del juego |
| **F11** o **Alt+Enter** | Pantalla completa / ventana |
| **Esc** | Suelta el mouse (teclado y mouse) |
| **+ / -** | Sensibilidad del mouse (0,1x a 99x) |

- Con **mando** no hace falta nada más (se recomienda un mando de Xbox).
- Con **teclado y mouse** haz clic en el video para capturar el mouse. La lista completa de teclas está en
  `CoOpEvolved/LEEME.txt` y en F2 → *Controles del teclado*.
- **Mira directa con mouse (beta)** (teclado y mouse, en el menú o F2): el mouse gira tu cámara directo,
  como Halo en PC, en vez de mover el stick derecho del mando (sin zona muerta ni aceleración del stick). A su
  lado, *activa* significa que está funcionando; si el juego del host todavía no puede usarla, se usa el stick
  como antes. El panel del host muestra *mira con mouse* en tu tarjeta. Ajústala con **+ / -**.
- Si el video se congela un momento por paquetes perdidos, se recupera solo en menos de medio segundo.
- El **volumen del juego** se ajusta con el control del menú principal o en F2 (se guarda); **F3** silencia.

### Problemas comunes (amigos)
- **"El host no responde":** revisa la IP, que Radmin VPN esté conectado (haz ping al host desde Radmin) y
  que el host tenga Co Op Evolved abierto en **Crear partida**.
- **"Esperando video...":** el host todavía no abre MCC, o su panel muestra que falta el mod del juego.
- **"Ese jugador ya está ocupado":** otro amigo eligió tu número; elige otro.
- **"Entraste con la misión ya empezada":** estás mirando; jugarás cuando el host vuelva a empezar la misión desde
  el menú de MCC.
- **"Tu cliente y el del host son de versiones distintas":** el host y tú tienen que usar la misma versión de Co Op Evolved; descárgala de nuevo.
- **Pantalla negra o error de NVIDIA en un portátil:** *Configuración de Windows → Pantalla → Gráficos* →
  pon `CoOpEvolved.exe` en **Alto rendimiento**.
- **Tu personaje no se mueve:** el juego tiene que ser la ventana activa en el PC del host; avísale.
- Si algo falla, mándale al host tu `coopevolved-cliente.log` (junto a `CoOpEvolved.exe`) por privado.

---

## Compilar desde el código (opcional)

**Solo si quieres cambiar el código.** Para jugar usa los archivos que ya vienen en `CoOpEvolved/` (ver
*Descarga*).

1. **Instala Visual Studio** (gratis):
   1. Entra a <https://visualstudio.microsoft.com/downloads/> y descarga **Visual Studio Community**.
   2. Abre el instalador. En la pestaña **Cargas de trabajo** marca **Desarrollo para el escritorio con C++**
      y presiona **Instalar**. Descarga varios GB y tarda un rato; no hace falta cambiar nada más.
   3. Cuando termine puedes cerrar Visual Studio: no tiene que quedar abierto.
2. **Descarga el código**: botón verde **Code** → **Download ZIP**, y descomprímelo.
3. **Compila**: en la carpeta descomprimida, haz doble clic en **`build.cmd`**.
   - Si Windows muestra "Windows protegió su PC": *Más información → Ejecutar de todas formas*.
   - Se abre una ventana negra. La primera vez tarda unos minutos; después es mucho más rápido.
   - Termina con **`DONE: ...`** y *Presione una tecla*: los archivos nuevos ya están en `CoOpEvolved/`.
   - Si termina con **`BUILD FAILED`**, lee los mensajes de arriba. Las causas más comunes: Visual Studio se
     instaló sin *Desarrollo para el escritorio con C++* (abre *Visual Studio Installer* → **Modificar** y
     márcalo), o `CoOpEvolved.exe` estaba abierto al compilar.

La compilación también crea una carpeta `build/` con archivos temporales; puedes borrarla.

---

## Desinstalar

Borra `VERSION.dll` (este mod), `coopevolved-juego.ini` y `coopevolved-juego.log` de la carpeta
`mcc\binaries\win64` del juego (y `WTSAPI32.dll` si instalaste AlphaRing para una versión anterior).
Después en Steam: clic derecho en MCC → Propiedades → Archivos instalados → **Verificar integridad**.

## Créditos

La pantalla dividida de 4 jugadores sigue a [AlphaRing](https://github.com/WinterSquire/AlphaRing) de
WinterSquire (licencia MIT); ver [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
