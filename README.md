# Bare Frontier

A native SFSE plugin for Starfield that adds a **NUDE: ON/OFF** option directly to the **BODY** tab of Character Creator, including Enhance. Built by **VexenDeimos**.

**Version:** v0.1.7

## Features

- Native-style BODY tab toggle; mouse, keyboard and controller navigation (DualSense tested).
- Removes equipped body clothing from the **temporary Character Creator preview actor only**. Your actual character equipment and saved inventory are not modified.
- NUDE remains enabled while you move between Character Creator tabs, including body-editing tabs.
- NUDE OFF restores the preview outfit. The temporary preview state is discarded when the editor closes; the actual player retains their equipment.
- Optional **NudeOnByDefault** INI setting to automatically remove preview clothing as soon as the preview is ready, without first visiting BODY.
- Fast preview-readiness checking minimizes any clothing flash when the editor opens.
- Works with vanilla Character Creator and CharGenMenu. Does not require an ESP/ESM/ESL, Papyrus scripts or a replacement SWF.

## Requirements

- A compatible version of **Starfield** and **Starfield Script Extender (SFSE)**.
- **Address Library for SFSE Plugins**, for native-plugin address resolution.
- Optional: **CharGenMenu** (the mod also works with vanilla Character Creator).
- Optional: a compatible nude body replacer. **Bare Frontier includes no nude meshes or textures.**

## Installation

Install **BareFrontier_v0.1.7.zip** using Vortex or Mod Organizer 2, or manually copy `SFSE/Plugins/BareFrontier.dll` and `SFSE/Plugins/BareFrontier.ini` into `Starfield/Data/SFSE/Plugins/`.

## Optional configuration

Edit `Starfield/Data/SFSE/Plugins/BareFrontier.ini`:

```ini
[General]
NudeOnByDefault=0
```

- `0` (the shipped default): begin with clothing and `NUDE: OFF`, as in v0.1.6.
- `1`: automatically remove the preview outfit when Character Creator is ready, without navigating to BODY. Manual NUDE ON/OFF still works.

The setting is read when Character Creator opens. Close and reopen Character Creator after editing the file.

If a preview is already undressed (for example, in some New Game Plus flows), the mod has no removable outfit to act on; it will not force a change.

## Safety and compatibility

Only a verified **separate preview actor** can be modified. Bare Frontier does not undress the actual player in gameplay and does not edit saved inventory. It intentionally refuses operations it cannot safely verify. On Character Creator close, Starfield may have already disposed of the preview actor before the plugin's final restoration attempt; a diagnostic warning can be logged, while the actual player remains unaffected in tested gameplay.

The v0.1.7 RC3 build was tested in-game with a DualSense, repeated editor openings, the default-ON and default-OFF options, tab persistence, and manual ON/OFF restoration. Other controllers and untested third-party body editors have not been independently verified.

## Credits

- **Feature idea:** [gluonman](https://next.nexusmods.com/profile/gluonman) on Nexus Mods suggested the optional automatic NUDE-on-open setting added in v0.1.7.
- [CommonLibSF](https://github.com/libxse/commonlibsf) contributors
- [Starfield Script Extender (SFSE)](https://sfse.silverlock.org/) developers
- [Address Library for SFSE Plugins](https://www.nexusmods.com/starfield/mods/3256) by meh321
- Bethesda Game Studios, creators of Starfield

## Source and license

Source: https://github.com/VexenDeimos/BareFrontier

Licensed under **GPL-3.0**. See `LICENSE` for full terms.
