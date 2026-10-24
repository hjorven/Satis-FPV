# FPVDrone lokal bauen (Windows-Laptop)

Diese Anleitung fuehrt von einem leeren Windows-Rechner bis zur fertigen Mod im Spiel. Die exakten Versionsnummern (Visual Studio-Komponenten, Wwise, Engine) aendern sich mit jedem Satisfactory-Update. Massgeblich ist immer die offizielle Anleitung: https://docs.ficsit.app -> *Development* -> *Getting Started*. Wo hier eine Version steht, im Zweifel dort nachsehen.

## 0. Passt der Laptop?

| Was | Minimum | Empfohlen |
|---|---|---|
| System | Windows 10/11 64 Bit | Windows 11 |
| Freier Speicher | ca. 150 GB (Engine, Starter Project, Build-Dateien) | SSD mit 200 GB frei |
| RAM | 16 GB | 32 GB |
| CPU | 4 Kerne | 8+ Kerne (der erste Build dauert sonst mehrere Stunden) |

Linux/macOS werden fuer das Modding nicht unterstuetzt.

## 1. Konten verknuepfen (einmalig, Wartezeit moeglich)

Die angepasste Unreal Engine von Coffee Stain liegt in einem privaten GitHub-Repo, das nur Mitglieder der Epic-Games-Organisation sehen.

1. Epic-Games-Konto anlegen bzw. einloggen.
2. Unter https://www.epicgames.com/account/connections das GitHub-Konto verbinden.
3. Die Einladungs-Mail von GitHub zur Organisation **EpicGames** annehmen (kommt manchmal erst nach einigen Minuten).

## 2. Software installieren

1. **Satisfactory** (Steam oder Epic) installieren und einmal starten.
2. **Satisfactory Mod Manager** (https://smm.ficsit.app) installieren. Damit spaeter SML ins Spiel kommt.
3. **Git for Windows**: https://git-scm.com
4. **Visual Studio 2022 Community**: https://visualstudio.microsoft.com
   - Workloads: *Desktopentwicklung mit C++* und *Spieleentwicklung mit C++*
   - Einzelkomponenten: die in docs.ficsit.app genannte **MSVC v143**-Version, ein **Windows 10/11 SDK**, **.NET Framework 4.8 SDK**
5. **Wwise**: Audiokinetic Launcher (https://www.audiokinetic.com) installieren, darin die in docs.ficsit.app genannte Wwise-Version mit Plattform *Microsoft -> Windows* installieren. Ohne Wwise baut das Starter Project nicht.

## 3. Unreal Engine (Coffee-Stain-Version) installieren

1. Im Browser https://github.com/satisfactorymodding/UnrealEngine/releases oeffnen (geht erst nach Schritt 1).
2. Das neueste Release passend zur Spielversion herunterladen (Installer bzw. alle Archivteile).
3. Installieren. Der Installer registriert die Engine, damit `.uproject`-Dateien sie finden.

## 4. Starter Project (SatisfactoryModLoader) holen

```powershell
cd C:\Modding
git clone https://github.com/satisfactorymodding/SatisfactoryModLoader.git
```

Danach mit dem Wwise Launcher: *Unreal Engine* -> *Integrate Wwise into Project* -> `C:\Modding\SatisfactoryModLoader\FactoryGame.uproject` waehlen (Version wie in docs.ficsit.app).

## 5. Diese Mod einhaengen

```powershell
cd C:\Modding\SatisfactoryModLoader\Mods
git clone https://github.com/hjorven/Satis-FPV.git Satis-FPV
cd Satis-FPV
git checkout claude/project-thread-7blv7j
cd ..
mklink /J FPVDrone Satis-FPV\Mods\FPVDrone
```

`mklink` braucht eine **Eingabeaufforderung** (cmd), nicht PowerShell. In PowerShell stattdessen:

```powershell
New-Item -ItemType Junction -Path FPVDrone -Target Satis-FPV\Mods\FPVDrone
```

Sobald PR #2 gemergt ist, reicht `main` statt des Branches.

## 6. Bauen

1. Im Explorer Rechtsklick auf `FactoryGame.uproject` -> **Generate Visual Studio project files**.
2. `FactoryGame.sln` in Visual Studio oeffnen.
3. Oben **Development Editor** und **Win64** waehlen.
4. Im Projektmappen-Explorer `FactoryGame` -> Rechtsklick -> **Erstellen**.

Der erste Build dauert lange (je nach Laptop 30 Minuten bis mehrere Stunden). Fehler aus `FPVDrone` bitte komplett kopieren (Fenster *Ausgabe*, nicht nur *Fehlerliste*) und mir schicken.

## 7. Einmalige Schritte im Editor

Editor starten (`FactoryGame.uproject` doppelklicken oder in Visual Studio `F5`).

1. Content Browser -> *Settings* -> **Show Plugin Content** anhaken.
2. Ordner `FPVDrone Content` oeffnen -> Rechtsklick -> *Blueprint Class* -> *All Classes* -> `FPVGameWorldModule` -> Name `RootGameWorld_FPVDrone`.
3. Blueprint oeffnen, pruefen: **Root Module** angehakt, unter **Schematics** steht `FPVSchematic_Drone`. Speichern.
4. Optional, aber empfohlen: die Pfade in `Source/FPVDrone/Private/Content/FPVContentPaths.h` mit dem Content Browser vergleichen (Asset suchen -> Rechtsklick -> *Copy Reference*).

## 8. Ins Spiel bringen (Alpakit)

1. Im Editor oben **Alpakit** oeffnen.
2. *Satisfactory Game Path* auf den Spielordner setzen, z. B. `C:\Program Files (x86)\Steam\steamapps\common\Satisfactory`.
3. Bei `FPVDrone` **Alpakit Dev** klicken (Ziel Windows). Die Mod wird gebaut und direkt in den Mods-Ordner des Spiels kopiert.
4. Mit dem Satisfactory Mod Manager SML installieren (falls noch nicht da) und das Spiel starten.

Fuer Multiplayer mit Freunden: unter **Alpakit Release** die Targets `Windows`, `WindowsServer` und `LinuxServer` anhaken. Die erzeugte `.zip` muss bei allen Spielern (und auf einem Dedicated Server) installiert sein.

## 9. Erster Test im Spiel

1. AWESOME Shop -> *FPV Drone* kaufen.
2. In der Werkstatt (Equipment Workshop) *FPV Drone* bauen.
3. `F10`: Funke einrichten (Radio-Tab), Rates/Tune pruefen.
4. `K`: Drohne starten, Arm-Schalter aus und wieder an, Gas hoch.

## 10. Wenn etwas schief geht

| Problem | Loesung |
|---|---|
| Release-Seite der Engine zeigt 404 | GitHub-Konto ist noch nicht mit Epic verknuepft oder die Einladung (Schritt 1) wurde nicht angenommen |
| *Generate Visual Studio project files* fehlt im Kontextmenue | Engine-Installer (Schritt 3) erneut ausfuehren |
| Build-Fehler zu Wwise / AkAudio | Wwise-Version passt nicht oder Integration (Schritt 4) fehlt |
| Build-Fehler in `FPVDrone` | Ausgabe kopieren und im Thread posten, ich korrigiere den Code |
| Drohne nicht im Shop | `RootGameWorld_FPVDrone` fehlt oder *Root Module* nicht angehakt (Schritt 7) |
