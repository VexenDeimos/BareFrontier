# BareFrontier

Native Starfield character-creator preview toggle project.

## v0.1.0 development probe

This first build does NOT unequip anything yet.

It verifies:
- SFSE/CommonLibSF plugin loading
- MenuOpenCloseEvent registration
- exact ChargenMenu open/close events
- PlayerCharacter::IsInChargen() state

Once that is confirmed in-game, the next build will add the reversible
preview-outfit suppression layer before any final UI work is added.

## Release status
**Version:** v0.1.6 - final release. In-game testing completed using DualSense; other controllers require independent verification.


## Tab behavior
NUDE remains ON across Character Creator tabs. Switching tabs does not re-equip the preview outfit. Toggle NUDE OFF or close Character Creator to restore it.
