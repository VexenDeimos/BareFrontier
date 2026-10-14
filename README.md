# Bare Frontier

**Bare Frontier** is a native SFSE plugin for **Starfield** by **VexenDeimos**. It adds a **NUDE: ON/OFF** toggle to the character creator's **BODY** tab without replacing the game's interface files.

## Features

- Nude preview toggle in the original character creator and Enhance (`slm 14`).
- Clickable mouse support and Up/Down navigation with keyboard and controller.
- Select NUDE with a controller and use Cross / A or Left / Right to toggle.
- Runs on the **character creation preview actor**, not the player's actual equipped inventory.
- Tracks and restores preview clothing when turned OFF or when leaving the Body tab.
- Compatible with the base character creation interface and [CharGenMenu](https://www.nexusmods.com/starfield/mods/6850) in testing.
- DLL-only runtime deployment. No ESP/ESM/ESL, Papyrus scripts, config file, or custom SWF file.

**Note:** Bare Frontier removes the preview outfit; it does **not** contain nude body meshes or textures. To see a nude body rather than the game's underlying body assets, use a compatible body replacer.

## Status

**Version:** v0.1.5j — tested stable candidate, **not yet a final public release**. The UI toggle and restoration passed repeated in-game tests with a DualSense controller; other controllers need independent testing. Future Starfield and UI-mod updates could require further compatibility testing.

## Requirements

**At runtime:** Starfield, [SFSE](https://sfse.silverlock.org/), and the Address Library setup required by the version of SFSE/CommonLibSF used for the build. Follow current installation guidance for your game version.

**For building:** Windows, Microsoft C++23-compatible MSVC toolchain (Visual Studio), [Git](https://git-scm.com/), [xmake](https://xmake.io/), and internet access once to obtain [CommonLibSF](https://github.com/libxse/commonlibsf).

## Build from source

Clone this repository (or extract the source ZIP), then run PowerShell in its root:

```powershell
.\scripts\Setup-Dependencies.ps1
.\scripts\Build.ps1
```

Build output will be at `build\windows\x64\releasedbg\BareFrontier.dll` by default.

To install the DLL to a specific Starfield installation (only when the game is closed):

```powershell
.\scripts\Build.ps1 -Install -GameRoot 'D:\SteamLibrary\steamapps\common\Starfield'
```

The installer backs up any existing `BareFrontier.dll` beside the original before replacing it.

## Install in game

Put the compiled `BareFrontier.dll` at:

```text
Starfield/Data/SFSE/Plugins/BareFrontier.dll
```

For Vortex or Mod Organizer 2, package the DLL with that same `Data/SFSE/Plugins` layout.

## Usage

Open character creation or use `slm 14` at an Enhance-like editor. Enter BODY and select **NUDE: ON** to remove the preview outfit; **NUDE: OFF** restores it. Clothing is also restored on leaving BODY. Test compatibility with other body/UI mods on a disposable save before relying on it.

## Source and dependencies

- The plugin's own implementation is in `src/main.cpp`.
- The project is built using the upstream [CommonLibSF](https://github.com/libxse/commonlibsf) framework. CommonLibSF is cloned separately into the ignored `lib/commonlibsf` directory; it is not bundled in this repository.
- `xmake.lua` follows the original project template and declares `GPL-3.0`. Before public publication, confirm the intended distribution license and add its corresponding LICENSE file.
- The helper scripts are for building/publishing the source, **not** required by players to run the plugin.

## Publishing

Create a **new empty repository** under `VexenDeimos` at https://github.com/new. Do not initialize it with a README, `.gitignore`, or license because this package already contains the first two. After extracting this ZIP, from the extracted project root run:

```powershell
.\scripts\Publish-To-GitHub.ps1 -RepositoryUrl 'https://github.com/VexenDeimos/BareFrontier.git'
```

Use whichever repository name you created, and choose public/private in GitHub before publishing. The script refuses to overwrite an existing Git checkout and creates an initial `main` branch and commit. Git authentication may prompt in your browser. You can also upload the files through GitHub or use your preferred Git client.
