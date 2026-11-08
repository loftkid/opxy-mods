# Testing Step velocity on an OP-XY

Checks to run on a device with a firmware that includes Step velocity, from top to bottom, in one
project. Host tests don't replace these: they can't see the leds or hear the result.

Report a failure with the line and what you saw or heard.

**Firmware.** Needs SDK branch `sdk/step-velocity`. Build it with
`sdk.py build ... --extension extensions/step_velocity`. Last full pass: not yet run.

AI agents working on the extension keep this file up to date: each change that adds, removes or alters
something a person would check updates its lines here.

## Setup

- New project **qa** (not one you care about). Track 1 with a sound; track 2 with a sound.
- Track 1, several pages (4 bars): a single note on step 1 and a chord (3 notes) on step 3 of bar 1, a
  note on step 5 of bar 2; step 2 empty.
- An aux track exists (select it for line 7).

## Checks

- [ ] 1. Hold step 1, press track buttons 1 to 8 in turn: the velocity changes each time (check in
      stock's note view or by ear); the led follows, pink. Does red + white read as pink? If not, tune
      `pink` in `policy.hpp`.
- [ ] 1b. Each value lights only its button, pink. A step tapped fresh (velocity 100) lights button 6.
      No other track light (selected track, red or white) shows while a step is held.
- [ ] 2. Chord step (step 3): a button changes all three notes.
- [ ] 3. Release the step afterwards: no note added or removed (also on a button press that changed
      nothing, and on an empty step).
- [ ] 4. The selected track is unchanged after the combo (track 1 stays selected, track 2 too).
- [ ] 5. One undo restores the old velocities. Pressing the same button again adds no undo.
- [ ] 6. Pages 2 to 4 (step 5 of bar 2) and a non-1x scale: the right step changes.
- [ ] 7. Aux track selected: hold a step, press a track button: nothing changes, the track does not
      switch, the leds stay dark.
- [ ] 8. Tap an empty step: a note at 100, as stock.
- [ ] 9. Keyboard key while holding a step: played velocity, as stock.
- [ ] 10. A velocity step component on the step still wins at playback.
- [ ] 11. Bar tools and capture still work (build them in too).
- [ ] 12. No step held: the track buttons select tracks as stock, and their leds are stock's.
- [ ] 13. Release the step: stock's lights (selected track etc.) come back.
- [ ] 14. Pink party: hold the empty step 2, press track button 1, then 8 (and the other order): every
      key led on the keyboard turns pink and stays, the step velocity leds are hidden. Repeat the
      gesture: back to stock's lights. No note is added or removed on release. Not toggled on a
      step with a note (step 1), with only one of the two buttons, with no step held, or on an aux
      track. After a reboot it is off.
