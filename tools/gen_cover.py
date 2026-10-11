"""
Картинки для Nexus в стиле экрана Пип-боя (зелёный на тёмном, сетка, строки развёртки), как у No Safe Level.
Мотив: карта с игроком и несколькими целями — ближайшая подсвечена, к ней путь через дверь (расстояние — по маршруту
маркера, как считает мод); справа — список целей, как в окне мода (квест, новый квест, локация, интерьер).

    images/cover.png   1920x1080 — картинка галереи / миниатюра
    images/banner.png  1300x372  — шапка страницы (Nexus режет по центру — всё важное в середине по высоте)

    python tools/gen_cover.py
"""

import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG = os.path.join(ROOT, 'images')
FONT = r'C:\Windows\Fonts\bahnschrift.ttf'

GREEN = (26, 255, 110)
GREEN_MID = (20, 190, 85)
GREEN_DIM = (14, 120, 60)
AMBER = (255, 190, 60)
WHITE = (225, 240, 228)
DARK = (6, 14, 10)

TITLE = 'NEAREST QUEST TRACKER'
SUBTITLE = 'Always know where to go next'

# Список «как в окне мода»: (отмечено, текст, расстояние, вид).
ROWS = [(True, 'Nearest objective', '96 m', 'quest'),
        (False, 'New quest nearby', '315 m', 'start'),
        (False, 'Undiscovered location', '420 m', 'location'),
        (False, 'Unvisited interior', '560 m', 'interior'),
        (False, 'Miscellaneous', '1.2 km', 'quest'),
        (False, 'Far Harbor', 'other world', 'quest')]


def font(size, weight='Bold'):
    f = ImageFont.truetype(FONT, size)
    try:
        f.set_variation_by_name(weight)
    except (OSError, ValueError):
        pass
    return f


