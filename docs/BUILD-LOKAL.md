# FPVDrone lokal bauen (Windows)

Diese Anleitung fuehrt von einem leeren Windows-Rechner bis zur fertigen Mod im Spiel.
Sie ist **erprobt** (UE 5.6.1-CSS, Wwise 2025.1.6.9117, Satisfactory 1.x, Oktober 2026).
Die exakten Versionsnummern koennen sich mit jedem Satisfactory-Update aendern; massgeblich
ist im Zweifel https://docs.ficsit.app -> *Development* -> *Getting Started*.

> **Wichtigste Fallen vorweg** (Details unten):
> - Es gibt **zwei** Konto-Verknuepfungen (Epic **und** FICSIT-Linker) - fehlt die zweite, ist das Engine-Repo 404.
> - Wwise muss **genau 2025.1.6.9117** sein - neuere Versionen lassen die WwisePatches scheitern.
> - Das Mod-Repo gehoert **ausserhalb** des `Mods`-Ordners (nur per Junction einhaengen), sonst findet der Build FPVDrone doppelt.

## 0. Passt der Rechner?

| Was | Minimum | Empfohlen |
|---|---|---|
| System | Windows 10/11 64 Bit | Windows 11 |
| Freier Speicher | ca. 150 GB | SSD mit 200 GB frei |
| RAM | 16 GB | 32 GB |
| CPU | 4 Kerne | 8+ Kerne |

Linux/macOS werden fuers Satisfactory-Modding nicht unterstuetzt.

## 1. Konten verknuepfen (einmalig, Wartezeit moeglich)

Die angepasste Unreal Engine liegt in einem **privaten** GitHub-Repo. Zugriff braucht **zwei** Schritte:

