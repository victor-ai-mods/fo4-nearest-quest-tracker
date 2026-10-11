"""Шапка страницы Nexus из иллюстрированной обложки (как в No Safe Level).

    images/cover_chatgpt.png  (картинка из ChatGPT, уже с заголовком)  ->  images/banner_chatgpt.png  1300x372

Из обложки берётся полоса сцены под её подзаголовком (голова и Пип-бой Выжившего, янтарный маркер над дверью,
начало пунктира, город с маркерами); заголовок и подзаголовок перерисовываются поверх, правее, над городом:
по центру они легли бы на голову. Левый нижний угол шапки Nexus закрывает названием мода — там только плечо.

    python tools/gen_banner.py
"""

import os

from PIL import Image, ImageDraw

from gen_cover import GREEN, WHITE, font, glow_text

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG = os.path.join(ROOT, 'images')

W, H = 1300, 372
CROP_TOP = 200        # верх полосы в пикселях обложки: сразу под её подзаголовком
SHADE_H = 140         # высота затемнения сверху, под заголовком
TITLE_X = 840         # центр заголовка: правее головы персонажа


def shade(img, h, alpha):
    """Затемнение сверху вниз с мягким краем — под заголовком."""
    mask = Image.new('L', img.size, 0)
    d = ImageDraw.Draw(mask)
    for y in range(h):
        d.line((0, y, img.width, y), fill=int(alpha * (1 - y / h) ** 1.5))
    img.paste(Image.new('RGB', img.size, (0, 0, 0)), (0, 0), mask)


def main():
    src = Image.open(os.path.join(IMG, 'cover_chatgpt.png')).convert('RGB')
    scaled = src.resize((W, round(src.height * W / src.width)), Image.LANCZOS)
    top = round(CROP_TOP * W / src.width)
    img = scaled.crop((0, top, W, top + H))

    shade(img, SHADE_H, 220)
    glow_text(img, (TITLE_X, 46), 'NEAREST QUEST TRACKER', font(72, 'Bold Condensed'), GREEN, radius=10)
    d = ImageDraw.Draw(img, 'RGBA')
    d.text((TITLE_X, 100), 'Always know where to go next', font=font(28, 'SemiBold Condensed'),
           fill=WHITE + (255,), anchor='mm')

    img.save(os.path.join(IMG, 'banner_chatgpt.png'))
    print('images/banner_chatgpt.png')


if __name__ == '__main__':
    main()
