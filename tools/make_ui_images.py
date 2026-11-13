# Genera las imágenes de los textos fijos de la interfaz (títulos, etiquetas y botones) con la fuente de Halo, y
# el logo listo para el programa. La fuente NO se distribuye: solo estas imágenes van al repositorio.
#
#   python tools/make_ui_images.py [ruta a Halo.ttf]
#
# Necesita Pillow y la fuente instalada (por defecto la de usuario: %LOCALAPPDATA%\Microsoft\Windows\Fonts\Halo.ttf).
# Escribe src/app/ui_text.png, src/app/logo.png y src/common/ui_text_atlas.h. Para agregar un texto: súmalo a
# TEXTS, vuelve a correr el script y compila.
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.environ['LOCALAPPDATA'], r'Microsoft\Windows\Fonts\Halo.ttf')
LOGO = os.path.join(ROOT, 'docs', 'logo.png')

# clave, inglés, español
TEXTS = [
    ('connection', 'Connection', 'Conexión'),
    ('options', 'Options', 'Opciones'),
    ('admin_panel', 'Admin panel', 'Panel de admin'),
    ('radmin_ip', 'Your Radmin IP', 'Tu IP de Radmin'),
    ('nickname', 'Nickname', 'Nickname'),
    ('armor', 'Armor', 'Armadura'),
    ('skin', 'Skin', 'Skin'),
    ('copy', 'Copy', 'Copiar'),
    ('open_mcc', 'Open MCC', 'Abrir MCC'),
    ('install_mod', 'Install mod', 'Instalar mod'),
    ('move', 'Move', 'Mover'),
    ('kick', 'Kick', 'Expulsar'),
    ('p1', 'P1', 'J1'),
    ('p2', 'P2', 'J2'),
    ('p3', 'P3', 'J3'),
    ('p4', 'P4', 'J4'),
    ('join_title', 'Join a game', 'Unirse a una partida'),
    ('host_ip', "Host's Radmin IP", 'IP de Radmin del host'),
    ('your_name', 'Your name', 'Tu nombre'),
    ('armor_color', 'Armor color', 'Color de armadura'),
    ('player', 'Player', 'Jugador'),
    ('control', 'Control', 'Control'),
    ('connect', 'Connect', 'Conectar'),
]

SIZE = 64          # tamaño de la fuente al dibujar; el programa las reduce
PAD = 6
ACCENTS = {'á': 'a', 'é': 'e', 'í': 'i', 'ó': 'o', 'ú': 'u', 'ñ': 'n', 'Á': 'A', 'É': 'E', 'Í': 'I', 'Ó': 'O', 'Ú': 'U',
           'Ñ': 'N'}

font = ImageFont.truetype(FONT, SIZE)
ascent, descent = font.getmetrics()
LINE = ascent + descent


def render(text):
    """Blanco sobre transparente, alto fijo (todas las imágenes comparten la línea base). Todo en mayúsculas: las
    minúsculas de esta fuente son versalitas bajas y se pierden a tamaño de etiqueta."""
    text = text.upper()
    width = int(sum(font.getlength(ACCENTS.get(c, c)) for c in text)) + PAD * 2
    img = Image.new('L', (width, LINE + PAD * 2), 0)
    d = ImageDraw.Draw(img)
    x = PAD
    for c in text:
        base = ACCENTS.get(c, c)
        d.text((x, PAD), base, font=font, fill=255)
        w = font.getlength(base)
        if c in ACCENTS:  # la fuente no trae tildes ni eñe: se dibujan encima de la letra
            box = font.getbbox(base)
            top = PAD + box[1] - SIZE * 0.10
            cx = x + w / 2
            if base.lower() == 'n':
                d.line([(cx - w * 0.3, top), (cx - w * 0.1, top - SIZE * 0.05), (cx + w * 0.1, top),
                        (cx + w * 0.3, top - SIZE * 0.05)], fill=255, width=max(2, SIZE // 16))
            else:
                d.polygon([(cx - SIZE * 0.02, top), (cx + SIZE * 0.08, top - SIZE * 0.12),
                           (cx + SIZE * 0.13, top - SIZE * 0.09), (cx + SIZE * 0.03, top + SIZE * 0.02)], fill=255)
        x += w
    bbox = img.getbbox()
    if bbox:  # recorte horizontal, alto fijo
        img = img.crop((max(0, bbox[0] - 2), 0, min(img.width, bbox[2] + 2), img.height))
    return img


images = []
for key, en, es in TEXTS:
    images.append((key, 'en', render(en)))
    images.append((key, 'es', render(es)))

# Empaquetado simple por filas (el ancho alcanza para el texto más largo).
ATLAS_W = max(2048, max(img.width for _, _, img in images))
x = y = row = 0
placed = []
for key, lang, img in images:
    if x + img.width > ATLAS_W:
        x, y = 0, y + row + 2
        row = 0
    placed.append((key, lang, x, y, img))
    x += img.width + 2
    row = max(row, img.height)
atlas_h = y + row
atlas = Image.new('RGBA', (ATLAS_W, atlas_h), (255, 255, 255, 0))
for key, lang, px, py, img in placed:
    white = Image.new('RGBA', img.size, (255, 255, 255, 255))
    white.putalpha(img)
    atlas.paste(white, (px, py))
atlas.save(os.path.join(ROOT, 'src', 'app', 'ui_text.png'))

# Logo: blanco con su transparencia (el programa le da el color).
logo = Image.open(LOGO).convert('RGBA')
logo = logo.resize((1200, round(logo.height * 1200 / logo.width)), Image.LANCZOS)
white = Image.new('RGBA', logo.size, (255, 255, 255, 255))
white.putalpha(logo.getchannel('A'))
white.save(os.path.join(ROOT, 'src', 'app', 'logo.png'))

rects = {}
for key, lang, px, py, img in placed:
    rects.setdefault(key, {})[lang] = (px, py, img.width, img.height)
with open(os.path.join(ROOT, 'src', 'common', 'ui_text_atlas.h'), 'w', encoding='utf-8', newline='\n') as f:
    f.write('#pragma once\n// Generado por tools/make_ui_images.py: no editar a mano.\n')
    f.write('// Textos fijos dibujados con la fuente de Halo (src/app/ui_text.png): x, y, ancho, alto en inglés y en español.\n\n')
    f.write('struct UiTextRect {\n    int x, y, w, h;\n};\n\nstruct UiTextEntry {\n    const char* key;\n    UiTextRect en, es;\n};\n\n')
    f.write('inline constexpr int k_ui_text_atlas_w = %d, k_ui_text_atlas_h = %d;\n' % (ATLAS_W, atlas_h))
    f.write('inline constexpr int k_ui_logo_w = %d, k_ui_logo_h = %d;\n\n' % (white.width, white.height))
    f.write('inline constexpr UiTextEntry k_ui_texts[] = {\n')
    for key, en, es in TEXTS:
        e, s = rects[key]['en'], rects[key]['es']
        f.write('    {"%s", {%d, %d, %d, %d}, {%d, %d, %d, %d}},\n' % ((key,) + e + s))
    f.write('};\n')
print('ok: %d textos, atlas %dx%d' % (len(TEXTS), ATLAS_W, atlas_h))
