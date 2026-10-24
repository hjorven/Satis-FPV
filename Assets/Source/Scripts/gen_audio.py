"""Synthetisiert die Sounds der FPV-Drohne (44.1 kHz, 16 Bit, mono WAV).

Loops sind nahtlos: alle Toene haben eine ganze Zahl an Perioden pro Loop,
Rauschen wird periodisch im Frequenzraum erzeugt.
"""
import os
import wave

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Export", "Audio")
SR = 44100
rng = np.random.default_rng(42)


def write(name, x, peak=0.89):
    x = np.asarray(x, np.float64)
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    with wave.open(os.path.join(OUT, name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((x * 32767).astype("<i2").tobytes())
    print("WAV:", name, "%.2fs" % (len(x) / SR))


def t_axis(sec):
    return np.arange(int(SR * sec)) / SR


def periodic_noise(n, lo, hi, tilt=0.0):
    """Bandbegrenztes, periodisches Rauschen der Laenge n."""
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / SR)
    band = ((f > lo) & (f < hi)).astype(float)
    shape = band * (np.maximum(f, 1) / 1000.0) ** tilt
    x = np.fft.irfft(spec * shape, n)
    return x / np.max(np.abs(x))


def env(n, attack, release):
    e = np.ones(n)
    a, r = int(attack * SR), int(release * SR)
    if a:
        e[:a] = np.linspace(0, 1, a)
    if r:
        e[-r:] *= np.linspace(1, 0, r) ** 2
    return e


def snap(f, loop_sec):
    """Frequenz auf ganze Perioden im Loop runden."""
    return round(f * loop_sec) / loop_sec


def motor_loop():
    """Tonales Motorsurren bei mittlerer Drehzahl. Code skaliert Pitch ~0.5..2.0."""
    L = 2.0
    t = t_axis(L)
    x = np.zeros_like(t)
    f0 = snap(310.0, L)  # elektrische Grundfrequenz (Kommutierung)
    for h, amp in ((1, 1.0), (2, 0.55), (3, 0.35), (4, 0.18), (6, 0.12), (12, 0.05)):
        # leichte Schwebung der vier Motoren
        for detune in (-1, 0, 1, 2):
            fd = snap(f0 * h + detune * 0.5 * h, L)
            x += amp * 0.25 * np.sin(2 * np.pi * fd * t + rng.random() * 6.28)
    blade = snap(f0 / 7 * 3, L)  # Blattpassierfrequenz (Tri-Blade)
    x *= 1 + 0.25 * np.sin(2 * np.pi * blade * t)
    x += 0.35 * periodic_noise(len(t), 1500, 9000, tilt=-0.5)
    write("SW_Motor_Loop.wav", x, 0.8)


def propwash_loop():
    """Breitbandiges Luftrauschen; Lautstaerke an Schub koppeln."""
    L = 3.0
    n = int(SR * L)
    x = periodic_noise(n, 80, 6000, tilt=-1.0)
    t = t_axis(L)
    x *= 1 + 0.3 * np.sin(2 * np.pi * snap(2.3, L) * t) * np.sin(2 * np.pi * snap(0.67, L) * t)
    write("SW_PropWash_Loop.wav", x, 0.7)


def beep(freq, dur, vol=1.0, square=True):
    t = t_axis(dur)
    s = np.sign(np.sin(2 * np.pi * freq * t)) if square else np.sin(2 * np.pi * freq * t)
    return s * vol * env(len(t), 0.004, 0.02) * 0.6


def silence(d):
    return np.zeros(int(SR * d))


def seq(*parts):
    return np.concatenate(parts)


def esc_beeps():
    # Motor-Piepser klingen durch die Wicklung etwas weicher: Rechteck + Tiefpass
    def soft(x):
        k = np.ones(6) / 6
        return np.convolve(x, k, "same")

    write("SW_ESC_Startup.wav", soft(seq(beep(1046, 0.16), beep(1318, 0.16), beep(1568, 0.22),
                                         silence(0.35), beep(1046, 0.25), silence(0.12),
                                         beep(1046, 0.25))))
    write("SW_Arm.wav", soft(seq(beep(880, 0.09), silence(0.04), beep(1175, 0.09), silence(0.04),
                                 beep(1568, 0.16))))
    write("SW_Disarm.wav", soft(seq(beep(1568, 0.09), silence(0.04), beep(1046, 0.2))))
    write("SW_ModeSwitch.wav", soft(seq(beep(1318, 0.05), silence(0.03), beep(1318, 0.05))), 0.6)
    write("SW_Beep_Warn.wav", seq(*[seq(beep(3200, 0.12, square=False), silence(0.08)) for _ in range(3)]))
    # Buzzer fuer "Drohne verloren" (Loop, 1 s)
    write("SW_Beep_Lost_Loop.wav", seq(beep(3600, 0.25, square=False), silence(0.75)))


def crash():
    d = 1.2
    t = t_axis(d)
    n = len(t)
    # dumpfer Aufprall
    thump = np.sin(2 * np.pi * (90 * np.exp(-t * 8) + 40) * t) * np.exp(-t * 14)
    # Knacken von Carbon/Plastik
    crack = periodic_noise(n, 1500, 12000) * np.exp(-t * 40)
    # Propeller schlagen auf: unregelmaessige Klicks, schneller werdende Abstaende
    clicks = np.zeros(n)
    pos = 0.02
    gap = 0.012
    while pos < 0.8:
        i = int(pos * SR)
        L = min(300, n - i)
        clicks[i:i + L] += rng.uniform(0.3, 1.0) * periodic_noise(L, 800, 8000) * np.exp(-np.arange(L) / 40)
        pos += gap * rng.uniform(0.7, 1.4)
        gap *= 1.09
    clicks *= np.exp(-t * 3)
    # auslaufender Motor
    spin = np.sin(2 * np.pi * np.cumsum(600 * np.exp(-t * 4)) / SR) * 0.15 * np.exp(-t * 3)
    write("SW_Crash.wav", thump * 1.0 + crack * 0.6 + clicks * 0.5 + spin)


def prop_strike():
    d = 0.25
    t = t_axis(d)
    n = len(t)
    x = np.zeros(n)
    for k in range(6):
        i = int((0.01 + k * 0.018) * SR)
        L = min(250, n - i)
        x[i:i + L] += periodic_noise(L, 1000, 10000) * np.exp(-np.arange(L) / 35) * (1 - k / 7)
    write("SW_PropStrike.wav", x)


def static_loop():
    """Analoges Videorauschen in der Brille bei schlechtem Signal."""
    L = 2.0
    n = int(SR * L)
    hiss = periodic_noise(n, 2000, 16000, tilt=0.3)
    t = t_axis(L)
    hum = 0.15 * np.sin(2 * np.pi * snap(15734 / 4, L) * t)  # Zeilenfrequenz-Pfeifen
    write("SW_VideoStatic_Loop.wav", hiss + hum, 0.6)


def turtle():
    """Turtle-Mode: kurzes, ruckartiges Hochdrehen eines Motors."""
    d = 0.5
    t = t_axis(d)
    f = 120 + 500 * np.clip(t / 0.15, 0, 1)
    x = np.sin(2 * np.pi * np.cumsum(f) / SR) + 0.3 * np.sin(4 * np.pi * np.cumsum(f) / SR)
    x *= env(len(t), 0.01, 0.2)
    write("SW_TurtleFlip.wav", x, 0.7)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    motor_loop()
    propwash_loop()
    esc_beeps()
    crash()
    prop_strike()
    static_loop()
    turtle()