1. **Epic <-> GitHub**: Epic-Konto anlegen/einloggen, unter https://www.epicgames.com/account/connections
   das GitHub-Konto verbinden, dann die GitHub-Einladung zur Organisation **EpicGames** annehmen
   (https://github.com/settings/organizations). Test: https://github.com/EpicGames/UnrealEngine muss sichtbar sein.
2. **FICSIT Unreal Linker**: zusaetzlich https://linker.ficsit.app/link durchlaufen - das gibt Zugriff auf den
   **satisfactorymodding**-Fork. Test: https://github.com/satisfactorymodding/UnrealEngine muss sichtbar sein (kein 404).

> Nur in der EpicGames-Org zu sein reicht **nicht** - der Fork braucht zusaetzlich den FICSIT-Linker.
> Nach dem Annehmen kann die Sichtbarkeit einige Minuten brauchen; auf GitHub einmal ab-/anmelden beschleunigt es.

## 2. Software installieren

1. **Satisfactory** (Steam oder Epic) installieren und einmal starten.
2. **Satisfactory Mod Manager**: https://smm.ficsit.app (installiert spaeter SML ins Spiel).
3. **Git for Windows**: https://git-scm.com
4. **Visual Studio 2022 Community** - nicht 2026! Direktlink: https://aka.ms/vs/17/release/vs_community.exe
   (das `17` = VS 2022). Beim Installer auswaehlen:
   - Workloads: *Desktopentwicklung mit C++* und *Spieleentwicklung mit C++*
   - Einzelkomponenten: **`MSVC v143 - VS 2022 C++ x64/x86-Buildtools (v14.38-17.8)`** (genau diese,
     neuere MSVC ab 14.40 verursachen Build-Fehler), ein **Windows 11 SDK**, **.NET 8.0-Runtime**, **.NET Framework 4.8.1 SDK**.
5. **Wwise 2025.1.6.9117** ueber den Audiokinetic Launcher (https://www.audiokinetic.com/download):
   - Reiter **Wwise** -> *Install New Version*. Der Filter steht auf **"Latest"** und zeigt nur die neueste Version ->
     auf **"All"** umstellen, dann **2025.1.6.9117** waehlen.
   - Packages: **SDK (C++)**. Deployment Platforms: **Microsoft -> Windows**. (Authoring optional.)

> **Warum genau 2025.1.6.9117?** Das Starter Project patcht den Wwise-Quellcode (`Mods/WwisePatches`).
> Diese Patches passen nur auf diese Version; neuere (z. B. 2025.1.12) lassen die CSS-Patches scheitern.

## 3. Unreal Engine (Coffee-Stain-Version) installieren

1. https://github.com/satisfactorymodding/UnrealEngine/releases/latest oeffnen (geht erst nach Schritt 1).
2. Aus den Assets **genau diese 4 Dateien** in **denselben Ordner** laden (Namen muessen exakt stimmen):
   - `UnrealEngine-CSS-Editor-Win64.exe`
   - `UnrealEngine-CSS-Editor-Win64-1.bin`
   - `UnrealEngine-CSS-Editor-Win64-2.bin`
   - `UnrealEngine-CSS-Editor-Win64-3.bin`
   (Die `Win64.7z`-Dateien sind nur fuer Legacy-CI - nicht nehmen.)
3. Die `.exe` ausfuehren. Zielordner z. B. `C:\Program Files\Unreal Engine - CSS`. Dauert eine Weile.
4. Der Installer registriert die Engine normalerweise selbst. Falls spaeter der Rechtsklick-Eintrag
   *Generate Visual Studio project files* fehlt, einmal registrieren:
   `"C:\Program Files\Unreal Engine - CSS\Engine\Binaries\Win64\UnrealVersionSelector.exe" /register`

## 4. Starter Project (SatisfactoryModLoader) holen

```powershell
git clone https://github.com/satisfactorymodding/SatisfactoryModLoader.git C:\Projekt\SatisFPV
```

(Der Ordner `C:\Projekt\SatisFPV` muss leer sein oder neu angelegt werden; git legt ihn an.)

## 5. Diese Mod einhaengen - Repo AUSSERHALB von Mods!

> ⚠️ Das Repo enthaelt selbst einen `Mods/FPVDrone`-Ordner. Liegt es im gescannten `Mods`-Ordner des
> Starter Projects, findet der UnrealBuildTool FPVDrone **doppelt** und bricht mit
> `error CS0101/CS0111: already contains a definition for 'FPVDrone'` ab. Darum: Repo ausserhalb ablegen,
> nur eine **Junction** kommt in `Mods`.

```powershell
# Repo ausserhalb des Starter Projects klonen
git clone https://github.com/hjorven/Satis-FPV.git C:\Projekt\Satis-FPV

# Junction im Mods-Ordner des Starter Projects anlegen
New-Item -ItemType Junction -Path C:\Projekt\SatisFPV\Mods\FPVDrone -Target C:\Projekt\Satis-FPV\Mods\FPVDrone
```

In einer **Eingabeaufforderung** (cmd) statt PowerShell:
```bat
mklink /J C:\Projekt\SatisFPV\Mods\FPVDrone C:\Projekt\Satis-FPV\Mods\FPVDrone
```

### Wwise ins Projekt integrieren
Audiokinetic Launcher -> Reiter **Unreal Engine** -> beim Projekt `FactoryGame` auf **Integrate Wwise in Project...**:
- Integration version: Filter **"All"** -> Major **2025.1** -> **2025.1.6.9117**.
- Unreal Engine version: nur **5.7** verfuegbar - ist in Ordnung (das Plugin passt sich beim Kompilieren an die echte 5.6-Engine an).
- Vor dem Bestaetigen pruefen: Wwise-SDK-Pfad zeigt `...Wwise_2025.1.6.9117\SDK` (mit **9117**).

> Die WwisePatches werden beim Bauen automatisch als Pre-Build-Schritt angewandt (`Mods/WwisePatches/applyPatches.ps1`).

## 6. Bauen

**Variante A - Kommandozeile** (schnell, keine IDE noetig):
```powershell
& "C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\Build.bat" FactoryEditor Win64 Development -project="C:\Projekt\SatisFPV\FactoryGame.uproject" -waitmutex
```

**Variante B - Visual Studio**:
1. Rechtsklick auf `C:\Projekt\SatisFPV\FactoryGame.uproject` -> **Generate Visual Studio project files**.
2. `FactoryGame.sln` oeffnen, oben **Development Editor** + **Win64** waehlen.
3. Im Projektmappen-Explorer `FactoryGame` -> Rechtsklick -> **Erstellen**.

Der erste Build dauert lange (CSS-Engine ist vorkompiliert, trotzdem 15 Min bis mehrere Stunden), weitere Builds laufen inkrementell in Sekunden.

## 7. Einmalige Schritte im Editor

Das **Root-Modul-Blueprint** `RootGameWorld_FPVDrone` ist im Repo bereits enthalten
(`Mods/FPVDrone/Content/RootGameWorld_FPVDrone.uasset`). Nach einem frischen Clone ist nichts weiter noetig.

Falls es neu angelegt werden muss (Blueprint-Assets lassen sich nicht aus C++ erzeugen):
1. Content Browser -> *Settings* -> **Show Plugin Content** anhaken.
2. Ordner **FPVDrone Content** -> Rechtsklick -> *Blueprint Class* -> *All Classes* -> `FPVGameWorldModule` -> Name **`RootGameWorld_FPVDrone`** -> speichern.
   `bRootModule` und das Schematic `FPVSchematic_Drone` sind bereits im C++-Code gesetzt und werden geerbt - nichts weiter ankreuzen.

Beim Speichern zeigt der Message Log viele Warnungen zu `Content\FactoryGame\...` (Grundspiel-Widgets:
`Event Reply should not be empty`, `ExposeOnSpawn ambiguity`). Das ist **normales** SML-Editor-Grundrauschen und betrifft die Mod nicht.

## 8. Ins Spiel bringen (Alpakit)

- **Alpakit Dev** (lokal testen): Alpakit-Dev-Fenster, bei der Windows-Zielkonfiguration *Copy to Game Path* auf den
  Spielordner setzen (z. B. `C:\Program Files (x86)\Steam\steamapps\common\Satisfactory`), dann **Alpakit!** bei `FPVDrone`.
  Baut die Mod (Shipping) und kopiert sie in den Mods-Ordner des Spiels.
- **Alpakit Release** (verteilen/Multiplayer): Targets `Windows`, `WindowsServer`, `LinuxServer` anhaken, **Alpakit!** -> erzeugt eine
  `.zip` unter `<StarterProject>\Saved\Alpakit\`. Die muss bei allen Spielern und auf dem Dedicated Server installiert sein.

Danach mit dem Satisfactory Mod Manager **SML** installieren (falls noch nicht da) und das Spiel starten.

## 9. Erster Test im Spiel

1. AWESOME Shop -> *FPV Drone* kaufen.
2. In der Werkstatt (Equipment Workshop) *FPV Drone* bauen.
3. `F10`: Funke einrichten (Radio-Tab), Rates/Tune pruefen.
4. `K`: Drohne starten, Arm-Schalter aus und wieder an, Gas hoch.

Bedienung im Detail: siehe [SETUP.md](SETUP.md).

## 10. Wenn etwas schief geht

| Problem | Loesung |
|---|---|
| Engine-Release zeigt **404** (`EpicGames/UnrealEngine` sichtbar, Fork nicht) | FICSIT-Linker (Schritt 1.2) fehlt - https://linker.ficsit.app/link durchlaufen, ggf. kurz warten / GitHub neu einloggen |
| *Generate Visual Studio project files* fehlt im Kontextmenue | Engine registrieren: `UnrealVersionSelector.exe /register` (Schritt 3.4) |
| `error CS0101/CS0111: already contains a definition for 'FPVDrone'` | Mod-Repo liegt im `Mods`-Ordner. Repo nach aussen verschieben, nur Junction in `Mods` (Schritt 5) |
| WwisePatches / Patch-Dry-Run scheitert (`AkWaapiClient.cpp.rej` o. ae.) | Falsche Wwise-Version. Genau **2025.1.6.9117** installieren und integrieren (Schritt 2/5) |
| Build-Fehler zu Wwise / AkAudio beim Kompilieren | Wwise-Integration fehlt oder falsche Version |
| Editor-Meldung *GeneratedSoundBanks folder does not seem to be set* | Mit **No** wegklicken - der Patch `ignore_missing_soundbanks` faengt fehlende Soundbaenke ab |
| Drohne nicht im Shop | `RootGameWorld_FPVDrone` fehlt (Schritt 7) |

## Anhang: ohne Editor testbar

Die Flugphysik/Flight-Controller laesst sich ohne Unreal mit `g++` bauen und testen:

```bash
./tests/run_sim_tests.sh
```