def background(w, h, seed=7):
    """Тёмный экран: радиальная подсветка, сетка, строки развёртки, лёгкий шум, виньетка."""
    img = Image.new('RGB', (w, h), DARK)
    glow = Image.new('L', (w, h), 0)
    ImageDraw.Draw(glow).ellipse((-w * 0.2, -h * 0.35, w * 1.2, h * 1.35), fill=70)
    glow = glow.filter(ImageFilter.GaussianBlur(min(w, h) // 3))
    img = Image.composite(Image.new('RGB', (w, h), (18, 60, 34)), img, glow)
    d = ImageDraw.Draw(img, 'RGBA')
    step = max(24, w // 40)
    for x in range(0, w, step):
        d.line((x, 0, x, h), fill=GREEN + (14,), width=1)
    for y in range(0, h, step):
        d.line((0, y, w, y), fill=GREEN + (14,), width=1)
    rnd = random.Random(seed)
    for _ in range(w * h // 400):
        x, y = rnd.randrange(w), rnd.randrange(h)
        d.point((x, y), fill=GREEN + (rnd.randrange(10, 40),))
    vignette = Image.new('L', (w, h), 0)
    ImageDraw.Draw(vignette).ellipse((-w * 0.15, -h * 0.25, w * 1.15, h * 1.25), fill=255)
    vignette = vignette.filter(ImageFilter.GaussianBlur(min(w, h) // 5))
    return Image.composite(img, Image.new('RGB', (w, h), (0, 0, 0)), vignette)


def scanlines(img):
    d = ImageDraw.Draw(img, 'RGBA')
    for y in range(0, img.size[1], 3):
        d.line((0, y, img.size[0], y), fill=(0, 0, 0, 38), width=1)


def glow_text(img, xy, text, f, fill, anchor='mm', radius=14, strength=2):
    """Текст со свечением, как на ЭЛТ-экране."""
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).text(xy, text, font=f, fill=fill + (255,), anchor=anchor)
    halo = layer.filter(ImageFilter.GaussianBlur(radius))
    for _ in range(strength):
        img.paste(halo, (0, 0), halo)
    img.paste(layer, (0, 0), layer)


def glow_layer(img, draw_fn, radius):
    """Рисует draw_fn(ImageDraw) на отдельном слое и кладёт его со свечением."""
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(layer))
    halo = layer.filter(ImageFilter.GaussianBlur(radius))
    img.paste(halo, (0, 0), halo)
    img.paste(layer, (0, 0), layer)


def dashed(d, points, color, width, dash, gap):
    """Пунктир по ломаной."""
    for (x1, y1), (x2, y2) in zip(points, points[1:]):
        length = math.hypot(x2 - x1, y2 - y1)
        pos = 0.0
        while pos < length:
            end = min(length, pos + dash)
            d.line((x1 + (x2 - x1) * pos / length, y1 + (y2 - y1) * pos / length,
                    x1 + (x2 - x1) * end / length, y1 + (y2 - y1) * end / length), fill=color, width=width)
            pos = end + gap


def quest_marker(d, x, y, s, color, filled):
    """Маркер цели: перевёрнутый треугольник-указатель с точкой (как метка на карте Пип-боя)."""
    pts = [(x - s, y - s * 1.5), (x + s, y - s * 1.5), (x, y)]
    if filled:
        d.polygon(pts, fill=color)
    else:
        d.polygon(pts, outline=color, width=max(2, s // 5))
    d.ellipse((x - s * 0.28, y - s * 1.3 - s * 0.28 + s * 0.35, x + s * 0.28, y - s * 1.3 + s * 0.28 + s * 0.35),
              fill=DARK if filled else color)


def location_marker(d, x, y, s, color):
    """Неоткрытая локация: пустой кружок со знаком вопроса."""
    d.ellipse((x - s, y - s, x + s, y + s), outline=color, width=max(2, s // 5))
    d.text((x, y + 1), '?', font=font(int(s * 1.4)), fill=color, anchor='mm')


def door_marker(d, x, y, s, color):
    """Дверь (интерьер): прямоугольник с ручкой."""
    d.rectangle((x - s * 0.6, y - s, x + s * 0.6, y + s), outline=color, width=max(2, s // 5))
    d.ellipse((x + s * 0.2, y - s * 0.1, x + s * 0.4, y + s * 0.1), fill=color)


def player_arrow(d, x, y, s, angle, color):
    a = math.radians(angle)
    tip = (x + math.cos(a) * s, y + math.sin(a) * s)
    left = (x + math.cos(a + 2.5) * s * 0.75, y + math.sin(a + 2.5) * s * 0.75)
    right = (x + math.cos(a - 2.5) * s * 0.75, y + math.sin(a - 2.5) * s * 0.75)
    d.polygon([tip, left, (x, y), right], fill=color)


def map_area(img, box, scale=1.0):
    """Карта: дороги, река, кварталы, игрок, цели; к ближайшей — путь через дверь."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    d = ImageDraw.Draw(img, 'RGBA')
    d.rectangle(box, outline=GREEN + (120,), width=2)
    rnd = random.Random(3)
    # кварталы
    for _ in range(26):
        bw, bh = rnd.randint(int(w * 0.04), int(w * 0.11)), rnd.randint(int(h * 0.05), int(h * 0.13))
        bx, by = rnd.randint(x0 + 10, x1 - bw - 10), rnd.randint(y0 + 10, y1 - bh - 10)
        d.rectangle((bx, by, bx + bw, by + bh), outline=GREEN_DIM + (110,), width=2)
    # река и дороги
    river = [(x0 + w * t, y0 + h * (0.82 - 0.25 * math.sin(t * 3.1))) for t in [i / 30 for i in range(31)]]
    d.line(river, fill=GREEN_DIM + (150,), width=int(10 * scale), joint='curve')
    for pts in ([(x0, y0 + h * 0.35), (x0 + w * 0.45, y0 + h * 0.42), (x1, y0 + h * 0.3)],
                [(x0 + w * 0.3, y0), (x0 + w * 0.36, y0 + h * 0.6), (x0 + w * 0.28, y1)],
                [(x0 + w * 0.62, y0), (x0 + w * 0.7, y1)]):
        d.line(pts, fill=GREEN_DIM + (170,), width=int(4 * scale))

    def at(fx, fy):
        return x0 + w * fx, y0 + h * fy

    px, py = at(0.22, 0.55)
    nearest = at(0.48, 0.24)
    door = at(0.38, 0.47)
    s = int(16 * scale)
    # остальные цели — тусклые, с расстояниями
    others = [(at(0.80, 0.68), '1.2 km', 'quest'), (at(0.66, 0.16), '315 m', 'start'),
              (at(0.12, 0.20), '420 m', 'location'), (at(0.58, 0.80), '560 m', 'interior'),
              (at(0.82, 0.30), '2.4 km', 'quest')]
    small = font(int(26 * scale), 'SemiBold')
    for (mx, my), label, kind in others:
        if kind == 'location':
            location_marker(d, mx, my - s, int(s * 0.9), GREEN_MID + (220,))
        elif kind == 'interior':
            door_marker(d, mx, my - s, int(s * 0.9), GREEN_MID + (220,))
        else:
            quest_marker(d, mx, my, s, GREEN_MID + (220,), kind == 'quest')
        d.line((px, py, mx, my), fill=GREEN_DIM + (60,), width=1)
        d.text((mx + s * 1.4, my - s), label, font=small, fill=GREEN_MID + (230,), anchor='lm')

    def bright(dd):
        dashed(dd, [(px, py), door, nearest], AMBER + (255,), int(5 * scale), int(18 * scale), int(10 * scale))
        door_marker(dd, door[0], door[1], int(s * 0.8), AMBER + (255,))
        quest_marker(dd, nearest[0], nearest[1], int(s * 1.6), AMBER + (255,), True)
        player_arrow(dd, px, py, int(26 * scale), -50, GREEN + (255,))
    glow_layer(img, bright, int(10 * scale))
    d = ImageDraw.Draw(img, 'RGBA')
    d.text((nearest[0] + s * 2.2, nearest[1] - s * 1.6), '96 m', font=font(int(40 * scale)), fill=AMBER + (255,),
           anchor='lm')
    d.text((nearest[0] + s * 2.2, nearest[1] - s * 1.6 + 40 * scale), 'NEAREST', font=font(int(24 * scale), 'SemiBold'),
           fill=AMBER + (230,), anchor='lm')


def list_panel(img, box, row_h, size):
    """Список целей как в окне мода: отмеченная строка — «»», с подсветкой и рамкой."""
    x0, y0, x1, y1 = box
    d = ImageDraw.Draw(img, 'RGBA')
    d.rectangle(box, fill=(0, 0, 0, 120), outline=GREEN + (200,), width=2)
    tab_w = (x1 - x0 - 30) // 2
    for i, name in enumerate(('QUESTS', 'LOCATIONS')):
        tx = x0 + 10 + i * (tab_w + 10)
        d.rectangle((tx, y0 + 10, tx + tab_w, y0 + 10 + row_h), fill=GREEN + (70 if i == 0 else 15,),
                    outline=GREEN + (220,), width=2 if i == 0 else 1)
        d.text((tx + tab_w // 2, y0 + 10 + row_h // 2), name, font=font(size, 'SemiBold'), fill=GREEN + (255,),
               anchor='mm')
    y = y0 + 30 + row_h
    f = font(size, 'SemiBold')
    for marked, text, dist, kind in ROWS:
        if y + row_h > y1 - 8:
            break
        if marked:
            d.rectangle((x0 + 8, y, x1 - 8, y + row_h - 4), fill=GREEN + (55,), outline=GREEN + (255,), width=2)
        color = AMBER if marked else GREEN
        d.text((x0 + 22, y + row_h // 2), ('» ' if marked else '') + text, font=f, fill=color + (255,), anchor='lm')
        d.text((x1 - 22, y + row_h // 2), dist, font=f, fill=color + (230,), anchor='rm')
        y += row_h


def cover():
    w, h = 1920, 1080
    img = background(w, h)
    glow_text(img, (w // 2, 150), TITLE, font(128), GREEN, radius=18)
    d = ImageDraw.Draw(img, 'RGBA')
    d.text((w // 2, 262), SUBTITLE, font=font(52, 'SemiBold'), fill=WHITE + (255,), anchor='mm')
    d.line((w // 2 - 560, 318, w // 2 + 560, 318), fill=GREEN + (160,), width=2)
    map_area(img, (150, 370, 1080, 860), 1.15)
    list_panel(img, (1140, 370, 1770, 860), 62, 32)
    d = ImageDraw.Draw(img, 'RGBA')
    d.text((w // 2, 935), 'One key: nearest quest, new quest, location or interior   |   Distance through doors',
           font=font(38, 'SemiBold'), fill=GREEN + (235,), anchor='mm')
    d.text((w // 2, 995), 'Quest window with wiki categories   |   Mouse, keyboard, gamepad   |   F4SE + MCM',
           font=font(34, 'Regular'), fill=WHITE + (210,), anchor='mm')
    scanlines(img)
    img.save(os.path.join(IMG, 'cover.png'))


def banner():
    w, h = 1300, 372
    img = background(w, h, seed=11)
    glow_text(img, (w // 2, 120), TITLE, font(92), GREEN, radius=12)
    d = ImageDraw.Draw(img, 'RGBA')
    d.text((w // 2, 196), SUBTITLE, font=font(36, 'SemiBold'), fill=WHITE + (255,), anchor='mm')
    # ряд маркеров: игрок -> пунктир -> ближайший (янтарный), дальше тусклые цели. Справа: левый нижний угол
    # шапки Nexus закрывает названием мода (No Safe Level, 2026-09-30).
    y = 280

    def bright(dd):
        player_arrow(dd, 600, y, 26, 0, GREEN + (255,))
        dashed(dd, [(635, y), (800, y)], AMBER + (255,), 5, 18, 10)
        quest_marker(dd, 830, y + 16, 24, AMBER + (255,), True)
    glow_layer(img, bright, 8)
    d = ImageDraw.Draw(img, 'RGBA')
    d.text((870, y), '96 m', font=font(34), fill=AMBER + (255,), anchor='lm')
    quest_marker(d, 1000, y + 12, 15, GREEN_MID + (220,), False)
    d.text((1025, y), '315 m', font=font(26, 'SemiBold'), fill=GREEN_MID + (230,), anchor='lm')
    location_marker(d, 1130, y, 14, GREEN_MID + (220,))
    d.text((1155, y), '420 m', font=font(26, 'SemiBold'), fill=GREEN_MID + (230,), anchor='lm')
    scanlines(img)
    img.save(os.path.join(IMG, 'banner.png'))


if __name__ == '__main__':
    os.makedirs(IMG, exist_ok=True)
    cover()
    banner()
    print('images/cover.png, images/banner.png')
