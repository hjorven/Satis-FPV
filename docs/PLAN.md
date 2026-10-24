# FPVDrone - Plan

FPV-Drohnen-Mod fuer Satisfactory (Unreal Engine 5 + SML). Mod-Referenz: `FPVDrone`.

## Ziel

- Drohne im AWESOME Shop freischalten (FICSIT-Coupons), in der Werkstatt bauen, aus dem Inventar starten.
- Echtes FPV-Flugverhalten: Acro, Angle, Horizon, 3D-Modus, Turtle-Mode, Airmode.
- Betaflight-kompatible Rates (Betaflight, Actual, Quick, KISS) und echte Tuning-Parameter (PID + Feedforward, TPA, Filter, Throttle-Mid/Expo, Motor-Idle).
- Fernbedienung als HID-Joystick (z. B. Radiomaster Pocket mit EdgeTX im USB-Joystick-Modus) inkl. Kanal-Mapping, Kalibrierung und Erkennungs-Assistent. Fallback: Gamepad und Tastatur.
- Multiplayer: jeder Spieler fliegt seine eigene Drohne, alle anderen sehen sie fluessig.

## Architektur

| Teil | Datei(en) | Aufgabe |
|---|---|---|
| Flugsimulation | `Source/FPVDrone/Public/Sim/*` | Reines C++ ohne UE-Abhaengigkeit: Rates, Flight Controller (PID), Mixer, Motoren, Akku, Aerodynamik. Ausserhalb von UE testbar. |
| Drohnen-Actor | `FPVDroneActor` | Replizierter Actor, Kamera, Kollision, Netz-Synchronisierung, Speichern. |
| Pilot-Link | `FPVPilotLink`, `FPVServerManager` | Pro Spieler ein Actor fuer Server-RPCs (Starten, Ein-/Aussteigen, Aufheben). |
| Eingabe | `Input/FPVHIDDevice*`, `Input/FPVInputSystem` | Windows-HID-Leser (eigener Thread), Kanal-Mapping, Kalibrierung, Gamepad/Tastatur. |
| Einstellungen | `FPVSettings` | JSON-Datei unter `Saved/FPVDrone/Settings.json`, Presets, Betaflight-CLI-Import. |
| UI | `UI/SFPVOSD`, `UI/SFPVConfigMenu` | Slate-OSD (Akku, Timer, Modus, Warnungen) und Konfigurationsmenue. |
| Inhalte | `Content/FPVContent*`, `FPVGameWorldModule` | Item, Rezept, Shop-Schematic, Registrierung bei SML. |

## Netzwerk-Modell

- Der Pilot simuliert seine Drohne lokal (keine Eingabeverzoegerung, wichtig fuer FPV).
- Er sendet den Zustand mit 30 Hz per unzuverlaessigem Server-RPC. Der Server prueft Plausibilitaet (Maximalgeschwindigkeit, Sprungweite) und repliziert an alle.
- Andere Clients interpolieren mit 100 ms Puffer und kurzer Extrapolation.
- Ohne Pilot (ausgestiegen, Absturz) simuliert der Server den Fall bis zum Stillstand.

## Ablauf im Spiel

1. AWESOME Shop: Schematic `FPV-Drohne` kaufen (Standard: 10 Coupons).
2. Werkstatt (Equipment Workshop): `FPV-Drohne` bauen.
3. Taste `K`: Drohne vor dir starten und FPV-Ansicht uebernehmen. Erneut `K`: Brille ab (Drohne bleibt, wo sie ist).
4. Taste `L` in der Naehe: Drohne aufheben (Akku wird beim naechsten Start voll).
5. Taste `F10`: Konfigurationsmenue (Rates, Tuning, Funke, Kamera, Import).

## Arbeitspakete

- [x] Plan
- [ ] Flugsimulation + Tests (laufen ohne UE: `tests/run_sim_tests.sh`)
- [ ] Plugin-Grundgeruest (`FPVDrone.uplugin`, `FPVDrone.Build.cs`, Modul)
- [ ] Drohnen-Actor mit Replikation und Kollision
- [ ] HID-Eingabe, Mapping, Kalibrierung
- [ ] Einstellungen + Betaflight-Import + Presets
- [ ] OSD und Konfigurationsmenue
- [ ] Item, Rezept, AWESOME-Shop-Schematic
- [ ] Setup-Anleitung fuer Editor-Schritte

## Grenzen

- In der Cloud gibt es weder Satisfactory noch das SML-Starterprojekt. Der UE-Teil kann hier **nicht kompiliert und nicht im Spiel getestet** werden. Nur die Flugsimulation wird hier mit `g++` gebaut und getestet.
- Einige FactoryGame-Pfade (Item-Klassen, Werkstatt-Komponente, Shop-Kategorie) stammen aus dem Gedaechtnis und muessen im Editor geprueft werden. Sie stehen gesammelt in `Source/FPVDrone/Private/Content/FPVContentPaths.h`.
- Das HID-Lesen ist nur unter Windows implementiert (der Dedicated Server braucht es nicht).
