# FPVDrone - Build, Setup und Bedienung

## 1. Voraussetzungen

- Satisfactory-Modding-Toolchain nach https://docs.ficsit.app (Starter Project `SatisfactoryModLoader`, passende Unreal-Engine-Version von Coffee Stain, Wwise, Visual Studio 2022 mit C++-Workload).
- SML ab Version 3.10.

## 2. Mod ins Starter Project holen

```powershell
# im Starter Project
cd SatisfactoryModLoader\Mods
git clone https://github.com/hjorven/Satis-FPV.git Satis-FPV
mklink /J FPVDrone Satis-FPV\Mods\FPVDrone
```

Danach `FactoryGame.uproject` -> Rechtsklick -> **Generate Visual Studio project files** und in Visual Studio `Development Editor | Win64` bauen.

## 3. Einmalige Schritte im Editor

Diese Schritte brauchen Blueprint-Assets und lassen sich nicht aus C++ erledigen:

1. **Root-Modul**: Im Content-Ordner des Plugins (`/FPVDrone/`) einen Blueprint mit Parent `FPVGameWorldModule` anlegen, Name `RootGameWorld_FPVDrone`. In den Class Defaults pruefen, dass `Root Module` angehakt ist und `FPVSchematic_Drone` unter `Schematics` steht.
2. **Shop-Kategorie (empfohlen)**: Blueprint-Kind von `FPVSchematic_Drone` anlegen (`Schematic_FPVDrone`), dort `Schematic Category` (z. B. die Kategorie fuer Ausruestung im AWESOME Shop), Icon und Kosten einstellen. Dann im Root-Modul `FPVSchematic_Drone` durch `Schematic_FPVDrone` ersetzen.
3. **Assets** aus `Assets/` gemaess `Assets/Docs/ASSETS.md` importieren. Die erwarteten Pfade stehen in `Source/FPVDrone/Private/Content/FPVContentPaths.h`. Fehlt ein Asset, nutzt der Code Platzhalter (Engine-Wuerfel/-Zylinder) bzw. bleibt still.
4. **FactoryGame-Pfade pruefen**: In `FPVContentPaths.h` stehen die Pfade fuer Coupons, Platinen, Schnelldraht, Rotor, Batterie und die Werkstatt. Im Content Browser jeweils Rechtsklick -> *Copy Reference* und vergleichen.

## 4. Packen

Alpakit -> `FPVDrone` -> Targets `Windows`, `WindowsServer`, `LinuxServer` -> **Alpakit!**.
Die Mod muss auf Server und allen Clients installiert sein (`RemoteVersionRange` im `.uplugin`).

## 5. Ohne Editor testbar: die Flugsimulation

```bash
./tests/run_sim_tests.sh
```

Baut die Physik/den Flight Controller mit `g++` und prueft Rates, Hover, Sprungantworten aller Achsen, Angle/Horizon, 3D, Turtle, Akku-Laufzeit, Hoechstgeschwindigkeit und die Presets.

## 6. Bedienung im Spiel

| Taste | Funktion |
|---|---|
| `K` | Drohne aus dem Inventar starten bzw. Brille auf/ab |
| `L` | Drohne in der Naehe (5 m) aufheben, Akku wird beim naechsten Start voll |
| `F10` | Konfigurationsmenue |
| `V` | FPV- / Verfolgerkamera |

Alle Tasten sind im Menue unter *Camera & Game* aenderbar.

**Ablauf**: AWESOME Shop -> *FPV Drone* kaufen -> in der Werkstatt (Equipment Workshop) bauen -> `K`.

### Fernbedienung (Radiomaster Pocket, EdgeTX)

1. Funke per USB anschliessen, im Dialog **USB Joystick (HID)** waehlen.
2. Im Spiel `F10` -> *Radio*: Input source `Auto` oder `HID radio`, Geraet auswaehlen (leer = automatisch).
3. *Start calibration* -> alle Sticks und Schalter in die Endlagen -> Sticks mittig, Gas unten -> *Finish calibration*.
4. Standardbelegung AETR: Kanal 1 Roll, 2 Pitch, 3 Gas, 4 Yaw, 5 Arm, 6 Flugmodus (3-Stufen), 7 3D, 8 Turtle. Bei anderer Belegung pro Kanal **Detect** druecken und Stick/Schalter in die positive Richtung bewegen.

Arming wie bei Betaflight: Arm-Schalter muss nach dem Aufsetzen der Brille einmal aus gewesen sein, Gas unten (im 3D-Modus mittig), Drohne nicht mehr als 25 Grad geneigt (sonst Turtle-Modus nutzen).

### Ohne Funke

- **Gamepad (Mode 2)**: linker Stick Gas/Yaw, rechter Stick Pitch/Roll, `A` Arm, `B` Flugmodus, `Y` 3D, `LB` Turtle.
- **Tastatur**: `W/S` Pitch, `A/D` Roll, `Q/E` Yaw, `Shift`/`Leertaste` Gas hoch, `Strg`/`C` runter, `X` Gas aus, `R` Arm, `M` Modus, `N` 3D, `T` Turtle.

### Betaflight-Tune uebernehmen

Im Betaflight-CLI `diff all` ausfuehren, Ausgabe kopieren, im Menue unter *Presets & Import* **Import from clipboard**. Uebernommen werden Rates (alle Rate-Typen), PID + Feedforward, TPA, Filter-Grenzfrequenzen, Throttle Mid/Expo/Limit, Idle, Angle/Horizon, 3D-Deadband und Airmode.

## 7. Was im Spiel noch verifiziert werden muss

Der UE-Teil wurde ohne Satisfactory-Toolchain geschrieben und ist **nicht kompiliert**. Beim ersten Build bitte besonders auf diese Stellen achten:

- Signaturen aus FactoryGame/SML: `IFGSaveInterface`, `UFGInventoryComponent::HasItems/Remove/AddStack`, `UGameWorldModule::DispatchLifecycleEvent`, die geschuetzten Felder von `UFGSchematic`, `UFGRecipe`, `UFGItemDescriptor`, `UFGUnlockRecipe`.
- Ob `Pawn->DisableInput(PC)` die Spielfigur waehrend des Flugs vollstaendig stilllegt (Satisfactory nutzt Enhanced Input). Falls nicht: zusaetzlich einen leeren Input Mapping Context mit hoher Prioritaet einhaengen.
- Ob die Engine-Basisformen (`/Engine/BasicShapes/...`) im gepackten Spiel vorhanden sind (nur relevant, solange die eigenen Meshes fehlen).
- Native UE-Sounds (Satisfactory nutzt sonst Wwise).
