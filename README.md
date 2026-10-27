# Satis-FPV

FPV-Drohnen-Mod fuer Satisfactory (SML, Mod-Referenz `FPVDrone`).

- Drohne im AWESOME Shop freischalten, in der Werkstatt bauen, mit `K` starten und FPV fliegen
- Acro, Angle, Horizon, 3D-Modus, Turtle-Mode, Airmode
- Betaflight-Rates (Betaflight, Actual, Quick, KISS), PID + Feedforward, TPA, Filter, Throttle-Kurve, Import von `diff all`
- Fernbedienung als USB-HID-Joystick (z. B. Radiomaster Pocket) mit Kalibrierung und Kanal-Erkennung, dazu Gamepad und Tastatur
- LiPo-Akku mit Spannungseinbruch, OSD, Reichweite/Funkverlust mit Failsafe
- Multiplayer: Pilot simuliert lokal, Server prueft und repliziert, andere Spieler interpolieren

| Dokument | Inhalt |
|---|---|
| [docs/PLAN.md](docs/PLAN.md) | Plan und Architektur |
| [docs/BUILD-LOKAL.md](docs/BUILD-LOKAL.md) | Schritt fuer Schritt: was installieren, wie lokal bauen |
| [docs/SETUP.md](docs/SETUP.md) | Build, Editor-Schritte, Bedienung, offene Pruefpunkte |

Flugsimulation ohne Unreal testen:

```bash
./tests/run_sim_tests.sh
```
