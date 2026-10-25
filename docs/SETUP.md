# FPVDrone - Build, Setup und Bedienung

## 1. Voraussetzungen

- Satisfactory-Modding-Toolchain: Starter Project `SatisfactoryModLoader`, Coffee-Stain-Unreal-Engine (5.6.1-CSS),
  **Wwise genau 2025.1.6.9117**, Visual Studio 2022 mit C++-Workload (MSVC v14.38-17.8).
- SML ab Version 3.10.

> **Komplette Einrichtung vom leeren Rechner bis zur Mod im Spiel: siehe [BUILD-LOKAL.md](BUILD-LOKAL.md).**
> Dort stehen alle Fallstricke (zwei Konto-Verknuepfungen, exakte Wwise-Version, Repo ausserhalb von `Mods`).

## 2. Mod ins Starter Project holen

> ⚠️ Das Repo **nicht** in `...\Mods\` klonen (sonst `error CS0101: already contains a definition for 'FPVDrone'`).
> Repo ausserhalb ablegen, nur eine Junction kommt in `Mods`:

```powershell
git clone https://github.com/hjorven/Satis-FPV.git C:\Projekt\Satis-FPV
New-Item -ItemType Junction -Path C:\Projekt\SatisFPV\Mods\FPVDrone -Target C:\Projekt\Satis-FPV\Mods\FPVDrone
```

Danach `FactoryGame.uproject` -> Rechtsklick -> **Generate Visual Studio project files** und `Development Editor | Win64` bauen
(Details und Kommandozeilen-Variante in [BUILD-LOKAL.md](BUILD-LOKAL.md)).

## 3. Einmalige Schritte im Editor

Diese Schritte brauchen Blueprint-Assets und lassen sich nicht aus C++ erledigen:

1. **Root-Modul**: Bereits im Repo enthalten (`Mods/FPVDrone/Content/RootGameWorld_FPVDrone.uasset`) - nach einem frischen Clone nichts noetig. Falls neu: Blueprint mit Parent `FPVGameWorldModule` im Content-Ordner `/FPVDrone/` anlegen, Name `RootGameWorld_FPVDrone`, speichern. `Root Module` und `FPVSchematic_Drone` sind im C++-Code gesetzt und werden geerbt.
2. **Shop-Kategorie (empfohlen)**: Blueprint-Kind von `FPVSchematic_Drone` anlegen (`Schematic_FPVDrone`), dort `Schematic Category` (z. B. die Kategorie fuer Ausruestung im AWESOME Shop), Icon und Kosten einstellen. Dann im Root-Modul `FPVSchematic_Drone` durch `Schematic_FPVDrone` ersetzen.
3. **Assets** aus `Assets/` gemaess `Assets/Docs/ASSETS.md` importieren. Die erwarteten Pfade stehen in `Source/FPVDrone/Private/Content/FPVContentPaths.h`. Fehlt ein Asset, nutzt der Code Platzhalter (Engine-Wuerfel/-Zylinder) bzw. bleibt still.
4. **Funke in der Hand (optional, fuer die Optik)**: Blueprint mit Parent `FPVRadioEquipment` unter `/FPVDrone/Equipment/` anlegen, Name `Equip_FPVRadio`. Darin eine `StaticMeshComponent` mit dem Funken-Mesh unter die Root haengen und Position/Rotation in der Hand einstellen. Der Code nimmt das Blueprint automatisch, ohne Blueprint funktioniert die Funke trotzdem, man sieht sie nur nicht. Eigene Icons optional unter `/FPVDrone/Drone/Icons/IconFPVRadio_256` und `_64`, sonst werden die Drohnen-Icons genutzt.
5. **FactoryGame-Pfade pruefen**: In `FPVContentPaths.h` stehen die Pfade fuer Coupons, Platinen, Schnelldraht, Rotor, Gummi und die Werkstatt. Im Content Browser jeweils Rechtsklick -> *Copy Reference* und vergleichen.

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
| Linksklick mit *FPV Radio* in der Hand | Drohne aus dem Inventar starten bzw. Brille auf/ab |
| `E` (Drohne anschauen) | Drohne aufheben, der Ladestand bleibt erhalten |
| `K` | Abkuerzung: wie Linksklick, auch ohne Funke in der Hand |
| `L` | Abkuerzung: naechste Drohne im Umkreis von 5 m aufheben |
| `F10` | Konfigurationsmenue |
| `V` | FPV- / Verfolgerkamera |

Alle Tasten sind im Menue unter *Camera & Game* aenderbar.

**Ablauf**: AWESOME Shop (10 Coupons) -> *FPV Drone* kaufen -> in der Werkstatt (Equipment Workshop) bauen:

- *FPV Radio* (Hand-Werkzeug): 1 Platine, 10 Schnelldraht, 4 Gummi
- *FPV Drone*: 3 Platinen, 25 Schnelldraht, 2 Rotoren, 10 Gummi

Funke in den Hand-Slot legen, Drohne ins Inventar, Linksklick.

**Akku**: laedt wie das Hoverpack im Umkreis von 30 m um Strommasten, Wandsteckdosen und Stromleitungen eines Netzes mit laufender Stromerzeugung (leer bis voll in etwa 20 s, auch im Flug). Das OSD zeigt dann `CHARGING` und den Ladestand. Flugzeit bei gemischtem Fliegen etwa 4 Minuten. Mehr Flugzeit gibt es im Menue unter *Craft & Battery* (Kapazitaets-Multiplikator) oder mit dem Preset *Beginner*.

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
- Hand-Werkzeug und Benutzen: `AFGEquipment::mEquipmentSlot`, `UFGEquipmentDescriptor::mEquipmentClass`, `AFGCharacterPlayer::GetActiveEquipment(EEquipmentSlot::ES_ARMS)` und die Signaturen von `IFGUseableInterface` (in `FPVDroneActor.h`). Test: Drohne anschauen, es muss "Pick up FPV Drone" erscheinen und `E` muss sie ins Inventar legen. Falls der Linksklick mit der Funke zusaetzlich eine Spielaktion ausloest, bitte melden.
- Strom-Erkennung in `Private/Power/FPVPowerGrid.cpp`: `UFGPowerConnectionComponent::GetPowerCircuit`, `UFGPowerCircuit::IsFuseTriggered/GetStats` (Feld `PowerProductionCapacity`), `AFGBuildableWire::GetConnection`. Test: Drohne neben einen Strommast mit laufendem Generator setzen, das OSD muss `CHARGING` zeigen. Mit ausgeloester Sicherung darf es nicht laden.
