# maybe later

## metronome sound controls in the tempo page
- tempo page, white knob (4) is already metronome level.
- shift + knob 2 = click pitch, shift + knob 3 = decay, shift + knob 4 = filter.
- needs: the constants in `8d504ffc` become variables the extension owns (or a hook replacing the renderer call at `0x11a02688`), plus shift-knob claims on the tempo page (like bar tools' `bar_menu_m3`), and saving the values (project companion or reboot-scoped like replace mode).
- the click itself: see `metronome-dig-notes.md`.

## toneless click
- truly toneless (filtered noise burst) instead of the short square blip: hook the call at `0x11a02688`, add 64 q31 frames into `0x11b01148`/`0x11b01248`, trigger byte `0x840479f0` (read+clear), accent word `0x08039108` (0 = beat 1), level byte `0x84c69836`.

