"""Erzeugt Inventar-Icons (256/64) und alle FPV-OSD/HUD-Grafiken.

Ausgabe: Assets/Export/Icons/*.png und Assets/Export/UI/OSD/*.png
OSD-Grafiken sind weiss mit schwarzer Kontur (Betaflight-Stil), damit sie in UMG
per Tint eingefaerbt werden koennen.
"""
import os
import shutil

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ICONS = os.path.join(ROOT, "Export", "Icons")
OSD = os.path.join(ROOT, "Export", "UI", "OSD")
FONTS = os.path.join(ROOT, "Export", "UI", "Fonts")
FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"

WHITE = (255, 255, 255, 255)
SS = 4  # Supersampling fuer saubere Kanten


def canvas(w, h):
    return Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))


def outline_and_save(img, name, outline=2):
    """Skaliert herunter und legt eine schwarze Kontur unter die weisse Form."""
    w, h = img.size[0] // SS, img.size[1] // SS
    img = img.resize((w, h), Image.LANCZOS)
    a = img.split()[3]
    grown = a.filter(ImageFilter.MaxFilter(outline * 2 + 1))
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    out.paste((0, 0, 0, 200), (0, 0), grown)
    out.alpha_composite(img)
    out.save(os.path.join(OSD, name), optimize=True)
    print("OSD:", name)


def font(px):
    return ImageFont.truetype(FONT_PATH, px * SS)


# --------------------------------------------------------------------------- Icons

