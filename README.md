# BAF Toolkit for 3ds Max

[![Download](https://img.shields.io/github/v/release/DennisHerrm/BAF-Toolkit-3ds-max?label=download&color=2ea043)](https://github.com/DennisHerrm/BAF-Toolkit-3ds-max/releases/latest)
[![3ds Max](https://img.shields.io/badge/3ds%20Max-2016%20–%202027-0696D7)](#installing)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

A native 3ds Max plugin that imports and exports the characters of
*Star Wars Episode I: The Phantom Menace* (PC, 1999) — the Big Ape
Actor (`.baf`) models inside the game's `big.lab` archive.

Model, skeleton, textures and animations, in one step — and back into
the game format.

<p align="center">
  <a href="https://github.com/DennisHerrm/BAF-Toolkit-3ds-max/releases/latest"><b>⬇&nbsp; Download the latest version</b></a>
  &nbsp;·&nbsp; <a href="#installing">Installation guide</a>
</p>

---

## What it does

| Area | What you get |
| :--- | :--- |
| **Archive** | Opens `big.lab` directly and lists every model in it — or load a single `.baf` |
| **Meshes** | Geometry with UVs and material IDs |
| **Skeleton** | Full bone hierarchy in its rest pose |
| **Textures** | Palette textures decoded and wired into materials |
| **Animations** | Every clip of the model — one at a time, or all as one sequence |
| **Export** | Writes `.baf` again; geometry and skeleton come from the scene, topology may change |
| **Diagnostics** | Built-in log and data dumps to compare the scene against the file |

The parser is plain C++ without the Max SDK. It was checked against a
Python reference on all **303 models** of `big.lab` — 655,923 dump
lines, 0 differences — and reading and writing every model unchanged
gives back the identical bytes.

## Installing

1. Download **`BafToolkit-<version>-Setup.exe`** from the
   [releases page](https://github.com/DennisHerrm/BAF-Toolkit-3ds-max/releases/latest).
2. Close 3ds Max and run the setup. It asks for administrator rights
   and installs the plugin for **all users** into

       C:\ProgramData\Autodesk\ApplicationPlugins\BafToolkit

   — the folder Autodesk provides for this. One installation covers
   every 3ds Max version from 2016 to 2027. If your Microsoft Visual
   C++ runtime is too old for the plugin, the setup installs the
   current one from Microsoft as well.
3. Start 3ds Max:

       File > Import > Big Ape Actor (*.baf, *.lab)
       File > Export > Big Ape Actor (*.baf)

   Both windows are also listed under *Customize > Customize User
   Interface > Category "DH Tools"* for a toolbar button or shortcut.

Uninstall via *Windows Settings > Apps > BAF Toolkit for 3ds Max*.

**Without the setup:** download `BafToolkit-<version>.zip`, extract it
and run `Install.bat` — it copies the same package to the same folder.

> **Windows SmartScreen:** the installer is not code-signed yet, so
> Windows may show *"Windows protected your PC"*. Click **More info →
> Run anyway**. You can check the download in PowerShell with
> `Get-FileHash .\BafToolkit-<version>-Setup.exe` and compare the value
> with `SHA256SUMS.txt` on the release page.

## Using it

Choose `big.lab` from your game folder in the import dialog, pick a
model from the list, pick a folder for the textures and click
**Build model**. A single `.baf` file works too.

The export window does not need an import in the same session: it
finds where the scene came from and reloads the original as template.
For a model of your own, choose the template — header, palette and
materials come from there, skeleton and geometry from the scene.

No game files are included. You need your own copy of the game.

## Building from source

Requires Visual Studio 2022 or 2026, CMake and the 3ds Max SDKs for the
versions you want to build.

    BAUE_ALLE.bat          all versions with an installed SDK
    BAUE_ALLE.bat 2025     one version
    ERSTELLE_RELEASE.bat   setup, ZIP and checksums into dist\

The build scripts and the developer notes ([BAUEN.md](BAUEN.md),
[DEVNOTES.md](DEVNOTES.md)) are in German.

## Credits

The format knowledge builds on the Blender add-on `io_scene_baf` from
the [OpenPhantom](https://github.com/OpenPhantom/OpenPhantom) project.
`tools/pydump.py` uses that add-on's codec as the reference the C++
parser is checked against.

## License

[MIT](LICENSE) © 2026 DennisH

Unofficial fan project. Not affiliated with or endorsed by Lucasfilm,
Disney, LucasArts, Big Ape Productions or Autodesk. Star Wars is a
trademark of its respective owners.
