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
| Flugsimulation | `Mods/FPVDrone/Source/FPVDrone/*/Sim/*` | Reines C++ ohne UE-Abhaengigkeit: Rates, Flight Controller (PID), Mixer, Motoren, Akku, Aerodynamik. Ausserhalb von UE testbar. |
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
- Laden: Der Server sucht jede Sekunde nach Strom in der Naehe und repliziert `bChargerInRange`. Der Pilot laedt seinen Akku lokal, der Server laesst Ladestand-Anstiege nur in Reichweite und nur bis zur Laderate zu. Ohne Pilot laedt der Server selbst.

## Ablauf im Spiel

1. AWESOME Shop: Schematic `FPV-Drohne` kaufen (Standard: 10 Coupons).
2. Werkstatt (Equipment Workshop): `FPV-Drohne` bauen aus 3 Platinen, 25 Schnelldraht, 2 Rotoren, 10 Gummi.
   Ausserdem die *FPV Radio* (Funke + Brille) als Hand-Werkzeug: 1 Platine, 10 Schnelldraht, 4 Gummi.
3. Funke in die Hand, Linksklick: Drohne vor dir starten und FPV-Ansicht uebernehmen. Erneut Linksklick: Brille ab (Drohne bleibt, wo sie ist). Abkuerzung ohne Funke: `K`.
4. Drohne anschauen und `E`: aufheben. Der Akku behaelt dabei seinen Ladestand. Abkuerzung: `L`.
5. Akku laden wie beim Hoverpack: Im Umkreis von 30 m um einen Strommast, eine Wandsteckdose oder eine Stromleitung mit laufender Stromerzeugung laedt die Drohne von leer auf voll in etwa 20 s, gelandet und im Flug. Flugzeit bei gemischtem Fliegen etwa 4 Minuten (Vollgas 2 bis 3, gemuetlich etwa 6).
6. Taste `F10`: Konfigurationsmenue (Rates, Tuning, Funke, Kamera, Import).

## Arbeitspakete

- [x] Plan
- [x] Flugsimulation + Tests (laufen ohne UE: `tests/run_sim_tests.sh`)
- [x] Plugin-Grundgeruest (`FPVDrone.uplugin`, `FPVDrone.Build.cs`, Modul)
- [x] Drohnen-Actor mit Replikation und Kollision
- [x] HID-Eingabe, Mapping, Kalibrierung
- [x] Einstellungen + Betaflight-Import + Presets
- [x] OSD und Konfigurationsmenue
- [x] Item, Rezept, AWESOME-Shop-Schematic
- [x] Setup-Anleitung fuer Editor-Schritte
- [x] Akku laden am Stromnetz (serverseitig geprueft), Ladestand bleibt beim Aufheben erhalten
- [x] Baukosten ohne Batterie-Bauteil (Mitte des Spiels)
- [x] Funke als Hand-Werkzeug (Linksklick), Aufheben mit `E`
- [ ] Mesh der Funke in der Hand (Blueprint im Editor, siehe SETUP.md)
- [ ] Optional: Lade- und Startplattform als Gebaeude, Ersatzpropeller

## Grenzen

- In der Cloud gibt es weder Satisfactory noch das SML-Starterprojekt. Der UE-Teil kann hier **nicht kompiliert und nicht im Spiel getestet** werden. Nur die Flugsimulation wird hier mit `g++` gebaut und getestet.
- Einige FactoryGame-Pfade (Item-Klassen, Werkstatt-Komponente, Shop-Kategorie) stammen aus dem Gedaechtnis und muessen im Editor geprueft werden. Sie stehen gesammelt in `Source/FPVDrone/Private/Content/FPVContentPaths.h`.
- Das HID-Lesen ist nur unter Windows implementiert (der Dedicated Server braucht es nicht).