def icons():
    for base in ("IconFPVDrone", "IconFPVRemote", "IconFPVGoggles", "IconFPVProp"):
        src = os.path.join(ICONS, base + "_512.png")
        if not os.path.exists(src):
            print("fehlt (zuerst Blender-Skript ausfuehren):", src)
            continue
        img = Image.open(src)
        bbox = img.split()[3].getbbox()
        # Motiv mittig, mit 6 % Rand auf das Quadrat ziehen
        crop = img.crop(bbox)
        side = int(max(crop.size) * 1.12)
        sq = Image.new("RGBA", (side, side), (0, 0, 0, 0))
        sq.paste(crop, ((side - crop.size[0]) // 2, (side - crop.size[1]) // 2))
        for size in (256, 64):
            sq.resize((size, size), Image.LANCZOS).save(
                os.path.join(ICONS, "%s_%d.png" % (base, size)), optimize=True)
        print("Icon:", base)


# --------------------------------------------------------------------------- OSD-Elemente

def crosshair():
    s = 64
    im = canvas(s, s)
    d = ImageDraw.Draw(im)
    c, lw = s * SS // 2, 2 * SS
    gap, arm = 6 * SS, 18 * SS
    for dx, dy in ((1, 0), (-1, 0), (0, 1)):
        d.line([(c + dx * gap, c + dy * gap), (c + dx * arm, c + dy * arm)], fill=WHITE, width=lw)
    d.ellipse([c - 2 * SS, c - 2 * SS, c + 2 * SS, c + 2 * SS], fill=WHITE)
    outline_and_save(im, "T_OSD_Crosshair.png")


def horizon():
    w, h = 512, 32
    im = canvas(w, h)
    d = ImageDraw.Draw(im)
    y, lw = h * SS // 2, 3 * SS
    d.line([(8 * SS, y), (200 * SS, y)], fill=WHITE, width=lw)
    d.line([(312 * SS, y), (504 * SS, y)], fill=WHITE, width=lw)
    for x in (8, 504):
        d.line([(x * SS, y), (x * SS, y + 8 * SS)], fill=WHITE, width=lw)
    outline_and_save(im, "T_OSD_Horizon.png")

    # Seitliche Begrenzungsklammern des kuenstlichen Horizonts
    im = canvas(32, 256)
    d = ImageDraw.Draw(im)
    for i in range(9):
        y = (16 + i * 28) * SS
        ln = 14 if i == 4 else 8
        d.line([(4 * SS, y), ((4 + ln) * SS, y)], fill=WHITE, width=2 * SS)
    outline_and_save(im, "T_OSD_HorizonSidebar.png")


def battery():
    w, h = 96, 48
    im = canvas(w, h)
    d = ImageDraw.Draw(im)
    lw = 3 * SS
    d.rounded_rectangle([4 * SS, 6 * SS, 82 * SS, 42 * SS], radius=5 * SS, outline=WHITE, width=lw)
    d.rounded_rectangle([82 * SS, 16 * SS, 90 * SS, 32 * SS], radius=2 * SS, fill=WHITE)
    outline_and_save(im, "T_OSD_Battery_Frame.png")
    # Fuellung als eigene Textur fuer UMG-Progressbar (horizontal skaliert)
    im = canvas(72, 28)
    ImageDraw.Draw(im).rounded_rectangle([0, 0, 72 * SS - 1, 28 * SS - 1], radius=3 * SS, fill=WHITE)
    im.resize((72, 28), Image.LANCZOS).save(os.path.join(OSD, "T_OSD_Battery_Fill.png"))
    print("OSD: T_OSD_Battery_Fill.png")


def rssi():
    s = 64
    im = canvas(s, s)
    d = ImageDraw.Draw(im)
    for i in range(4):
        x0 = (8 + i * 13) * SS
        top = (48 - i * 11 - 6) * SS
        d.rectangle([x0, top, x0 + 9 * SS, 56 * SS], fill=WHITE)
    outline_and_save(im, "T_OSD_RSSI.png")
    # Antennensymbol fuer Link Quality
    im = canvas(s, s)
    d = ImageDraw.Draw(im)
    c = 32 * SS
    d.line([(c, 24 * SS), (c, 58 * SS)], fill=WHITE, width=4 * SS)
    for r in (10, 18, 26):
        d.arc([c - r * SS, 24 * SS - r * SS, c + r * SS, 24 * SS + r * SS], 200, 340, fill=WHITE, width=3 * SS)
    outline_and_save(im, "T_OSD_LinkQuality.png")


def warning():
    s = 64
    im = canvas(s, s)
    d = ImageDraw.Draw(im)
    pts = [(32 * SS, 4 * SS), (60 * SS, 58 * SS), (4 * SS, 58 * SS)]
    d.polygon(pts, outline=WHITE, width=4 * SS)
    d.line([(32 * SS, 22 * SS), (32 * SS, 42 * SS)], fill=WHITE, width=5 * SS)
    d.ellipse([29 * SS, 47 * SS, 35 * SS, 53 * SS], fill=WHITE)
    outline_and_save(im, "T_OSD_Warning.png")


def home_arrow():
    s = 64
    im = canvas(s, s)
    d = ImageDraw.Draw(im)
    d.polygon([(32 * SS, 4 * SS), (56 * SS, 58 * SS), (32 * SS, 44 * SS), (8 * SS, 58 * SS)], fill=WHITE)
    outline_and_save(im, "T_OSD_HomeArrow.png")


def badge(text, name, filled=False):
    f = font(26)
    tw = int(f.getlength(text) / SS)
    w, h = tw + 28, 40
    im = canvas(w, h)
    d = ImageDraw.Draw(im)
    box = [2 * SS, 2 * SS, (w - 2) * SS, (h - 2) * SS]
    if filled:
        d.rounded_rectangle(box, radius=6 * SS, fill=WHITE)
        # Text ausstanzen
        mask = canvas(w, h)
        ImageDraw.Draw(mask).text((w * SS // 2, h * SS // 2 + SS), text, font=f, fill=WHITE, anchor="mm")
        a = np.array(im.split()[3]).astype(int) - np.array(mask.split()[3]).astype(int)
        im.putalpha(Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)))
    else:
        d.rounded_rectangle(box, radius=6 * SS, outline=WHITE, width=3 * SS)
        d.text((w * SS // 2, h * SS // 2 + SS), text, font=f, fill=WHITE, anchor="mm")
    outline_and_save(im, name)


def badges():
    modes = {"ACRO": "Acro", "ANGLE": "Angle", "HORIZON": "Horizon", "3D": "3D", "AIR": "AirMode",
             "TURTLE": "Turtle", "RESCUE": "Rescue"}
    for text, key in modes.items():
        badge(text, "T_OSD_Mode_%s.png" % key)
    for text, key in {"ARMED": "Armed", "DISARMED": "Disarmed", "FAILSAFE": "Failsafe",
                      "LOW BAT": "LowBattery", "THROTTLE": "Throttle", "NO SIGNAL": "NoSignal"}.items():
        badge(text, "T_OSD_Status_%s.png" % key, filled=key in ("Failsafe", "LowBattery", "NoSignal"))


def sticks():
    """Stick-Anzeige fuer Kalibrierung und Overlay: Rahmen + Punkt."""
    s = 128
    im = canvas(s, s)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([4 * SS, 4 * SS, 124 * SS, 124 * SS], radius=10 * SS, outline=WHITE, width=3 * SS)
    c = 64 * SS
    d.line([(c, 14 * SS), (c, 114 * SS)], fill=(255, 255, 255, 110), width=SS)
    d.line([(14 * SS, c), (114 * SS, c)], fill=(255, 255, 255, 110), width=SS)
    outline_and_save(im, "T_OSD_StickBox.png")
    im = canvas(24, 24)
    ImageDraw.Draw(im).ellipse([3 * SS, 3 * SS, 21 * SS, 21 * SS], fill=WHITE)
    outline_and_save(im, "T_OSD_StickDot.png")


def osd_font_atlas():
    """Zeichensatz-Atlas 16x6 (ASCII 32..126 + Gradzeichen) fuer ein klassisches MAX7456-OSD-Gefuehl."""
    cw, ch = 24, 36
    im = canvas(cw * 16, ch * 6)
    d = ImageDraw.Draw(im)
    f = font(28)
    chars = [chr(c) for c in range(32, 127)] + ["\u00b0"]  # letztes Feld: Gradzeichen
    for i, c in enumerate(chars):
        x, y = (i % 16) * cw + cw // 2, (i // 16) * ch + ch // 2
        d.text((x * SS, y * SS), c, font=f, fill=WHITE, anchor="mm")
    outline_and_save(im, "T_OSD_FontAtlas.png", outline=2)


# --------------------------------------------------------------------------- Vollbild-Overlays

def vignette():
    w, h = 1920, 1080
    y, x = np.mgrid[0:h, 0:w]
    nx, ny = (x / w - 0.5) * 2, (y / h - 0.5) * 2
    r = np.sqrt((nx * 0.9) ** 2 + (ny * 1.1) ** 2)
    a = np.clip((r - 0.75) / 0.6, 0, 1) ** 1.6
    rgba = np.zeros((h, w, 4), np.uint8)
    rgba[..., 3] = (a * 235).astype(np.uint8)
    Image.fromarray(rgba, "RGBA").save(os.path.join(OSD, "T_OSD_GogglesVignette.png"), optimize=True)
    print("OSD: T_OSD_GogglesVignette.png")


def static_noise():
    """Analoges Bildrauschen fuer schwaches Signal: 4 Frames in einem 2x2-Flipbook (Graustufe)."""
    rng = np.random.default_rng(7)
    fw, fh = 480, 270
    atlas = np.zeros((fh * 2, fw * 2), np.float32)
    for i in range(4):
        n = rng.random((fh, fw))
        # horizontale Streifen und Zeilenversatz wie bei analogem 5.8-GHz-Video
        rows = rng.random((fh, 1)) ** 6
        n = n * 0.7 + rows * 0.6
        band = np.exp(-((np.arange(fh)[:, None] - rng.integers(0, fh)) / 12.0) ** 2) * 0.5
        n = np.clip(n + band, 0, 1)
        atlas[(i // 2) * fh:(i // 2 + 1) * fh, (i % 2) * fw:(i % 2 + 1) * fw] = n
    Image.fromarray((atlas * 255).astype(np.uint8), "L").save(os.path.join(OSD, "T_OSD_StaticNoise.png"),
                                                              optimize=True)
    print("OSD: T_OSD_StaticNoise.png")


def scanlines():
    h = 64
    a = np.zeros((h, 4), np.uint8)
    a[::2] = 40
    rgba = np.zeros((h, 4, 4), np.uint8)
    rgba[..., 3] = a
    Image.fromarray(rgba, "RGBA").save(os.path.join(OSD, "T_OSD_Scanlines.png"))
    print("OSD: T_OSD_Scanlines.png")


if __name__ == "__main__":
    os.makedirs(OSD, exist_ok=True)
    os.makedirs(FONTS, exist_ok=True)
    icons()
    crosshair()
    horizon()
    battery()
    rssi()
    warning()
    home_arrow()
    badges()
    sticks()
    osd_font_atlas()
    vignette()
    static_noise()
    scanlines()
    shutil.copy(FONT_PATH, os.path.join(FONTS, "DejaVuSansMono-Bold.ttf"))
    print("Font kopiert: DejaVuSansMono-Bold.ttf")
