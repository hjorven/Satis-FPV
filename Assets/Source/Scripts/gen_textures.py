"""Erzeugt die kachelbaren Material-Texturen fuer die FPV-Drohne.

Ausgabe: Assets/Export/Textures/*.png
Normal Maps sind im DirectX-Format (gruen invertiert), wie Unreal sie erwartet.
ORM = R: Ambient Occlusion, G: Roughness, B: Metallic.
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Export", "Textures")
SIZE = 1024
rng = np.random.default_rng(1337)


def tile_noise(size, scale, seed):
    """Kachelbare Rauschtextur ueber gefiltertes FFT-Rauschen (0..1)."""
    r = np.random.default_rng(seed)
    white = r.standard_normal((size, size))
    f = np.fft.fftfreq(size)
    fx, fy = np.meshgrid(f, f)
    rad = np.sqrt(fx ** 2 + fy ** 2)
    spec = np.exp(-(rad * size / scale) ** 2)
    n = np.real(np.fft.ifft2(np.fft.fft2(white) * spec))
    n -= n.min()
    return n / n.max()


def save_rgb(arr, name):
    img = (np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8)
    mode = "RGBA" if img.shape[-1] == 4 else "RGB"
    Image.fromarray(img, mode).save(os.path.join(OUT, name), optimize=True)
    print("geschrieben:", name)


def normal_from_height(h, strength):
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * strength
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * strength
    n = np.stack([-dx, dy, np.ones_like(h)], axis=-1)  # DirectX: +Y zeigt nach unten
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5


def carbon():
    """2x2-Koeper-Carbon, 16 Rovings pro Kachel."""
    tows = 16
    y, x = np.mgrid[0:SIZE, 0:SIZE] / SIZE * tows
    ix, iy = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = x - ix, y - iy
    horizontal = ((ix + iy) // 2) % 2 == 0  # Koeperbindung 2/2
    along = np.where(horizontal, fx, fy)
    across = np.where(horizontal, fy, fx)
    # Wolbung jedes Rovings und Faserstreifen
    bulge = np.sin(np.pi * across) ** 0.5 * (0.75 + 0.25 * np.sin(np.pi * along))
    # Faserstreifen laufen entlang des Rovings: 1D-Rauschen quer zur Faser
    lines = np.random.default_rng(2).random(SIZE)
    lines = np.convolve(np.tile(lines, 3), np.ones(3) / 3, "same")[SIZE:2 * SIZE]
    streak = np.where(horizontal, lines[:, None], lines[None, :])
    height = bulge * 0.85 + streak * 0.15
    sheen = np.where(horizontal, 0.055, 0.035)  # Anisotropie-Imitat
    base = 0.018 + sheen * bulge + 0.02 * streak
    bc = np.stack([base, base, base * 1.08], axis=-1)
    save_rgb(bc, "T_FPV_Carbon_BC.png")
    save_rgb(normal_from_height(height, 6.0), "T_FPV_Carbon_N.png")
    ao = 0.75 + 0.25 * bulge
    rough = 0.28 + 0.12 * (1 - bulge) + 0.05 * tile_noise(SIZE, 40, 3)
    save_rgb(np.stack([ao, rough, np.zeros_like(ao)], -1), "T_FPV_Carbon_ORM.png")


def paint():
    """FICSIT-Orange Lack mit leichter Abnutzung."""
    big = tile_noise(SIZE, 12, 10)
    mid = tile_noise(SIZE, 60, 11)
    fine = tile_noise(SIZE, 300, 12)
    orange = np.array([0.98, 0.56, 0.16])
    dirt = np.array([0.35, 0.24, 0.15])
    grime = np.clip((big * 0.6 + mid * 0.4 - 0.68) * 3, 0, 1)[..., None]
    bc = orange * (0.92 + 0.08 * fine[..., None]) * (1 - grime * 0.5) + dirt * grime * 0.5
    save_rgb(bc, "T_FPV_Paint_BC.png")
    h = fine * 0.3 + mid * 0.7
    save_rgb(normal_from_height(h, 1.5), "T_FPV_Paint_N.png")
    rough = 0.38 + 0.25 * grime[..., 0] + 0.08 * fine
    ao = np.ones_like(rough) * (0.95 + 0.05 * mid)
    save_rgb(np.stack([ao, rough, np.zeros_like(ao)], -1), "T_FPV_Paint_ORM.png")


def anodized():
    """Gebuerstetes, eloxiertes Aluminium fuer Motorglocken (Graustufe, Farbe per Material)."""
    streaks = tile_noise(SIZE, 600, 20)
    streaks = np.repeat(streaks.mean(axis=1, keepdims=True), SIZE, axis=1) * 0.5 + streaks * 0.5
    bc = 0.75 + 0.25 * streaks
    save_rgb(np.stack([bc] * 3, -1), "T_FPV_Brushed_BC.png")
    rough = 0.22 + 0.15 * streaks
    save_rgb(np.stack([np.ones_like(bc), rough, np.ones_like(bc)], -1), "T_FPV_Brushed_ORM.png")


def prop_blur():
    """Radialer Bewegungsunschaerfe-Ring fuer schnell drehende Propeller (RGBA, 512)."""
    s = 512
    y, x = (np.mgrid[0:s, 0:s] + 0.5) / s * 2 - 1
    r = np.sqrt(x ** 2 + y ** 2)
    ang = np.arctan2(y, x)
    hub = 0.12
    disc = np.clip((r - hub) / 0.05, 0, 1) * np.clip((1 - r) / 0.03, 0, 1)
    tipband = np.exp(-((r - 0.9) / 0.05) ** 2) * 0.5
    density = (1 - (r - hub) / (1 - hub)) * 0.35 + 0.15 + tipband
    flicker = 0.85 + 0.15 * np.cos(ang * 3)  # leichte Dreiblatt-Struktur
    alpha = np.clip(disc * density * flicker, 0, 1)
    rgb = np.stack([np.full_like(r, 0.12)] * 3, -1)
    save_rgb(np.concatenate([rgb, alpha[..., None]], -1), "T_FPV_PropBlur.png")


def led():
    """Maske fuer den LED-Streifen: 8 Segmente in einer Reihe (Graustufe)."""
    w, h = 512, 64
    x = (np.arange(w) + 0.5) / w * 8
    seg = np.abs((x % 1) - 0.5)
    m = np.clip((0.35 - seg) * 8, 0, 1)
    yy = np.abs((np.arange(h) + 0.5) / h - 0.5)
    m = m[None, :] * np.clip((0.4 - yy) * 8, 0, 1)[:, None]
    save_rgb(np.stack([m] * 3, -1), "T_FPV_LEDMask.png")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    carbon()
    paint()
    anodized()
    prop_blur()
    led()
