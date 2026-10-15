# Changelog

## v0.1.7

- Added `BareFrontier.ini` with optional `NudeOnByDefault` setting (`0` is the backwards-compatible default).
- When enabled, NUDE automatically activates as soon as the Character Creator preview outfit is ready, even if BODY is not the active tab.
- Removed the fixed startup delay in favor of faster readiness checks to minimize visible clothing flashes.
- Preserved manual ON/OFF, tab persistence, controller support, and the separate-preview-actor safeguards.

## v0.1.6

- Native BODY-tab NUDE toggle with controller, keyboard and mouse support.
- NUDE state persists across Character Creator tabs; preview outfit is restored on manual OFF.
- Preview-only changes that do not modify the actual player's equipment.
